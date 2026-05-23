#include <windows.h>
#include <cstring>

#include "EuroScopePlugIn.h"

using namespace EuroScopePlugIn;

namespace
{
const int kIndicatorVerticalOffset = 4;
const int kTrackColorSampleRadius = 2;
const char kSModeIndicatorText[] = "S";

bool TryGetRenderedTrackColor(HDC hDC, const POINT &trackPoint, COLORREF *color)
{
    const COLORREF background =
        GetPixel(hDC, trackPoint.x + kTrackColorSampleRadius + 1, trackPoint.y + kTrackColorSampleRadius + 1);

    for (int radius = 0; radius <= kTrackColorSampleRadius; ++radius) {
        for (int y = trackPoint.y - radius; y <= trackPoint.y + radius; ++y) {
            for (int x = trackPoint.x - radius; x <= trackPoint.x + radius; ++x) {
                if (x != trackPoint.x - radius && x != trackPoint.x + radius &&
                    y != trackPoint.y - radius && y != trackPoint.y + radius) {
                    continue;
                }

                const COLORREF sample = GetPixel(hDC, x, y);
                if (sample != CLR_INVALID && sample != GetBkColor(hDC) && sample != background) {
                    *color = sample;
                    return true;
                }
            }
        }
    }

    return false;
}

bool HasModeC(const CRadarTarget &target)
{
    const CRadarTargetPositionData position = target.GetPosition();
    return position.IsValid() && position.GetTransponderC();
}

class SModePlugin;

class SModeScreen : public CRadarScreen
{
public:
    explicit SModeScreen(SModePlugin *plugin)
        : m_plugin(plugin)
    {
    }

    void OnRefresh(HDC hDC, int Phase) override;
    void OnAsrContentToBeClosed(void) override;

private:
    SModePlugin *m_plugin;

    void DrawModeCIndicators(HDC hDC);
    void DrawIndicator(HDC hDC, const CRadarTarget &target);
};

class SModePlugin : public CPlugIn
{
public:
    SModePlugin()
        : CPlugIn(COMPATIBILITY_CODE,
                  "S-Mode Plugin",
                  "1.0.1",
                  "Maghreb vACC",
                  "GPL 3.0")
    {
    }

    CRadarScreen *OnRadarScreenCreated(
        const char * /*sDisplayName*/,
        bool /*NeedRadarContent*/,
        bool /*GeoReferenced*/,
        bool /*CanBeSaved*/,
        bool /*CanBeCreated*/) override
    {
        return new SModeScreen(this);
    }
};

SModePlugin *g_plugin = nullptr;

void SModeScreen::OnRefresh(HDC hDC, int Phase)
{
    if (Phase != REFRESH_PHASE_AFTER_TAGS || m_plugin == nullptr) {
        return;
    }

    DrawModeCIndicators(hDC);
}

void SModeScreen::OnAsrContentToBeClosed(void)
{
    delete this;
}

void SModeScreen::DrawModeCIndicators(HDC hDC)
{
    for (CRadarTarget target = m_plugin->RadarTargetSelectFirst();
         target.IsValid();
         target = m_plugin->RadarTargetSelectNext(target)) {
        DrawIndicator(hDC, target);
    }
}

void SModeScreen::DrawIndicator(HDC hDC, const CRadarTarget &target)
{
    if (!HasModeC(target)) {
        return;
    }

    const CRadarTargetPositionData position = target.GetPosition();
    const POINT trackPoint = ConvertCoordFromPositionToPixel(position.GetPosition());

    COLORREF color = {};
    if (!TryGetRenderedTrackColor(hDC, trackPoint, &color)) {
        color = GetTextColor(hDC);
    }

    HFONT font = CreateFontA(
        -12,
        0,
        0,
        0,
        FW_BOLD,
        FALSE,
        FALSE,
        FALSE,
        ANSI_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_SWISS,
        "./EuroScope.tff");

    HFONT oldFont = font != nullptr ? static_cast<HFONT>(SelectObject(hDC, font)) : nullptr;
    const COLORREF oldTextColor = SetTextColor(hDC, color);
    const int oldBkMode = SetBkMode(hDC, TRANSPARENT);

    SIZE textSize = {};
    GetTextExtentPoint32A(
        hDC,
        kSModeIndicatorText,
        static_cast<int>(std::strlen(kSModeIndicatorText)),
        &textSize);

    TextOutA(
        hDC,
        trackPoint.x - (textSize.cx / 2),
        trackPoint.y + kIndicatorVerticalOffset,
        kSModeIndicatorText,
        static_cast<int>(std::strlen(kSModeIndicatorText)));

    SetBkMode(hDC, oldBkMode);
    SetTextColor(hDC, oldTextColor);
    if (oldFont != nullptr) {
        SelectObject(hDC, oldFont);
    }
    if (font != nullptr) {
        DeleteObject(font);
    }
}
}

void __declspec(dllexport) EuroScopePlugInInit(CPlugIn **ppPlugInInstance)
{
    g_plugin = new SModePlugin();
    *ppPlugInInstance = g_plugin;
}

void __declspec(dllexport) EuroScopePlugInExit(void)
{
    delete g_plugin;
    g_plugin = nullptr;
}
