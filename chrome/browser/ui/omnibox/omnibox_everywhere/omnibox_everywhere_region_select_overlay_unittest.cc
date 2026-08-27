// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_region_select_overlay.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/test/test_screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace omnibox_everywhere {

namespace {

// Overrides the active Screen instance for the scope of a test and restores
// the original instance installed by ChromeViewsTestBase upon destruction.
class ScopedScreenOverride {
 public:
  explicit ScopedScreenOverride(display::Screen* new_screen)
      : old_screen_(display::Screen::SetScreenInstance(nullptr)) {
    display::Screen::SetScreenInstance(new_screen);
  }
  ~ScopedScreenOverride() {
    display::Screen::SetScreenInstance(nullptr);
    if (old_screen_) {
      display::Screen::SetScreenInstance(old_screen_);
    }
  }

 private:
  raw_ptr<display::Screen> old_screen_;
};

}  // namespace

class OmniboxEverywhereRegionSelectOverlayTest : public ChromeViewsTestBase {
 public:
  using RegionCaptureSource =
      OmniboxEverywhereRegionSelectOverlay::RegionCaptureSource;

  OmniboxEverywhereRegionSelectOverlayTest() = default;
  ~OmniboxEverywhereRegionSelectOverlayTest() override = default;
};

TEST_F(OmniboxEverywhereRegionSelectOverlayTest, CreateAndDismiss) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  bitmap.eraseColor(SK_ColorRED);

  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      bitmap, RegionCaptureSource::AllDisplays(), future.GetCallback(),
      GetContext());
  ASSERT_TRUE(overlay);
  ASSERT_TRUE(overlay->widget());
  EXPECT_TRUE(overlay->widget()->IsVisible());

  // Close the overlay widget (simulating Escape / dismiss).
  overlay->widget()->CloseWithReason(
      views::Widget::ClosedReason::kEscKeyPressed);

  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
}

TEST_F(OmniboxEverywhereRegionSelectOverlayTest, ConfirmReturnsBitmap) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  bitmap.eraseColor(SK_ColorBLUE);

  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      bitmap, RegionCaptureSource::AllDisplays(), future.GetCallback(),
      GetContext());
  ASSERT_TRUE(overlay);
  ASSERT_TRUE(overlay->widget());

  // Trigger mouse press to confirm.
  ui::MouseEvent press_event(ui::EventType::kMousePressed, gfx::Point(50, 50),
                             gfx::Point(50, 50), base::TimeTicks::Now(),
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  overlay->widget()->GetContentsView()->OnMousePressed(press_event);

  EXPECT_TRUE(future.IsReady());
  EXPECT_FALSE(future.Get().empty());
  EXPECT_EQ(future.Get().getColor(0, 0), SK_ColorBLUE);
}

TEST_F(OmniboxEverywhereRegionSelectOverlayTest, EscapeKeyPressedCancels) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  bitmap.eraseColor(SK_ColorRED);

  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      bitmap, RegionCaptureSource::AllDisplays(), future.GetCallback(),
      GetContext());
  ASSERT_TRUE(overlay);
  ASSERT_TRUE(overlay->widget());

  // Trigger Escape key via FocusManager accelerator.
  ui::Accelerator escape_accel(ui::VKEY_ESCAPE, ui::EF_NONE);
  EXPECT_TRUE(
      overlay->widget()->GetFocusManager()->ProcessAccelerator(escape_accel));

  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
}

TEST_F(OmniboxEverywhereRegionSelectOverlayTest,
       NativeCloseNowDismissesCleanly) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  bitmap.eraseColor(SK_ColorRED);

  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      bitmap, RegionCaptureSource::AllDisplays(), future.GetCallback(),
      GetContext());
  ASSERT_TRUE(overlay);
  ASSERT_TRUE(overlay->widget());

  // Synchronously closing native window directly.
  overlay->widget()->CloseNow();

  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
}

TEST_F(OmniboxEverywhereRegionSelectOverlayTest,
       DestructorDismissesPendingCallback) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  bitmap.eraseColor(SK_ColorGREEN);

  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      bitmap, RegionCaptureSource::AllDisplays(), future.GetCallback(),
      GetContext());
  ASSERT_TRUE(overlay);

  overlay.reset();

  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
}

TEST_F(OmniboxEverywhereRegionSelectOverlayTest,
       MultiDisplayVirtualDesktop_SpansAllDisplays) {
  display::test::TestScreen test_screen(/*create_display=*/false,
                                        /*register_screen=*/false);
  ScopedScreenOverride screen_override(&test_screen);
  display::Display display1(1, gfx::Rect(0, 0, 800, 600));
  display::Display display2(2, gfx::Rect(800, 0, 1024, 768));
  test_screen.display_list().AddDisplay(display1,
                                        display::DisplayList::Type::PRIMARY);
  test_screen.display_list().AddDisplay(
      display2, display::DisplayList::Type::NOT_PRIMARY);

  SkBitmap bitmap;
  bitmap.allocN32Pixels(1824, 768);

  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      bitmap, RegionCaptureSource::AllDisplays(), future.GetCallback(),
      GetContext());
  ASSERT_TRUE(overlay);
  ASSERT_TRUE(overlay->widget());
  EXPECT_EQ(overlay->widget()->GetWindowBoundsInScreen(),
            gfx::Rect(0, 0, 1824, 768));
}

// Verifies that when a specific display is requested but cannot be found (e.g.
// disconnected), GetOverlayBoundsForSource falls back to the primary display.
TEST_F(OmniboxEverywhereRegionSelectOverlayTest,
       ForDisplay_DisconnectedDisplayFallsBackToPrimary) {
  display::test::TestScreen test_screen(/*create_display=*/false,
                                        /*register_screen=*/false);
  ScopedScreenOverride screen_override(&test_screen);
  display::Display display1(1, gfx::Rect(0, 0, 800, 600));
  test_screen.display_list().AddDisplay(display1,
                                        display::DisplayList::Type::PRIMARY);

  SkBitmap bitmap;
  bitmap.allocN32Pixels(800, 600);

  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      bitmap, RegionCaptureSource::ForDisplay(900), future.GetCallback(),
      GetContext());
  ASSERT_TRUE(overlay);
  ASSERT_TRUE(overlay->widget());
  EXPECT_EQ(overlay->widget()->GetWindowBoundsInScreen(),
            gfx::Rect(0, 0, 800, 600));
}

TEST_F(OmniboxEverywhereRegionSelectOverlayTest,
       SingleDisplayMatch_MatchesTargetDisplay) {
  display::test::TestScreen test_screen(/*create_display=*/false,
                                        /*register_screen=*/false);
  ScopedScreenOverride screen_override(&test_screen);
  display::Display display1(1, gfx::Rect(0, 0, 800, 600));
  display::Display display2(2, gfx::Rect(800, 0, 1024, 768));
  test_screen.display_list().AddDisplay(display1,
                                        display::DisplayList::Type::PRIMARY);
  test_screen.display_list().AddDisplay(
      display2, display::DisplayList::Type::NOT_PRIMARY);

  SkBitmap bitmap;
  bitmap.allocN32Pixels(1024, 768);

  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      bitmap, RegionCaptureSource::ForDisplay(display2.id()),
      future.GetCallback(), GetContext());
  ASSERT_TRUE(overlay);
  ASSERT_TRUE(overlay->widget());
  EXPECT_EQ(overlay->widget()->GetWindowBoundsInScreen(),
            gfx::Rect(800, 0, 1024, 768));
}

// Verifies that RegionCaptureSource::ForDisplay correctly targets and scales to
// a display in portrait orientation.
TEST_F(OmniboxEverywhereRegionSelectOverlayTest,
       ForDisplay_MatchesPortraitDisplayBounds) {
  display::test::TestScreen test_screen(/*create_display=*/false,
                                        /*register_screen=*/false);
  ScopedScreenOverride screen_override(&test_screen);
  display::Display display1(1, gfx::Rect(0, 0, 1080, 1920));
  test_screen.display_list().AddDisplay(display1,
                                        display::DisplayList::Type::PRIMARY);

  SkBitmap bitmap;
  bitmap.allocN32Pixels(1080, 1920);

  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      bitmap, RegionCaptureSource::ForDisplay(display1.id()),
      future.GetCallback(), GetContext());
  ASSERT_TRUE(overlay);
  ASSERT_TRUE(overlay->widget());
  EXPECT_EQ(overlay->widget()->GetWindowBoundsInScreen(),
            gfx::Rect(0, 0, 1080, 1920));
}

}  // namespace omnibox_everywhere
