// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/owner_draw_controls.h"

#include <windows.h>

#include <bitset>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_select_object.h"
#include "chrome/updater/win/ui/message_loop.h"
#include "chrome/updater/win/ui/owner_draw_controls_test_api.h"
#include "chrome/updater/win/ui/progress_wnd.h"
#include "chrome/updater/win/ui/ui_constants.h"
#include "chrome/updater/win/ui/ui_test_util.h"
#include "chrome/updater/win/ui/ui_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace updater::ui {
namespace {

using ::testing::Gt;
using ::testing::Optional;
using ::updater::test::CreateTestDIB24;

// Writes the registry value that `IsDarkModeOn()` reads. Fatal on failure so
// that a broken setup surfaces directly instead of cascading into confusing
// color mismatches.
void SetDarkMode(bool dark) {
  base::win::RegKey key;
  ASSERT_EQ(
      key.Create(
          HKEY_CURRENT_USER,
          L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
          KEY_SET_VALUE),
      ERROR_SUCCESS);
  ASSERT_EQ(
      key.WriteValue(L"AppsUseLightTheme", static_cast<DWORD>(dark ? 0 : 1)),
      ERROR_SUCCESS);
}

// Pre-filled into the test bitmap before every render. No design token and no
// system color is magenta, so any pixel still holding this after a render is a
// region no paint path covered. Without it such a region reads as black, which
// is `COLOR_BTNTEXT` on the default palette, and a missing background would be
// reported as a glyph color mismatch.
constexpr COLORREF kUnpaintedSentinel = RGB(0xFF, 0x00, 0xFF);

// Renders `button` through its owner-draw entry point into a memory DC with
// `item_state` as the DRAWITEMSTRUCT state, then invokes `sample` with the
// memory DC and the control's dimensions. Returns false and adds a failure if
// the GDI setup could not be completed, so a setup problem is not reported as
// a color mismatch.
template <typename SampleFn>
bool DrawItemAndSample(CaptionButton& button,
                       UINT item_state,
                       SampleFn sample) {
  RECT client_rect = {};
  if (!::GetClientRect(button.hwnd(), &client_rect)) {
    ADD_FAILURE() << "::GetClientRect failed for the caption button";
    return false;
  }
  const int width = client_rect.right - client_rect.left;
  const int height = client_rect.bottom - client_rect.top;
  if (width <= 0 || height <= 0) {
    ADD_FAILURE() << "Caption button has an empty client rect: " << width << "x"
                  << height;
    return false;
  }

  base::win::ScopedGetDC screen_dc(nullptr);
  base::win::ScopedCreateDC memory_dc(::CreateCompatibleDC(screen_dc));
  if (!memory_dc.is_valid()) {
    ADD_FAILURE() << "::CreateCompatibleDC failed";
    return false;
  }
  base::win::ScopedGDIObject<HBITMAP> bitmap =
      CreateTestDIB24(memory_dc.get(), width, height);
  if (!bitmap.is_valid()) {
    ADD_FAILURE() << "::CreateDIBSection failed";
    return false;
  }
  // `select_bitmap` is declared after `bitmap` so that the DC's original
  // bitmap is restored before `bitmap` is destroyed: ::DeleteObject() fails
  // for a bitmap that is still selected into a DC, which would leak it.
  base::win::ScopedSelectObject select_bitmap(memory_dc.get(), bitmap.get());

  base::win::ScopedGDIObject<HBRUSH> sentinel_brush(
      ::CreateSolidBrush(kUnpaintedSentinel));
  if (!sentinel_brush.is_valid()) {
    ADD_FAILURE() << "::CreateSolidBrush failed for the sentinel fill";
    return false;
  }
  ::FillRect(memory_dc.get(), &client_rect, sentinel_brush.get());

  DRAWITEMSTRUCT draw_item = {.CtlType = ODT_BUTTON,
                              .itemState = item_state,
                              .hwndItem = button.hwnd(),
                              .hDC = memory_dc.get(),
                              .rcItem = client_rect};
  button.DrawItem(&draw_item);

  // Every pixel must have been covered, either by the hover fill or by the
  // parent's background. `DrawParentBackground()` returns early on several GDI
  // failures, and leaving that undetected would surface later as a confusing
  // color mismatch rather than as the setup problem it is.
  //
  // Two full-rect scans via ::GetPixel() (sentinel verification and color
  // counting) require O(width * height) GDI round-trips, which is acceptable
  // here because caption buttons are small (~30x30).
  int unpainted = 0;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      if (::GetPixel(memory_dc.get(), x, y) == kUnpaintedSentinel) {
        ++unpainted;
      }
    }
  }
  if (unpainted) {
    ADD_FAILURE() << "The control's background was not painted: " << unpainted
                  << " of " << (width * height)
                  << " pixels still hold the sentinel";
    return false;
  }

  sample(memory_dc.get(), width, height);
  return true;
}

// Returns how many pixels of the rendered control are exactly `color`.
//
// The glyph is deliberately located by color rather than by coordinate: its
// position is the result of two independent floor divisions (`DrawItem`'s
// centering and `GetButtonRgn`'s own centering) and its thickness scales with
// DPI, so a fixed sample point silently reads the background at some control
// heights. The minimize bar is only two pixels tall at 96 DPI, which makes
// that failure mode easy to hit and hard to see.
std::optional<int> CountRenderedPixels(CaptionButton& button,
                                       UINT item_state,
                                       COLORREF color) {
  int count = 0;
  if (!DrawItemAndSample(button, item_state,
                         [&](HDC dc, int width, int height) {
                           for (int y = 0; y < height; ++y) {
                             for (int x = 0; x < width; ++x) {
                               if (::GetPixel(dc, x, y) == color) {
                                 ++count;
                               }
                             }
                           }
                         })) {
    // Distinguishable from a real count of zero, so a rendering failure does
    // not also read as a color mismatch.
    return std::nullopt;
  }
  return count;
}

// Where in the control's client rect to sample a single rendered pixel. Used
// for the surfaces whose geometry is unambiguous, unlike the glyph.
enum class SamplePoint {
  // A point inside the control but outside the glyph region, which spans the
  // centered middle 12/31 of the control in both axes.
  kBackground,
  // A point along the control's outer boundary where the focus frame is drawn.
  kFrame,
};

// Returns the pixel at `point` in the rendered control.
COLORREF SampleRenderedPixel(CaptionButton& button,
                             UINT item_state,
                             SamplePoint point) {
  COLORREF color = CLR_INVALID;
  if (!DrawItemAndSample(
          button, item_state, [&](HDC dc, int width, int height) {
            POINT sample = {};
            switch (point) {
              case SamplePoint::kBackground:
                // Just inside the border, clear of the glyph.
                sample = {2, 2};
                break;
              case SamplePoint::kFrame:
                // The focus frame is drawn along the border.
                sample = {0, 0};
                break;
            }
            // Both points are fixed offsets, so a caller that
            // shrinks the control would otherwise read
            // CLR_INVALID and report it as a color mismatch.
            if (sample.x >= width || sample.y >= height) {
              ADD_FAILURE() << "Sample point (" << sample.x << ", " << sample.y
                            << ") is outside the " << width << "x" << height
                            << " control";
              return;
            }
            color = ::GetPixel(dc, sample.x, sample.y);
          })) {
    return CLR_INVALID;
  }
  return color;
}

// Thin wrappers over the test API, so the expectations below read as if they
// were calling the control directly.
COLORREF GlyphColor(const CaptionButton& button) {
  return test::CaptionButtonTestApi(button).ResolveGlyphColor();
}
bool IsDarkMode(const CaptionButton& button) {
  return test::CaptionButtonTestApi(button).is_dark_mode();
}
bool IsMouseHovering(const TrackedButton& button) {
  return test::TrackedButtonTestApi(button).is_mouse_hovering();
}
bool IsTrackingMouseEvents(const TrackedButton& button) {
  return test::TrackedButtonTestApi(button).is_tracking_mouse_events();
}

// The caption button behavior under test never reaches the event sink, so the
// host dialog is given an inert one rather than a mock.
class NoOpProgressWndEvents : public ProgressWndEvents {
 public:
  void DoClose() override {}
  void DoExit() override {}
  bool DoLaunchBrowser(const std::string& url) override { return true; }
  bool DoRestartBrowser(bool restart_all_browsers,
                        const std::vector<GURL>& urls) override {
    return true;
  }
  bool DoReboot() override { return true; }
  void DoCancel() override {}
};

// Exercises the owner-drawn caption buttons. `ProgressWnd` is only the host:
// it is the sole production dialog that installs an `OwnerDrawTitleBar`, and
// the controls need a real parent window to be created and painted.
class CaptionButtonTest : public ::testing::Test {
 protected:
  // Destroyed here rather than in each test body, so an early return from a
  // failed ASSERT_* cannot leak a top-level window.
  void TearDown() override {
    title_bar_.reset();
    if (progress_wnd_) {
      progress_wnd_->DestroyWindow();
      progress_wnd_.reset();
    }
  }

  // Creates the host dialog, optionally configuring `dark` mode.
  void CreateTestDialog(std::optional<bool> dark = std::nullopt) {
    if (dark.has_value()) {
      ASSERT_NO_FATAL_FAILURE(SetDarkMode(*dark));
    }
    progress_wnd_ = std::make_unique<ProgressWnd>(&message_loop_, nullptr);
    progress_wnd_->SetEventSink(&events_);
    // `Initialize()` calls `Create()`, which realizes the dialog and its
    // children. `Show()` is deliberately skipped: a visible, foreground window
    // would let a physical cursor deliver mouse messages that perturb the
    // hover expectations.
    ASSERT_HRESULT_SUCCEEDED(progress_wnd_->Initialize());

    // Belt and braces, in case something makes the window visible: keep it
    // where no real cursor can be.
    ASSERT_TRUE(::SetWindowPos(progress_wnd_->hwnd(), nullptr, -10000, -10000,
                               0, 0,
                               SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE));

    title_bar_ =
        std::make_unique<test::OwnerDrawTitleBarTestApi>(*progress_wnd_);
    ASSERT_TRUE(title_bar_->title_bar_window().IsWindow());
    ASSERT_TRUE(close_button().IsWindow());
    ASSERT_TRUE(minimize_button().IsWindow());
  }

  CloseButton& close_button() { return title_bar_->close_button(); }
  MinimizeButton& minimize_button() { return title_bar_->minimize_button(); }

  MessageLoop message_loop_;
  // Declared before the dialog so that it outlives the sink pointer.
  NoOpProgressWndEvents events_;
  std::unique_ptr<ProgressWnd> progress_wnd_;
  std::unique_ptr<test::OwnerDrawTitleBarTestApi> title_bar_;
};

// The tests that pin a specific light or dark color. `IsDarkModeOn()` reports
// the system window color rather than `AppsUseLightTheme` while high contrast
// is on, and SPI_GETHIGHCONTRAST is not redirected by `registry_override_`, so
// a host that is actually in high contrast cannot be driven into a known
// state. The state machine tests above have no such dependency and still run.
class CaptionButtonThemeTest : public CaptionButtonTest {
 protected:
  void SetUp() override {
    // Chained even though the base currently has no `SetUp()`, so that adding
    // one later does not silently skip it. The override must be installed
    // before any test body calls `CreateTestDialog(dark)`.
    CaptionButtonTest::SetUp();
    if (HasFatalFailure()) {
      return;
    }
    ASSERT_NO_FATAL_FAILURE(
        registry_override_.OverrideRegistry(HKEY_CURRENT_USER));
    if (IsHighContrastOn()) {
      GTEST_SKIP() << "The host is in high contrast mode";
    }
  }

  registry_util::RegistryOverrideManager registry_override_;
};

// Position of `paint_state` in the 16 entry space spanned by the four flags
// `ResolveGlyphColor()` reads. Defined once so that the enumeration below and
// its exhaustiveness check cannot disagree about the encoding.
int StateIndex(const test::CaptionButtonTestApi::PaintState& paint_state) {
  return (paint_state.is_enabled ? 1 : 0) | (paint_state.is_hovered ? 2 : 0) |
         (paint_state.is_dark_mode ? 4 : 0) |
         (paint_state.is_high_contrast ? 8 : 0);
}

// Every state the glyph color is resolved from, enumerated. This is possible
// because `ResolveGlyphColor()` is static and total, so no window, no message
// pump and no particular host theme is needed. The fixtured tests below cover
// the message plumbing that produces these states and the fact that the
// resolved color is what actually reaches the pixels.
TEST(CaptionButtonGlyphColorTest, EveryState) {
  using PaintState = test::CaptionButtonTestApi::PaintState;
  const struct {
    const char* name;
    PaintState paint_state;
    COLORREF expected;
  } kCases[] = {
      // Light mode.
      {"light",
       {.is_enabled = true, .is_hovered = false},
       kCaptionForegroundColor},
      {"light hovered",
       {.is_enabled = true, .is_hovered = true},
       kCaptionForegroundColor},
      {"light disabled",
       {.is_enabled = false, .is_hovered = false},
       kCaptionForegroundColorDisabled},
      {"light disabled hovered",
       {.is_enabled = false, .is_hovered = true},
       kCaptionForegroundColorDisabled},

      // Dark mode.
      {"dark",
       {.is_enabled = true, .is_hovered = false, .is_dark_mode = true},
       kCaptionForegroundColorDark},
      {"dark hovered",
       {.is_enabled = true, .is_hovered = true, .is_dark_mode = true},
       kCaptionForegroundColorDark},
      {"dark disabled",
       {.is_enabled = false, .is_hovered = false, .is_dark_mode = true},
       kCaptionForegroundColorDisabledDark},
      {"dark disabled hovered",
       {.is_enabled = false, .is_hovered = true, .is_dark_mode = true},
       kCaptionForegroundColorDisabledDark},

      // High contrast replaces the tokens with system colors, and is the only
      // mode in which hover changes the glyph. Dark mode must not affect it.
      {"high contrast",
       {.is_enabled = true, .is_hovered = false, .is_high_contrast = true},
       ::GetSysColor(COLOR_BTNTEXT)},
      {"high contrast hovered",
       {.is_enabled = true, .is_hovered = true, .is_high_contrast = true},
       ::GetSysColor(COLOR_HIGHLIGHTTEXT)},
      {"high contrast disabled",
       {.is_enabled = false, .is_hovered = false, .is_high_contrast = true},
       ::GetSysColor(COLOR_GRAYTEXT)},
      // Disabled outranks hover, so this stays COLOR_GRAYTEXT rather than
      // becoming COLOR_HIGHLIGHTTEXT: `paints_hover()` is false while disabled
      // and `DrawItem()` therefore does not paint the COLOR_HIGHLIGHT
      // background that COLOR_HIGHLIGHTTEXT exists to sit on.
      {"high contrast disabled hovered",
       {.is_enabled = false, .is_hovered = true, .is_high_contrast = true},
       ::GetSysColor(COLOR_GRAYTEXT)},
      {"high contrast dark",
       {.is_enabled = true,
        .is_hovered = false,
        .is_dark_mode = true,
        .is_high_contrast = true},
       ::GetSysColor(COLOR_BTNTEXT)},
      {"high contrast dark hovered",
       {.is_enabled = true,
        .is_hovered = true,
        .is_dark_mode = true,
        .is_high_contrast = true},
       ::GetSysColor(COLOR_HIGHLIGHTTEXT)},
      {"high contrast dark disabled",
       {.is_enabled = false,
        .is_hovered = false,
        .is_dark_mode = true,
        .is_high_contrast = true},
       ::GetSysColor(COLOR_GRAYTEXT)},
      {"high contrast dark disabled hovered",
       {.is_enabled = false,
        .is_hovered = true,
        .is_dark_mode = true,
        .is_high_contrast = true},
       ::GetSysColor(COLOR_GRAYTEXT)},
  };
  ASSERT_EQ(std::size(kCases), 16u)
      << "One case per combination of enabled, hover, dark mode and high "
         "contrast";

  // The size check alone would accept a duplicated row paired with a missing
  // one, which would quietly drop a state from a test whose whole premise is
  // that it covers all of them. Together, 16 rows and 16 distinct bit patterns
  // prove the enumeration is exhaustive.
  uint32_t covered = 0;
  for (const auto& test_case : kCases) {
    covered |= 1u << StateIndex(test_case.paint_state);
  }
  ASSERT_EQ(covered, 0xFFFFu)
      << "The rows are not the 16 distinct states. Missing indices are the set "
         "bits of "
      << std::bitset<16>(~covered & 0xFFFFu)
      << " (bit 0 is enabled, 1 hover, 2 dark mode, 3 high contrast)";

  for (const auto& test_case : kCases) {
    SCOPED_TRACE(test_case.name);
    EXPECT_EQ(
        test::CaptionButtonTestApi::ResolveGlyphColor(test_case.paint_state),
        test_case.expected);
  }
}

// The caption buttons report and paint the accent color in light mode.
TEST_F(CaptionButtonThemeTest, LightMode) {
  ASSERT_NO_FATAL_FAILURE(CreateTestDialog(/*dark=*/false));

  EXPECT_FALSE(IsDarkMode(close_button()));
  EXPECT_FALSE(IsDarkMode(minimize_button()));
  EXPECT_EQ(GlyphColor(close_button()), kCaptionForegroundColor);
  EXPECT_EQ(GlyphColor(minimize_button()), kCaptionForegroundColor);

  EXPECT_THAT(CountRenderedPixels(close_button(), /*item_state=*/0,
                                  kCaptionForegroundColor),
              Optional(Gt(0)));
  EXPECT_THAT(CountRenderedPixels(minimize_button(), /*item_state=*/0,
                                  kCaptionForegroundColor),
              Optional(Gt(0)));
}

// Caption buttons created while the system is already in dark mode pick up the
// dark accent color.
TEST_F(CaptionButtonThemeTest, DarkMode) {
  ASSERT_NO_FATAL_FAILURE(CreateTestDialog(/*dark=*/true));

  EXPECT_TRUE(IsDarkMode(close_button()));
  EXPECT_TRUE(IsDarkMode(minimize_button()));
  EXPECT_EQ(GlyphColor(close_button()), kCaptionForegroundColorDark);
  EXPECT_EQ(GlyphColor(minimize_button()), kCaptionForegroundColorDark);

  EXPECT_THAT(CountRenderedPixels(close_button(), /*item_state=*/0,
                                  kCaptionForegroundColorDark),
              Optional(Gt(0)));
  EXPECT_THAT(CountRenderedPixels(minimize_button(), /*item_state=*/0,
                                  kCaptionForegroundColorDark),
              Optional(Gt(0)));
}

// `GlyphColor()` reads the live WS_DISABLED bit, which is the path taken
// outside a draw cycle. The pixel test below covers the other direction, the
// ODS_DISABLED bit the system puts in `DRAWITEMSTRUCT::itemState`.
TEST_F(CaptionButtonThemeTest, DisabledReportsTheDisabledGlyphColor) {
  ASSERT_NO_FATAL_FAILURE(CreateTestDialog(/*dark=*/false));

  // Both caption buttons resolve the glyph the same way, so both are
  // exercised, even though only the close button is disabled in production.
  CaptionButton* const buttons[] = {&close_button(), &minimize_button()};
  for (CaptionButton* button : buttons) {
    ::EnableWindow(button->hwnd(), FALSE);
    EXPECT_EQ(GlyphColor(*button), kCaptionForegroundColorDisabled);
    ::EnableWindow(button->hwnd(), TRUE);
    EXPECT_EQ(GlyphColor(*button), kCaptionForegroundColor);
  }
}

// A disabled button paints the disabled token and never the accent token, so
// it is visually distinguishable from an enabled one.
TEST_F(CaptionButtonThemeTest, DisabledGlyphPixels) {
  ASSERT_NO_FATAL_FAILURE(CreateTestDialog(/*dark=*/false));

  // ODS_DISABLED is passed directly, exactly as the sibling tests do with
  // ODS_FOCUS.
  EXPECT_THAT(CountRenderedPixels(close_button(), ODS_DISABLED,
                                  kCaptionForegroundColorDisabled),
              Optional(Gt(0)));
  EXPECT_THAT(CountRenderedPixels(close_button(), ODS_DISABLED,
                                  kCaptionForegroundColor),
              Optional(0));

  EXPECT_THAT(CountRenderedPixels(minimize_button(), ODS_DISABLED,
                                  kCaptionForegroundColorDisabled),
              Optional(Gt(0)));
  EXPECT_THAT(CountRenderedPixels(minimize_button(), ODS_DISABLED,
                                  kCaptionForegroundColor),
              Optional(0));

  ASSERT_NO_FATAL_FAILURE(SetDarkMode(true));
  ::SendMessage(close_button().hwnd(), WM_THEMECHANGED, 0, 0);
  ::SendMessage(minimize_button().hwnd(), WM_THEMECHANGED, 0, 0);
  // Pin the switch itself: a theme change that did not take effect would
  // otherwise surface below as a zero count for the dark token, which reads as
  // a color bug rather than the setup failure it is.
  ASSERT_TRUE(IsDarkMode(close_button()));
  ASSERT_TRUE(IsDarkMode(minimize_button()));

  EXPECT_THAT(CountRenderedPixels(close_button(), ODS_DISABLED,
                                  kCaptionForegroundColorDisabledDark),
              Optional(Gt(0)));
  EXPECT_THAT(CountRenderedPixels(close_button(), ODS_DISABLED,
                                  kCaptionForegroundColorDark),
              Optional(0));

  EXPECT_THAT(CountRenderedPixels(minimize_button(), ODS_DISABLED,
                                  kCaptionForegroundColorDisabledDark),
              Optional(Gt(0)));
  EXPECT_THAT(CountRenderedPixels(minimize_button(), ODS_DISABLED,
                                  kCaptionForegroundColorDark),
              Optional(0));
}

// A disabled control takes neither the hover background nor the hover glyph,
// even while `is_mouse_hovering_` is set.
//
// `OnEnable()` clears `is_mouse_hovering_` when the control is disabled, so a
// live control does not reach this state, and
// `CaptionButtonTest.DisablingClearsHoverAndCancelsTracking` pins that. But
// `SnapshotPaintState()` mixes the paint time ODS_DISABLED bit with the live
// `is_mouse_hovering_` member, so the two can still disagree within a single
// draw. `paints_hover()` is what keeps that disagreement off the screen.
TEST_F(CaptionButtonThemeTest, DisabledIgnoresStaleHover) {
  ASSERT_NO_FATAL_FAILURE(CreateTestDialog(/*dark=*/false));

  ::SendMessage(close_button().hwnd(), WM_MOUSEMOVE, 0, 0);
  ASSERT_TRUE(IsMouseHovering(close_button()));

  // Asserted as a non match rather than against a named token: the background
  // painted here is the parent's, blitted in by `DrawParentBackground()`.
  EXPECT_NE(SampleRenderedPixel(close_button(), ODS_DISABLED,
                                SamplePoint::kBackground),
            kCaptionBkHover);
  EXPECT_THAT(CountRenderedPixels(close_button(), ODS_DISABLED,
                                  kCaptionForegroundColorDisabled),
              Optional(Gt(0)));
}

// The hover background and the focus frame use their own tokens, neither of
// which is reachable through the reported glyph color.
TEST_F(CaptionButtonThemeTest, HoverAndFocusPixels) {
  ASSERT_NO_FATAL_FAILURE(CreateTestDialog(/*dark=*/false));

  ::SendMessage(close_button().hwnd(), WM_MOUSEMOVE, 0, 0);
  ASSERT_TRUE(IsMouseHovering(close_button()));
  EXPECT_EQ(SampleRenderedPixel(close_button(), /*item_state=*/0,
                                SamplePoint::kBackground),
            kCaptionBkHover);
  EXPECT_EQ(SampleRenderedPixel(close_button(), ODS_FOCUS, SamplePoint::kFrame),
            kCaptionFrameColor);

  ASSERT_NO_FATAL_FAILURE(SetDarkMode(true));
  ::SendMessage(close_button().hwnd(), WM_THEMECHANGED, 0, 0);
  ::SendMessage(close_button().hwnd(), WM_MOUSEMOVE, 0, 0);
  ASSERT_TRUE(IsMouseHovering(close_button()));
  EXPECT_EQ(SampleRenderedPixel(close_button(), /*item_state=*/0,
                                SamplePoint::kBackground),
            kCaptionBkHoverDark);
  EXPECT_EQ(SampleRenderedPixel(close_button(), ODS_FOCUS, SamplePoint::kFrame),
            kCaptionFrameColorDark);
}

// A control too small for a glyph region still paints its background and its
// focus frame. `GetButtonRgn` returns null below a few pixels, and because
// BS_OWNERDRAW suppresses the stock focus rectangle, skipping the frame here
// would leave the button with no keyboard focus indicator at all.
TEST_F(CaptionButtonThemeTest, DegenerateSizeStillDrawsFocusFrame) {
  ASSERT_NO_FATAL_FAILURE(CreateTestDialog(/*dark=*/false));

  // 2x2 is below the glyph threshold: the region is sized at 12/31 of the
  // control, which floors to zero, and `GetButtonRgn` returns null.
  ASSERT_TRUE(::SetWindowPos(close_button().hwnd(), nullptr, 0, 0, 2, 2,
                             SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE));

  // No glyph: the accent color must not appear anywhere.
  EXPECT_THAT(CountRenderedPixels(close_button(), /*item_state=*/0,
                                  kCaptionForegroundColor),
              Optional(0));

  // The focus frame is still drawn.
  EXPECT_THAT(
      CountRenderedPixels(close_button(), ODS_FOCUS, kCaptionFrameColor),
      Optional(Gt(0)));
}

// The caption buttons follow a dynamic theme change delivered as
// WM_SETTINGCHANGE, and a WM_SETTINGCHANGE that `CouldBeThemeSettingChange()`
// rejects leaves them alone.
TEST_F(CaptionButtonThemeTest, ThemeSwitching) {
  ASSERT_NO_FATAL_FAILURE(CreateTestDialog(/*dark=*/false));
  ASSERT_EQ(GlyphColor(close_button()), kCaptionForegroundColor);

  // The theme is switched underneath first, so a handler that acted on the
  // rejected message would be caught by the color check below.
  ASSERT_NO_FATAL_FAILURE(SetDarkMode(true));
  ::SendMessage(progress_wnd_->hwnd(), WM_SETTINGCHANGE,
                SPI_SETNONCLIENTMETRICS,
                reinterpret_cast<LPARAM>(L"WindowMetrics"));
  EXPECT_FALSE(IsDarkMode(close_button()));
  EXPECT_EQ(GlyphColor(close_button()), kCaptionForegroundColor);

  // "ImmersiveColorSet" is the section the shell broadcasts on a light/dark
  // mode change, and arrives with a zero wParam, which is accepted.
  ::SendMessage(progress_wnd_->hwnd(), WM_SETTINGCHANGE, 0,
                reinterpret_cast<LPARAM>(L"ImmersiveColorSet"));
  EXPECT_TRUE(IsDarkMode(close_button()));
  EXPECT_TRUE(IsDarkMode(minimize_button()));
  EXPECT_EQ(GlyphColor(close_button()), kCaptionForegroundColorDark);
  EXPECT_EQ(GlyphColor(minimize_button()), kCaptionForegroundColorDark);

  // Switching back via WM_THEMECHANGED restores the light mode color. The
  // system sends WM_THEMECHANGED to every window, including children, so the
  // broadcast is simulated here rather than relying on parent propagation.
  ASSERT_NO_FATAL_FAILURE(SetDarkMode(false));
  ::SendMessage(progress_wnd_->hwnd(), WM_THEMECHANGED, 0, 0);
  ::SendMessage(close_button().hwnd(), WM_THEMECHANGED, 0, 0);
  ::SendMessage(minimize_button().hwnd(), WM_THEMECHANGED, 0, 0);
  EXPECT_FALSE(IsDarkMode(close_button()));
  EXPECT_FALSE(IsDarkMode(minimize_button()));
  EXPECT_EQ(GlyphColor(close_button()), kCaptionForegroundColor);
  EXPECT_EQ(GlyphColor(minimize_button()), kCaptionForegroundColor);
}

// Pins the propagation rule in `OmahaWnd::RepaintForThemeChange()`: the system
// delivers WM_THEMECHANGED to children directly, so the dialog must not
// forward it.
TEST_F(CaptionButtonThemeTest, ThemeChangedIsNotForwardedToChildren) {
  ASSERT_NO_FATAL_FAILURE(CreateTestDialog(/*dark=*/false));
  ASSERT_FALSE(IsDarkMode(close_button()));

  ASSERT_NO_FATAL_FAILURE(SetDarkMode(true));
  ::SendMessage(progress_wnd_->hwnd(), WM_THEMECHANGED, 0, 0);
  EXPECT_FALSE(IsDarkMode(close_button()))
      << "WM_THEMECHANGED must not be forwarded to descendants; the system "
         "already delivers it to them";

  // Which the control does act on, so the check above is not just asserting
  // that nothing works.
  ::SendMessage(close_button().hwnd(), WM_THEMECHANGED, 0, 0);
  EXPECT_TRUE(IsDarkMode(close_button()));
}

// The other side of that rule: WM_SYSCOLORCHANGE only reaches top-level
// windows, so the dialog must forward it.
TEST_F(CaptionButtonThemeTest, SysColorChangeIsForwardedToChildren) {
  ASSERT_NO_FATAL_FAILURE(CreateTestDialog(/*dark=*/false));
  ASSERT_FALSE(IsDarkMode(close_button()));

  ASSERT_NO_FATAL_FAILURE(SetDarkMode(true));
  ::SendMessage(progress_wnd_->hwnd(), WM_SYSCOLORCHANGE, 0, 0);
  EXPECT_TRUE(IsDarkMode(close_button()));
  EXPECT_TRUE(IsDarkMode(minimize_button()));
}

// The mouse-tracking contract `TrackedButton` implements for every subclass.
// Shared rather than duplicated per control: the point of the base class is
// that `CaptionButton` and `FlatButton` behave identically here, so two copies
// of these assertions would only be free to drift apart.
//
// Callers must keep the control parked off screen and must not pump between
// the sends and the expectations, otherwise a physical cursor can deliver a
// WM_MOUSELEAVE that perturbs the state.
void ExpectHoverIgnoresPointsOutsideTheClientRect(const TrackedButton& button) {
  const HWND hwnd = button.hwnd();

  // A move over the control highlights it and arms tracking so that the
  // highlight can be cleared again.
  ::SendMessage(hwnd, WM_MOUSEMOVE, 0, 0);
  EXPECT_TRUE(IsMouseHovering(button));
  EXPECT_TRUE(IsTrackingMouseEvents(button));

  ::SendMessage(hwnd, WM_MOUSELEAVE, 0, 0);
  EXPECT_FALSE(IsMouseHovering(button));
  EXPECT_FALSE(IsTrackingMouseEvents(button));

  // `BUTTON` captures the mouse while pressed, so WM_MOUSEMOVE for points
  // outside the client rect is still delivered to the control. Such moves must
  // not apply the hover highlight.
  ::SendMessage(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(-1, -1));
  EXPECT_FALSE(IsMouseHovering(button));
  EXPECT_FALSE(IsTrackingMouseEvents(button));

  // ::PtInRect is inclusive on left/top and exclusive on right/bottom, and a
  // real drag exits across an edge, so pin both sides of that boundary.
  RECT client_rect = {};
  ASSERT_TRUE(::GetClientRect(hwnd, &client_rect));
  const int width = client_rect.right - client_rect.left;
  const int height = client_rect.bottom - client_rect.top;
  ASSERT_GT(width, 0);
  ASSERT_GT(height, 0);

  ::SendMessage(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(width - 1, height - 1));
  EXPECT_TRUE(IsMouseHovering(button)) << "the last inside pixel";
  ::SendMessage(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(width, height));
  EXPECT_FALSE(IsMouseHovering(button)) << "right/bottom are exclusive";

  ::SendMessage(hwnd, WM_MOUSELEAVE, 0, 0);

  // Dragging back inside restores the hover highlight, and dragging back out
  // clears it again.
  ::SendMessage(hwnd, WM_MOUSEMOVE, 0, 0);
  EXPECT_TRUE(IsMouseHovering(button));
  ::SendMessage(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(-1, -1));
  EXPECT_FALSE(IsMouseHovering(button));

  ::SendMessage(hwnd, WM_MOUSELEAVE, 0, 0);
  EXPECT_FALSE(IsMouseHovering(button));
  EXPECT_FALSE(IsTrackingMouseEvents(button));
}

void ExpectDisablingClearsHoverAndCancelsTracking(const TrackedButton& button) {
  const HWND hwnd = button.hwnd();

  // A disabled control does not enter the hover highlight state.
  ::EnableWindow(hwnd, FALSE);
  ::SendMessage(hwnd, WM_MOUSEMOVE, 0, 0);
  EXPECT_FALSE(IsMouseHovering(button));
  EXPECT_FALSE(IsTrackingMouseEvents(button));

  // Re-enabling allows the hover state again, and arms mouse tracking so that
  // WM_MOUSELEAVE can clear the highlight.
  ::EnableWindow(hwnd, TRUE);
  ::SendMessage(hwnd, WM_MOUSEMOVE, 0, 0);
  EXPECT_TRUE(IsMouseHovering(button));
  EXPECT_TRUE(IsTrackingMouseEvents(button));

  // Disabling while hovered clears the highlight via WM_ENABLE and cancels
  // tracking, since a disabled window never receives WM_MOUSELEAVE.
  ::EnableWindow(hwnd, FALSE);
  EXPECT_FALSE(IsMouseHovering(button));
  EXPECT_FALSE(IsTrackingMouseEvents(button));

  // As with a stock `BUTTON`, the pointer has to move again.
  ::EnableWindow(hwnd, TRUE);
  EXPECT_FALSE(IsMouseHovering(button));
  EXPECT_FALSE(IsTrackingMouseEvents(button));

  ::SendMessage(hwnd, WM_MOUSEMOVE, 0, 0);
  EXPECT_TRUE(IsMouseHovering(button));
  EXPECT_TRUE(IsTrackingMouseEvents(button));

  ::SendMessage(hwnd, WM_MOUSELEAVE, 0, 0);
  EXPECT_FALSE(IsMouseHovering(button));
  EXPECT_FALSE(IsTrackingMouseEvents(button));
}

void ExpectHidingClearsHoverAndCancelsTracking(const TrackedButton& button) {
  const HWND hwnd = button.hwnd();

  ::SendMessage(hwnd, WM_MOUSEMOVE, 0, 0);
  EXPECT_TRUE(IsMouseHovering(button));
  EXPECT_TRUE(IsTrackingMouseEvents(button));

  // Hiding while hovered clears the highlight via WM_SHOWWINDOW and cancels
  // tracking, since a hidden window never receives WM_MOUSELEAVE.
  ::ShowWindow(hwnd, SW_HIDE);
  EXPECT_FALSE(IsMouseHovering(button));
  EXPECT_FALSE(IsTrackingMouseEvents(button));

  // Re-showing does not automatically restore hover; the pointer must move
  // again.
  ::ShowWindow(hwnd, SW_SHOW);
  EXPECT_FALSE(IsMouseHovering(button));
  EXPECT_FALSE(IsTrackingMouseEvents(button));

  ::SendMessage(hwnd, WM_MOUSEMOVE, 0, 0);
  EXPECT_TRUE(IsMouseHovering(button));
  EXPECT_TRUE(IsTrackingMouseEvents(button));

  ::SendMessage(hwnd, WM_MOUSELEAVE, 0, 0);
  EXPECT_FALSE(IsMouseHovering(button));
  EXPECT_FALSE(IsTrackingMouseEvents(button));
}

// Both caption buttons share the state machine, so both are exercised, even
// though only the close button is disabled in production.
TEST_F(CaptionButtonTest, HoverIgnoresPointsOutsideTheClientRect) {
  ASSERT_NO_FATAL_FAILURE(CreateTestDialog());

  {
    SCOPED_TRACE("close");
    ASSERT_NO_FATAL_FAILURE(
        ExpectHoverIgnoresPointsOutsideTheClientRect(close_button()));
  }
  {
    SCOPED_TRACE("minimize");
    ASSERT_NO_FATAL_FAILURE(
        ExpectHoverIgnoresPointsOutsideTheClientRect(minimize_button()));
  }
}

TEST_F(CaptionButtonTest, DisablingClearsHoverAndCancelsTracking) {
  ASSERT_NO_FATAL_FAILURE(CreateTestDialog());

  {
    SCOPED_TRACE("close");
    ASSERT_NO_FATAL_FAILURE(
        ExpectDisablingClearsHoverAndCancelsTracking(close_button()));
  }
  {
    SCOPED_TRACE("minimize");
    ASSERT_NO_FATAL_FAILURE(
        ExpectDisablingClearsHoverAndCancelsTracking(minimize_button()));
  }
}

TEST_F(CaptionButtonTest, HidingClearsHoverAndCancelsTracking) {
  ASSERT_NO_FATAL_FAILURE(CreateTestDialog());

  {
    SCOPED_TRACE("close");
    ASSERT_NO_FATAL_FAILURE(
        ExpectHidingClearsHoverAndCancelsTracking(close_button()));
  }
  {
    SCOPED_TRACE("minimize");
    ASSERT_NO_FATAL_FAILURE(
        ExpectHidingClearsHoverAndCancelsTracking(minimize_button()));
  }
}

// `FlatButton` subclasses a bare `BUTTON` rather than living in a dialog, so
// the control is created directly here.
class FlatButtonTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // WS_VISIBLE is required: `SW_HIDE` only produces WM_SHOWWINDOW for a
    // window that is currently visible, which is what
    // `HidingClearsHoverAndCancelsTracking` drives. The window is parked off
    // screen so that a physical cursor still cannot reach it, which is the
    // same precaution `CaptionButtonTest` takes by not showing its dialog.
    hwnd_ = ::CreateWindowExW(
        0, L"BUTTON", L"TestFlatButton", WS_POPUP | WS_VISIBLE | BS_PUSHBUTTON,
        -10000, -10000, 100, 30, nullptr, nullptr, nullptr, nullptr);
    ASSERT_NE(hwnd_, nullptr);
    ASSERT_TRUE(button_.SubclassWindow(hwnd_));
  }

  void TearDown() override {
    if (hwnd_ && ::IsWindow(hwnd_)) {
      ::DestroyWindow(hwnd_);
    }
  }

  HWND hwnd_ = nullptr;
  FlatButton button_;
};

TEST_F(FlatButtonTest, HoverIgnoresPointsOutsideTheClientRect) {
  ASSERT_NO_FATAL_FAILURE(
      ExpectHoverIgnoresPointsOutsideTheClientRect(button_));
}

TEST_F(FlatButtonTest, DisablingClearsHoverAndCancelsTracking) {
  ASSERT_NO_FATAL_FAILURE(
      ExpectDisablingClearsHoverAndCancelsTracking(button_));
}

TEST_F(FlatButtonTest, HidingClearsHoverAndCancelsTracking) {
  ASSERT_NO_FATAL_FAILURE(ExpectHidingClearsHoverAndCancelsTracking(button_));
}

}  // namespace
}  // namespace updater::ui
