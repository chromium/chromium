// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/default_browser/visual_guided_setter_layout_utils.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <limits>
#include <optional>

#include "base/i18n/rtl.h"
#include "base/numerics/ranges.h"
#include "base/numerics/safe_conversions.h"
#include "ui/base/win/hwnd_metrics.h"
#include "ui/display/screen.h"
#include "ui/display/win/screen_win.h"
#include "ui/display/win/uwp_text_scale_factor.h"
#include "ui/gfx/animation/tween.h"

namespace visual_guided_setter {

namespace {

// Layout constants in DIPs. These values were determined based on the visual
// alignment with the native Windows Settings app.
// LINT.IfChange(docked_settings_geometry)
constexpr int kHorizontalInsetDip = 61;
constexpr int kPreferredHeightDip = 220;
constexpr int kMinHeightDip = 180;
// LINT.ThenChange(//chrome/browser/resources/default_browser/visual_guided_setter/visual_guided_setter.css:docked_settings_slot)

// The client width below which the "Set default" button wraps for the widest
// channel name, so how far the window has to open does not depend on which
// channel is running. It is the measured boundary with no margin added, and it
// was measured at text scale 1.0 in one locale on one machine. A locale that
// falls back to a wider font can still wrap at this width. kLayoutScaleTable's
// wrap width is for the product name as it ships and is narrower; the
// static_assert after the table keeps the two in step.
constexpr int kNoWrapClientWidthDip = 410;

// The left and right window borders of the Settings window together.
constexpr int kHorizontalFrameDip = 16;

// Minimum dimensions of the WebUI docking area bounding box required
// to dock the Settings window. If the docking area is smaller than this, we
// degrade the flow to floating.
constexpr int kMinAnchorWidthDip =
    kNoWrapClientWidthDip + kHorizontalFrameDip + 2 * kHorizontalInsetDip;
constexpr int kMinAnchorHeightDip = 160;

// The client width at which the Settings app collapses its left-hand
// navigation pane. Above this width, the navigation pane occupies 300 DIP,
// shifting the main content and changing the vertical row center.
constexpr int kPaneCollapseClientWidthDip = 800;

// If the docked window lands within this band, docking is declined because
// slight window sizing variance could flip the navigation pane state and cause
// the arrow to point at the wrong row.
constexpr int kBreakpointMarginDip = 20;

// Layout parameters for the Windows 11 Settings "Default apps" page across
// different Windows text scale factors.
struct LayoutScaleEntry {
  float text_scale;
  int wrap_client_width_dip;  // Below this, button wraps below text (x=43).
  int subtitle_wrap_client_width_dip;  // Below this, subtitle wraps onto 2
                                       // lines.
  int button_center_wrapped_dip;       // Center when subtitle wraps (2 lines).
  int button_center_narrow_dip;  // Center when narrow collapsed (< 800 DIP).
  int button_center_wide_dip;    // Center when wide pane (>= 800 DIP).
};

// Rounds to the nearest int, halves up, which is what std::round() does for the
// values it is given here. std::round() itself cannot be used since every call
// is made in `kLayoutScaleTable` construction, which is constexpr.
constexpr int RoundToInt(float value) {
  CHECK(value >= 0.5f && value < static_cast<float>(1 << 23));
  return static_cast<int>(value + 0.5f);
}

// Each layout parameter below is a smooth function of the text scale, fitted
// to the page measured at the scales in kLayoutScaleTable. That table is
// generated from these functions, so the two cannot drift apart; between those
// scales InterpolateLayoutEntry() interpolates the table rather than evaluating
// the fits, which are only known to hold where they were fitted.

// Below this client width the "Set default" button stops sitting beside the
// name Chrome is registered under and stacks underneath it.
constexpr int WrapClientWidthDip(float text_scale) {
  return RoundToInt(376.0f + 112.0f * (text_scale - 1.0f));
}

// Below this client width the subtitle under that name wraps onto a second
// line, which moves the button down further. The fit is to the rows at text
// scale 1.25 and above. At 1.0 the subtitle still fits on one line at every
// width that keeps the button beside it, so that row is the button's own wrap
// width; between the two rows the table is interpolated, so which of the two
// this returns there never matters.
constexpr int SubtitleWrapClientWidthDip(float text_scale) {
  if (text_scale < 1.25f) {
    return WrapClientWidthDip(text_scale);
  }
  // Never below the button's wrap width: the wrapped-subtitle layout is only
  // reachable while the button is still beside the subtitle.
  return std::max(WrapClientWidthDip(text_scale),
                  RoundToInt(556.0f + 224.0f * (text_scale - 1.25f)));
}

constexpr int ButtonCenterWrappedDip(float text_scale) {
  return RoundToInt(164.0f + 32.0f * (text_scale - 1.0f));
}

constexpr int ButtonCenterNarrowDip(float text_scale) {
  return RoundToInt(164.0f + 22.0f * (text_scale - 1.0f));
}

constexpr int ButtonCenterWideDip(float text_scale) {
  const float ds = text_scale - 1.0f;
  return RoundToInt(175.0f + 21.0f * ds + 32.0f * ds * ds);
}

constexpr LayoutScaleEntry MakeLayoutScaleEntry(float text_scale) {
  return LayoutScaleEntry{
      text_scale,
      WrapClientWidthDip(text_scale),
      SubtitleWrapClientWidthDip(text_scale),
      ButtonCenterWrappedDip(text_scale),
      ButtonCenterNarrowDip(text_scale),
      ButtonCenterWideDip(text_scale),
  };
}

// The text scales the page was measured at. The values the fits above produce
// for them are what was measured;
// VisualGuidedSetterLayoutUtilsTest.FittedLayoutMatchesMeasuredLayout holds
// them to that.
constexpr auto kLayoutScaleTable = std::to_array({
    MakeLayoutScaleEntry(1.00f),
    MakeLayoutScaleEntry(1.25f),
    MakeLayoutScaleEntry(1.50f),
    MakeLayoutScaleEntry(1.58f),
    MakeLayoutScaleEntry(1.75f),
    MakeLayoutScaleEntry(2.00f),
});

static_assert(
    kNoWrapClientWidthDip >= kLayoutScaleTable[0].wrap_client_width_dip,
    "The widest channel name cannot need less room than the shipping one.");

LayoutScaleEntry InterpolateLayoutEntry(float text_scale) {
  constexpr size_t kTableSize = kLayoutScaleTable.size();
  if (text_scale <= kLayoutScaleTable[0].text_scale) {
    return kLayoutScaleTable[0];
  }

  if (text_scale >= kLayoutScaleTable[kTableSize - 1].text_scale) {
    return kLayoutScaleTable[kTableSize - 1];
  }

  for (size_t i = 0; i < kTableSize - 1; ++i) {
    const auto& low = kLayoutScaleTable[i];
    const auto& high = kLayoutScaleTable[i + 1];
    if (text_scale >= low.text_scale && text_scale <= high.text_scale) {
      const float progress =
          (text_scale - low.text_scale) / (high.text_scale - low.text_scale);
      return LayoutScaleEntry{
          text_scale,
          gfx::Tween::LinearIntValueBetween(progress, low.wrap_client_width_dip,
                                            high.wrap_client_width_dip),
          gfx::Tween::LinearIntValueBetween(
              progress, low.subtitle_wrap_client_width_dip,
              high.subtitle_wrap_client_width_dip),
          gfx::Tween::LinearIntValueBetween(progress,
                                            low.button_center_wrapped_dip,
                                            high.button_center_wrapped_dip),
          gfx::Tween::LinearIntValueBetween(progress,
                                            low.button_center_narrow_dip,
                                            high.button_center_narrow_dip),
          gfx::Tween::LinearIntValueBetween(progress,
                                            low.button_center_wide_dip,
                                            high.button_center_wide_dip),
      };
    }
  }

  return kLayoutScaleTable[0];
}

float GetTextScaleFactor() {
  return display::win::UwpTextScaleFactor::Instance()->GetTextScaleFactor();
}

// The left and right window borders together, in physical pixels. The frame
// scales with DPI, so it is measured rather than assumed. Returns nullopt if
// `hwnd` cannot be measured; the caller then treats the window width as the
// client width, which errs wide by one frame and so only matters within a
// frame's width of the breakpoint.
std::optional<int> GetHorizontalFrameWidthPx(HWND hwnd) {
  if (!hwnd || !::IsWindow(hwnd)) {
    return std::nullopt;
  }
  return 2 * ui::GetFrameThicknessFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
}

float GetMonitorScaleFactor(const gfx::Rect& rect_px) {
  if (!display::Screen::Get()) {
    return 1.0f;
  }
  gfx::Rect dip_rect =
      display::win::GetScreenWin()->ScreenToDIPRect(nullptr, rect_px);
  display::Display display =
      display::Screen::Get()->GetDisplayMatching(dip_rect);
  const float text_scale = display.text_scale_multiplier();
  const float monitor_scale = text_scale > 0.0f
                                  ? display.device_scale_factor() / text_scale
                                  : display.device_scale_factor();
  return monitor_scale > 0.0f ? monitor_scale : 1.0f;
}

int GetClientWidthDip(HWND settings_hwnd, const gfx::Rect& window_rect_px) {
  int client_width_px = window_rect_px.width();
  if (std::optional<int> frame_px = GetHorizontalFrameWidthPx(settings_hwnd)) {
    client_width_px = std::max(0, client_width_px - *frame_px);
  }
  const float monitor_scale = GetMonitorScaleFactor(window_rect_px);
  return base::ClampRound(client_width_px / monitor_scale);
}

int GetButtonCenterFromClientWidthAndTextScale(int client_width_dip,
                                               float text_scale) {
  const LayoutScaleEntry entry = InterpolateLayoutEntry(text_scale);

  if (client_width_dip >= kPaneCollapseClientWidthDip) {
    return entry.button_center_wide_dip;
  }
  if (client_width_dip >= entry.subtitle_wrap_client_width_dip) {
    return entry.button_center_narrow_dip;
  }
  return entry.button_center_wrapped_dip;
}

int GetButtonCenterFromWindowTopDip(HWND settings_hwnd,
                                    const gfx::Rect& window_rect_px) {
  const int client_width_dip = GetClientWidthDip(settings_hwnd, window_rect_px);
  return GetButtonCenterFromClientWidthAndTextScale(client_width_dip,
                                                    GetTextScaleFactor());
}

}  // namespace

bool IsAnchorLargeEnoughForDocking(const gfx::Rect& anchor_rect_dip) {
  return anchor_rect_dip.width() >= kMinAnchorWidthDip &&
         anchor_rect_dip.height() >= kMinAnchorHeightDip;
}

bool IsSettingsLayoutStableForDocking(HWND settings_hwnd,
                                      const gfx::Rect& target_rect_px) {
  const int client_width_dip = GetClientWidthDip(settings_hwnd, target_rect_px);
  const LayoutScaleEntry entry = InterpolateLayoutEntry(GetTextScaleFactor());

  // If the client width is too narrow, the button wraps below text (x = 43).
  if (client_width_dip < entry.wrap_client_width_dip) {
    return false;
  }

  // If the client width is too close to the navigation pane collapse/expand
  // threshold (800 DIP), Settings can unpredictably shift between wide and
  // narrow layout.
  if (client_width_dip >=
          (kPaneCollapseClientWidthDip - kBreakpointMarginDip) &&
      client_width_dip <=
          (kPaneCollapseClientWidthDip + kBreakpointMarginDip)) {
    return false;
  }

  return true;
}

gfx::Rect ComputeDockedSettingsRectFromAnchor(HWND chrome_hwnd,
                                              const gfx::Rect& anchor_rect_dip,
                                              const gfx::Rect& work_area_px,
                                              HWND settings_hwnd) {
  // 1. Convert anchor rect to physical screen pixels.
  gfx::Rect anchor_px = display::win::GetScreenWin()->DIPToScreenRect(
      chrome_hwnd, anchor_rect_dip);

  // 2. Compute target dimensions in physical pixels.
  int target_width_dip =
      std::max(0, anchor_rect_dip.width() - 2 * kHorizontalInsetDip);
  int target_height_dip = kPreferredHeightDip;
  gfx::Size target_size_px = display::win::GetScreenWin()->DIPToScreenSize(
      chrome_hwnd, gfx::Size(target_width_dip, target_height_dip));

  int min_height_px =
      display::win::GetScreenWin()
          ->DIPToScreenSize(chrome_hwnd, gfx::Size(0, kMinHeightDip))
          .height();
  int target_height_px =
      std::clamp(target_size_px.height(), min_height_px,
                 std::max(min_height_px, work_area_px.height()));
  int target_width_px = target_size_px.width();

  // 3. Compute top frame offset (titlebar + borders) so inner client top aligns
  // directly with anchor top (no padding).
  HMONITOR monitor = ::MonitorFromWindow(chrome_hwnd, MONITOR_DEFAULTTONEAREST);
  int top_frame_offset =
      display::win::GetScreenWin()->GetSystemMetricsForMonitor(monitor,
                                                               SM_CYCAPTION) +
      ui::GetResizableFrameThicknessFromMonitorInPixels(monitor,
                                                        /*has_caption=*/true);
  if (settings_hwnd && ::IsWindow(settings_hwnd)) {
    RECT win_rect, client_rect;
    if (::GetWindowRect(settings_hwnd, &win_rect) &&
        ::GetClientRect(settings_hwnd, &client_rect)) {
      POINT pt = {client_rect.left, client_rect.top};
      if (::ClientToScreen(settings_hwnd, &pt)) {
        int measured = pt.y - win_rect.top;
        if (measured > 0) {
          top_frame_offset = measured;
        }
      }
    }
  }

  int target_x_px = anchor_px.x() + (anchor_px.width() - target_width_px) / 2;
  int target_y_px = anchor_px.y() - top_frame_offset;

  gfx::Rect target_px(target_x_px, target_y_px, target_width_px,
                      target_height_px);

  // 4. Clamp the final pixel rect to the work area.
  target_px.AdjustToFit(work_area_px);

  return target_px;
}

gfx::Point ComputeArrowStartPointFromAnchor(const gfx::Rect& anchor_rect) {
  return base::i18n::IsRTL() ? anchor_rect.left_center()
                             : anchor_rect.right_center();
}

gfx::Point ComputeArrowEndPoint(HWND settings_hwnd,
                                const gfx::Rect& target_rect) {
  // Distance from the window's top down to the row the guidance indicates,
  // which depends on window width and Windows text scale.
  const int button_center_dip =
      GetButtonCenterFromWindowTopDip(settings_hwnd, target_rect);
  const float monitor_scale = GetMonitorScaleFactor(target_rect);
  const int button_center_px =
      base::ClampRound(button_center_dip * monitor_scale);

  return gfx::Point(
      base::i18n::IsRTL() ? target_rect.x() : target_rect.right(),
      target_rect.y() + std::min(button_center_px, target_rect.height()));
}

bool IsDpiCompatibleForDocking(HWND chrome_hwnd,
                               const gfx::Rect& target_screen_px) {
  if (!chrome_hwnd || !display::Screen::Get()) {
    return false;
  }

  // Passing a nullptr HWND instructs ScreenWin to evaluate scaling strictly
  // based on the monitor nearest to the physical rect.
  gfx::Rect target_dip =
      display::win::GetScreenWin()->ScreenToDIPRect(nullptr, target_screen_px);

  display::Display target_display =
      display::Screen::Get()->GetDisplayMatching(target_dip);

  float target_scale = target_display.device_scale_factor();
  float chrome_scale =
      display::win::GetScreenWin()->GetScaleFactorForHWND(chrome_hwnd);

  return base::IsApproximatelyEqual(target_scale, chrome_scale,
                                    std::numeric_limits<float>::epsilon());
}

}  // namespace visual_guided_setter
