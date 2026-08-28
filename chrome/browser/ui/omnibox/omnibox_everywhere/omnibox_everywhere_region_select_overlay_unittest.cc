// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_region_select_overlay.h"

#include <memory>
#include <vector>

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

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    test_screen_ = std::make_unique<display::test::TestScreen>(
        /*create_display=*/false, /*register_screen=*/false);
    screen_override_ =
        std::make_unique<ScopedScreenOverride>(test_screen_.get());
    SetDisplays({display::Display(1, gfx::Rect(0, 0, 100, 100))});
  }

  void TearDown() override {
    screen_override_.reset();
    test_screen_.reset();
    ChromeViewsTestBase::TearDown();
  }

  void SetDisplays(const std::vector<display::Display>& displays,
                   size_t primary_index = 0) {
    auto list = test_screen_->display_list().displays();
    for (const auto& d : list) {
      test_screen_->display_list().RemoveDisplay(d.id());
    }
    for (size_t i = 0; i < displays.size(); ++i) {
      test_screen_->display_list().AddDisplay(
          displays[i], i == primary_index
                           ? display::DisplayList::Type::PRIMARY
                           : display::DisplayList::Type::NOT_PRIMARY);
    }
  }

  SkBitmap CreateTestBitmap(int width = 100,
                            int height = 100,
                            SkColor color = SK_ColorBLUE) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(width, height);
    bitmap.eraseColor(color);
    return bitmap;
  }

  void SimulateMouseDrag(views::View* view,
                         const gfx::Point& start,
                         const gfx::Point& end) {
    ui::MouseEvent press(ui::EventType::kMousePressed, start, start,
                         base::TimeTicks::Now(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
    view->OnMousePressed(press);

    ui::MouseEvent drag(ui::EventType::kMouseDragged, end, end,
                        base::TimeTicks::Now(), ui::EF_LEFT_MOUSE_BUTTON,
                        ui::EF_LEFT_MOUSE_BUTTON);
    view->OnMouseDragged(drag);

    ui::MouseEvent release(ui::EventType::kMouseReleased, end, end,
                           base::TimeTicks::Now(), ui::EF_LEFT_MOUSE_BUTTON,
                           ui::EF_LEFT_MOUSE_BUTTON);
    view->OnMouseReleased(release);
  }

  void SimulateMouseClick(views::View* view, const gfx::Point& point) {
    ui::MouseEvent press(ui::EventType::kMousePressed, point, point,
                         base::TimeTicks::Now(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
    view->OnMousePressed(press);

    ui::MouseEvent release(ui::EventType::kMouseReleased, point, point,
                           base::TimeTicks::Now(), ui::EF_LEFT_MOUSE_BUTTON,
                           ui::EF_LEFT_MOUSE_BUTTON);
    view->OnMouseReleased(release);
  }

  void SimulateGestureDrag(views::View* view,
                           const gfx::Point& start,
                           const gfx::Point& end) {
    ui::GestureEvent tap_down(
        start.x(), start.y(), ui::EF_NONE, base::TimeTicks::Now(),
        ui::GestureEventDetails(ui::EventType::kGestureTapDown));
    view->OnGestureEvent(&tap_down);

    ui::GestureEvent tap_cancel(
        start.x(), start.y(), ui::EF_NONE, base::TimeTicks::Now(),
        ui::GestureEventDetails(ui::EventType::kGestureTapCancel));
    view->OnGestureEvent(&tap_cancel);

    ui::GestureEvent begin(
        start.x(), start.y(), ui::EF_NONE, base::TimeTicks::Now(),
        ui::GestureEventDetails(ui::EventType::kGestureScrollBegin));
    view->OnGestureEvent(&begin);

    ui::GestureEvent update(
        end.x(), end.y(), ui::EF_NONE, base::TimeTicks::Now(),
        ui::GestureEventDetails(ui::EventType::kGestureScrollUpdate));
    view->OnGestureEvent(&update);

    ui::GestureEvent finish(
        end.x(), end.y(), ui::EF_NONE, base::TimeTicks::Now(),
        ui::GestureEventDetails(ui::EventType::kGestureScrollEnd));
    view->OnGestureEvent(&finish);
  }

 private:
  std::unique_ptr<display::test::TestScreen> test_screen_;
  std::unique_ptr<ScopedScreenOverride> screen_override_;
};

TEST_F(OmniboxEverywhereRegionSelectOverlayTest, CreateAndDismiss) {
  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      CreateTestBitmap(100, 100, SK_ColorRED),
      RegionCaptureSource::AllDisplays(), future.GetCallback(), GetContext());
  ASSERT_TRUE(overlay);
  ASSERT_TRUE(overlay->widget());
  EXPECT_TRUE(overlay->widget()->IsVisible());

  // Close the overlay widget (simulating Escape / dismiss).
  overlay->widget()->CloseWithReason(
      views::Widget::ClosedReason::kEscKeyPressed);

  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
}

TEST_F(OmniboxEverywhereRegionSelectOverlayTest,
       DragSelectReturnsCroppedBitmap) {
  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      CreateTestBitmap(), RegionCaptureSource::AllDisplays(),
      future.GetCallback(), GetContext());
  ASSERT_TRUE(overlay);

  SimulateMouseDrag(overlay->widget()->GetContentsView(), gfx::Point(10, 10),
                    gfx::Point(60, 60));

  EXPECT_TRUE(future.IsReady());
  EXPECT_FALSE(future.Get().empty());
  EXPECT_EQ(future.Get().width(), 50);
  EXPECT_EQ(future.Get().height(), 50);
  EXPECT_EQ(future.Get().getColor(0, 0), SK_ColorBLUE);
}

TEST_F(OmniboxEverywhereRegionSelectOverlayTest, ClickWithoutDragCancels) {
  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      CreateTestBitmap(), RegionCaptureSource::AllDisplays(),
      future.GetCallback(), GetContext());
  ASSERT_TRUE(overlay);

  // Single click without dragging (< 10px selection size threshold) should
  // cancel.
  SimulateMouseClick(overlay->widget()->GetContentsView(), gfx::Point(50, 50));

  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
}

TEST_F(OmniboxEverywhereRegionSelectOverlayTest, EscapeKeyPressedCancels) {
  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      CreateTestBitmap(100, 100, SK_ColorRED),
      RegionCaptureSource::AllDisplays(), future.GetCallback(), GetContext());
  ASSERT_TRUE(overlay);

  // Trigger Escape key via FocusManager accelerator.
  ui::Accelerator escape_accel(ui::VKEY_ESCAPE, ui::EF_NONE);
  EXPECT_TRUE(
      overlay->widget()->GetFocusManager()->ProcessAccelerator(escape_accel));

  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
}

TEST_F(OmniboxEverywhereRegionSelectOverlayTest,
       NativeCloseNowDismissesCleanly) {
  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      CreateTestBitmap(100, 100, SK_ColorRED),
      RegionCaptureSource::AllDisplays(), future.GetCallback(), GetContext());
  ASSERT_TRUE(overlay);

  // Synchronously closing native window directly.
  overlay->widget()->CloseNow();

  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
}

TEST_F(OmniboxEverywhereRegionSelectOverlayTest,
       DestructorDismissesPendingCallback) {
  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      CreateTestBitmap(100, 100, SK_ColorGREEN),
      RegionCaptureSource::AllDisplays(), future.GetCallback(), GetContext());
  ASSERT_TRUE(overlay);

  overlay.reset();

  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
}

TEST_F(OmniboxEverywhereRegionSelectOverlayTest,
       MultiDisplayVirtualDesktop_SpansAllDisplays) {
  SetDisplays({display::Display(1, gfx::Rect(0, 0, 800, 600)),
               display::Display(2, gfx::Rect(800, 0, 1024, 768))});

  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      CreateTestBitmap(1824, 768), RegionCaptureSource::AllDisplays(),
      future.GetCallback(), GetContext());
  ASSERT_TRUE(overlay);
  ASSERT_TRUE(overlay->widget());
  EXPECT_EQ(overlay->widget()->GetWindowBoundsInScreen(),
            gfx::Rect(0, 0, 1824, 768));
}

// Verifies that when a specific display is requested but cannot be found (e.g.
// disconnected), GetOverlayBoundsForSource falls back to the primary display.
TEST_F(OmniboxEverywhereRegionSelectOverlayTest,
       ForDisplay_DisconnectedDisplayFallsBackToPrimary) {
  SetDisplays({display::Display(1, gfx::Rect(0, 0, 800, 600))});

  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      CreateTestBitmap(800, 600), RegionCaptureSource::ForDisplay(900),
      future.GetCallback(), GetContext());
  ASSERT_TRUE(overlay);
  ASSERT_TRUE(overlay->widget());
  EXPECT_EQ(overlay->widget()->GetWindowBoundsInScreen(),
            gfx::Rect(0, 0, 800, 600));
}

TEST_F(OmniboxEverywhereRegionSelectOverlayTest,
       SingleDisplayMatch_MatchesTargetDisplay) {
  SetDisplays({display::Display(1, gfx::Rect(0, 0, 800, 600)),
               display::Display(2, gfx::Rect(800, 0, 1024, 768))});

  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      CreateTestBitmap(1024, 768), RegionCaptureSource::ForDisplay(2),
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
  SetDisplays({display::Display(1, gfx::Rect(0, 0, 1080, 1920))});

  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      CreateTestBitmap(1080, 1920), RegionCaptureSource::ForDisplay(1),
      future.GetCallback(), GetContext());
  ASSERT_TRUE(overlay);
  ASSERT_TRUE(overlay->widget());
  EXPECT_EQ(overlay->widget()->GetWindowBoundsInScreen(),
            gfx::Rect(0, 0, 1080, 1920));
}

TEST_F(OmniboxEverywhereRegionSelectOverlayTest,
       GestureDragSelectReturnsCroppedBitmap) {
  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      CreateTestBitmap(), RegionCaptureSource::AllDisplays(),
      future.GetCallback(), GetContext());
  ASSERT_TRUE(overlay);

  SimulateGestureDrag(overlay->widget()->GetContentsView(), gfx::Point(10, 10),
                      gfx::Point(70, 70));

  EXPECT_TRUE(future.IsReady());
  EXPECT_FALSE(future.Get().empty());
  EXPECT_EQ(future.Get().width(), 60);
  EXPECT_EQ(future.Get().height(), 60);
  EXPECT_EQ(future.Get().getColor(0, 0), SK_ColorBLUE);
}

TEST_F(OmniboxEverywhereRegionSelectOverlayTest,
       DragOutOfBoundsClampsToImageIntersection) {
  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      CreateTestBitmap(), RegionCaptureSource::AllDisplays(),
      future.GetCallback(), GetContext());
  ASSERT_TRUE(overlay);

  // Drag from (50, 50) to (150, 150) (crossing outer image boundary).
  SimulateMouseDrag(overlay->widget()->GetContentsView(), gfx::Point(50, 50),
                    gfx::Point(150, 150));

  EXPECT_TRUE(future.IsReady());
  EXPECT_FALSE(future.Get().empty());
  // Cropped area should be the intersection (50, 50) -> (100, 100), width=50,
  // height=50.
  EXPECT_EQ(future.Get().width(), 50);
  EXPECT_EQ(future.Get().height(), 50);
  EXPECT_EQ(future.Get().getColor(0, 0), SK_ColorBLUE);
}

TEST_F(OmniboxEverywhereRegionSelectOverlayTest,
       InvertedDragSelectReturnsCroppedBitmap) {
  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      CreateTestBitmap(), RegionCaptureSource::AllDisplays(),
      future.GetCallback(), GetContext());
  ASSERT_TRUE(overlay);

  // Inverted drag: Press at bottom-right (60, 60), drag to top-left (10, 10).
  SimulateMouseDrag(overlay->widget()->GetContentsView(), gfx::Point(60, 60),
                    gfx::Point(10, 10));

  EXPECT_TRUE(future.IsReady());
  EXPECT_FALSE(future.Get().empty());
  EXPECT_EQ(future.Get().width(), 50);
  EXPECT_EQ(future.Get().height(), 50);
  EXPECT_EQ(future.Get().getColor(0, 0), SK_ColorBLUE);
}

TEST_F(OmniboxEverywhereRegionSelectOverlayTest,
       DragNegativeOutOfBoundsClampsToImageIntersection) {
  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      CreateTestBitmap(), RegionCaptureSource::AllDisplays(),
      future.GetCallback(), GetContext());
  ASSERT_TRUE(overlay);

  // Drag from (50, 50) to negative coordinates (-50, -50).
  SimulateMouseDrag(overlay->widget()->GetContentsView(), gfx::Point(50, 50),
                    gfx::Point(-50, -50));

  EXPECT_TRUE(future.IsReady());
  EXPECT_FALSE(future.Get().empty());
  // Cropped area should be the intersection (0, 0) -> (50, 50), width=50,
  // height=50.
  EXPECT_EQ(future.Get().width(), 50);
  EXPECT_EQ(future.Get().height(), 50);
  EXPECT_EQ(future.Get().getColor(0, 0), SK_ColorBLUE);
}

TEST_F(OmniboxEverywhereRegionSelectOverlayTest,
       SmallSelectionAboveThresholdClampsReticlesAndReturnsBitmap) {
  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      CreateTestBitmap(), RegionCaptureSource::AllDisplays(),
      future.GetCallback(), GetContext());
  ASSERT_TRUE(overlay);

  // Drag 14x14 rectangle (above 10px min size, below 20px reticle len).
  SimulateMouseDrag(overlay->widget()->GetContentsView(), gfx::Point(10, 10),
                    gfx::Point(24, 24));

  EXPECT_TRUE(future.IsReady());
  EXPECT_FALSE(future.Get().empty());
  EXPECT_EQ(future.Get().width(), 14);
  EXPECT_EQ(future.Get().height(), 14);
  EXPECT_EQ(future.Get().getColor(0, 0), SK_ColorBLUE);
}

TEST_F(OmniboxEverywhereRegionSelectOverlayTest,
       MouseCaptureLostCancelsSelection) {
  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      CreateTestBitmap(), RegionCaptureSource::AllDisplays(),
      future.GetCallback(), GetContext());
  ASSERT_TRUE(overlay);

  views::View* contents_view = overlay->widget()->GetContentsView();
  ASSERT_TRUE(contents_view);

  // Press at (10, 10), drag to (50, 50).
  ui::MouseEvent press_event(ui::EventType::kMousePressed, gfx::Point(10, 10),
                             gfx::Point(10, 10), base::TimeTicks::Now(),
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  contents_view->OnMousePressed(press_event);

  ui::MouseEvent drag_event(ui::EventType::kMouseDragged, gfx::Point(50, 50),
                            gfx::Point(50, 50), base::TimeTicks::Now(),
                            ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  contents_view->OnMouseDragged(drag_event);

  // Simulate mouse capture lost during drag.
  contents_view->OnMouseCaptureLost();

  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
}

}  // namespace omnibox_everywhere
