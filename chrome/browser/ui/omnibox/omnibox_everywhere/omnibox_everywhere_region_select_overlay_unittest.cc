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
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/test/views_drawing_test_utils.h"
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

#if BUILDFLAG(IS_MAC)
constexpr int kExpectedTopMargin = 56;
#else
constexpr int kExpectedTopMargin = 28;
#endif

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
    if (primary_index < displays.size()) {
      test_screen_->display_list().AddDisplay(
          displays[primary_index], display::DisplayList::Type::PRIMARY);
    }
    for (size_t i = 0; i < displays.size(); ++i) {
      if (i != primary_index) {
        test_screen_->display_list().AddDisplay(
            displays[i], display::DisplayList::Type::NOT_PRIMARY);
      }
    }
  }

  void SetCursorScreenPoint(const gfx::Point& point) {
    test_screen_->set_cursor_screen_point(point);
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
  ASSERT_TRUE(overlay->GetActiveWidgetForTesting());
  EXPECT_TRUE(overlay->GetActiveWidgetForTesting()->IsVisible());

  // Close the overlay widget (simulating Escape / dismiss).
  overlay->GetActiveWidgetForTesting()->CloseWithReason(
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

  SimulateMouseDrag(overlay->GetActiveWidgetForTesting()->GetContentsView(),
                    gfx::Point(10, 10), gfx::Point(60, 60));

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
  SimulateMouseClick(overlay->GetActiveWidgetForTesting()->GetContentsView(),
                     gfx::Point(50, 50));

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
  EXPECT_TRUE(overlay->GetActiveWidgetForTesting()
                  ->GetFocusManager()
                  ->ProcessAccelerator(escape_accel));

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
  overlay->GetActiveWidgetForTesting()->CloseNow();

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
  // Use non-zero origin (100, 100) away from edges to avoid OS menu bar
  // clamping on macOS (where y=0 at the top of the display is adjusted down to
  // y=30 for frameless top-level windows).
  SetDisplays({display::Display(1, gfx::Rect(100, 100, 800, 600)),
               display::Display(2, gfx::Rect(900, 100, 1024, 768))});

  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      CreateTestBitmap(1824, 768), RegionCaptureSource::AllDisplays(),
      future.GetCallback(), GetContext());
  ASSERT_TRUE(overlay);
  ASSERT_EQ(overlay->widgets_for_testing().size(), 2u);
  EXPECT_EQ(overlay->widgets_for_testing()[0]->GetWindowBoundsInScreen(),
            gfx::Rect(100, 100, 800, 600));
  EXPECT_EQ(overlay->widgets_for_testing()[1]->GetWindowBoundsInScreen(),
            gfx::Rect(900, 100, 1024, 768));
}

// Verifies that when a specific display is requested but cannot be found (e.g.
// disconnected), GetOverlayBoundsForSource falls back to the primary display.
TEST_F(OmniboxEverywhereRegionSelectOverlayTest,
       ForDisplay_DisconnectedDisplayFallsBackToPrimary) {
  // Use non-zero origin (100, 100) away from edges to avoid OS menu bar
  // clamping on macOS (where y=0 at the top of the display is adjusted down to
  // y=30 for frameless top-level windows).
  SetDisplays({display::Display(1, gfx::Rect(100, 100, 800, 600))});

  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      CreateTestBitmap(800, 600), RegionCaptureSource::ForDisplay(900),
      future.GetCallback(), GetContext());
  ASSERT_TRUE(overlay);
  ASSERT_TRUE(overlay->GetActiveWidgetForTesting());
  EXPECT_EQ(overlay->GetActiveWidgetForTesting()->GetWindowBoundsInScreen(),
            gfx::Rect(100, 100, 800, 600));
}

TEST_F(OmniboxEverywhereRegionSelectOverlayTest,
       SingleDisplayMatch_MatchesTargetDisplay) {
  SetDisplays({display::Display(1, gfx::Rect(100, 100, 800, 600)),
               display::Display(2, gfx::Rect(900, 100, 1024, 768))});

  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      CreateTestBitmap(1024, 768), RegionCaptureSource::ForDisplay(2),
      future.GetCallback(), GetContext());
  ASSERT_TRUE(overlay);
  ASSERT_TRUE(overlay->GetActiveWidgetForTesting());
  EXPECT_EQ(overlay->GetActiveWidgetForTesting()->GetWindowBoundsInScreen(),
            gfx::Rect(900, 100, 1024, 768));
}

// Verifies that RegionCaptureSource::ForDisplay correctly targets and scales to
// a display in portrait orientation.
TEST_F(OmniboxEverywhereRegionSelectOverlayTest,
       ForDisplay_MatchesPortraitDisplayBounds) {
  SetDisplays({display::Display(1, gfx::Rect(100, 100, 1080, 1920))});

  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      CreateTestBitmap(1080, 1920), RegionCaptureSource::ForDisplay(1),
      future.GetCallback(), GetContext());
  ASSERT_TRUE(overlay);
  ASSERT_TRUE(overlay->GetActiveWidgetForTesting());
  EXPECT_EQ(overlay->GetActiveWidgetForTesting()->GetWindowBoundsInScreen(),
            gfx::Rect(100, 100, 1080, 1920));
}

TEST_F(OmniboxEverywhereRegionSelectOverlayTest,
       GestureDragSelectReturnsCroppedBitmap) {
  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      CreateTestBitmap(), RegionCaptureSource::AllDisplays(),
      future.GetCallback(), GetContext());
  ASSERT_TRUE(overlay);

  SimulateGestureDrag(overlay->GetActiveWidgetForTesting()->GetContentsView(),
                      gfx::Point(10, 10), gfx::Point(70, 70));

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
  SimulateMouseDrag(overlay->GetActiveWidgetForTesting()->GetContentsView(),
                    gfx::Point(50, 50), gfx::Point(150, 150));

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
  SimulateMouseDrag(overlay->GetActiveWidgetForTesting()->GetContentsView(),
                    gfx::Point(60, 60), gfx::Point(10, 10));

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
  SimulateMouseDrag(overlay->GetActiveWidgetForTesting()->GetContentsView(),
                    gfx::Point(50, 50), gfx::Point(-50, -50));

  EXPECT_TRUE(future.IsReady());
  EXPECT_FALSE(future.Get().empty());
  // Cropped area should be the intersection (0, 0) -> (50, 50), width=50,
  // height=50.
  EXPECT_EQ(future.Get().width(), 50);
  EXPECT_EQ(future.Get().height(), 50);
  EXPECT_EQ(future.Get().getColor(0, 0), SK_ColorBLUE);
}

TEST_F(OmniboxEverywhereRegionSelectOverlayTest,
       SmallSelectionAboveThresholdClampsCornersAndReturnsBitmap) {
  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      CreateTestBitmap(), RegionCaptureSource::AllDisplays(),
      future.GetCallback(), GetContext());
  ASSERT_TRUE(overlay);

  // Drag 14x14 rectangle (above 10px min size, below 20px default corner
  // radius).
  SimulateMouseDrag(overlay->GetActiveWidgetForTesting()->GetContentsView(),
                    gfx::Point(10, 10), gfx::Point(24, 24));

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

  views::View* contents_view =
      overlay->GetActiveWidgetForTesting()->GetContentsView();
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

TEST_F(OmniboxEverywhereRegionSelectOverlayTest,
       MouseMoveUpdatesHoverStateAndPaints) {
  SetDisplays({display::Display(1, gfx::Rect(0, 0, 200, 200))});
  SkBitmap bitmap;
  bitmap.allocN32Pixels(200, 200);
  bitmap.eraseColor(SK_ColorGREEN);

  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      bitmap, RegionCaptureSource::AllDisplays(), future.GetCallback(),
      GetContext());
  ASSERT_TRUE(overlay);
  ASSERT_TRUE(overlay->GetActiveWidgetForTesting());

  views::View* contents_view =
      overlay->GetActiveWidgetForTesting()->GetContentsView();
  ASSERT_TRUE(contents_view);

  // Mouse move updates hover cursor position and triggers repaint.
  ui::MouseEvent move_event(ui::EventType::kMouseMoved, gfx::Point(50, 50),
                            gfx::Point(50, 50), base::TimeTicks::Now(), 0, 0);
  contents_view->OnMouseMoved(move_event);

  // Paint during hover state (rendering rainbow gradient wash and teardrop
  // chip).
  SkBitmap hover_painted = views::test::PaintViewToBitmap(
      overlay->GetActiveWidgetForTesting()->GetRootView());
  EXPECT_FALSE(hover_painted.empty());

  // Mouse press and drag to create selection.
  ui::MouseEvent press_event(ui::EventType::kMousePressed, gfx::Point(20, 20),
                             gfx::Point(20, 20), base::TimeTicks::Now(),
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  contents_view->OnMousePressed(press_event);

  ui::MouseEvent drag_event(ui::EventType::kMouseDragged, gfx::Point(120, 120),
                            gfx::Point(120, 120), base::TimeTicks::Now(),
                            ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  contents_view->OnMouseDragged(drag_event);

  // Paint during drag selection state (rendering rounded punch-out and
  // continuous perimeter).
  SkBitmap drag_painted = views::test::PaintViewToBitmap(
      overlay->GetActiveWidgetForTesting()->GetRootView());
  EXPECT_FALSE(drag_painted.empty());

  // Release mouse to complete selection.
  ui::MouseEvent release_event(ui::EventType::kMouseReleased,
                               gfx::Point(120, 120), gfx::Point(120, 120),
                               base::TimeTicks::Now(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  contents_view->OnMouseReleased(release_event);

  EXPECT_TRUE(future.IsReady());
  EXPECT_FALSE(future.Get().empty());
  EXPECT_EQ(future.Get().width(), 100);
  EXPECT_EQ(future.Get().height(), 100);
  EXPECT_EQ(future.Get().getColor(0, 0), SK_ColorGREEN);
}

TEST_F(OmniboxEverywhereRegionSelectOverlayTest,
       ChildViewsAndToastPositioning) {
  SetDisplays({display::Display(1, gfx::Rect(0, 0, 800, 600))});
  SkBitmap bitmap;
  bitmap.allocN32Pixels(800, 600);
  bitmap.eraseColor(SK_ColorBLUE);

  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      bitmap, RegionCaptureSource::AllDisplays(), future.GetCallback(),
      GetContext());
  ASSERT_TRUE(overlay);
  ASSERT_TRUE(overlay->GetActiveWidgetForTesting());

  views::View* contents_view =
      overlay->GetActiveWidgetForTesting()->GetContentsView();
  ASSERT_TRUE(contents_view);

  // Contains toast chip and cursor chip child views.
  EXPECT_EQ(contents_view->children().size(), 2u);

  views::View* toast_chip = contents_view->children()[0];
  views::View* cursor_chip = contents_view->children()[1];

  ASSERT_TRUE(toast_chip);
  ASSERT_TRUE(cursor_chip);

  // Toast chip is visible and positioned at the top of the display.
  EXPECT_TRUE(toast_chip->GetVisible());
  EXPECT_GT(toast_chip->width(), 0);
  EXPECT_EQ(toast_chip->y(), kExpectedTopMargin);
  EXPECT_EQ(
      toast_chip->x(),
      (overlay->GetActiveWidgetForTesting()->GetWindowBoundsInScreen().width() -
       toast_chip->width()) /
          2);

  // Cursor chip is initially hidden before mouse enters.
  EXPECT_FALSE(cursor_chip->GetVisible());

  // Moving mouse makes cursor chip visible and positions it near the cursor.
  ui::MouseEvent move_event(ui::EventType::kMouseMoved, gfx::Point(100, 100),
                            gfx::Point(100, 100), base::TimeTicks::Now(), 0, 0);
  contents_view->OnMouseMoved(move_event);

  EXPECT_TRUE(cursor_chip->GetVisible());
  EXPECT_EQ(cursor_chip->width(), 56);
  EXPECT_EQ(cursor_chip->height(), 56);
  EXPECT_EQ(cursor_chip->x(), 108);
  EXPECT_EQ(cursor_chip->y(), 108);

  // Moving mouse near bottom-right edge flips cursor chip.
  ui::MouseEvent move_edge(ui::EventType::kMouseMoved, gfx::Point(780, 580),
                           gfx::Point(780, 580), base::TimeTicks::Now(), 0, 0);
  contents_view->OnMouseMoved(move_edge);
  EXPECT_TRUE(cursor_chip->GetVisible());
  EXPECT_LT(cursor_chip->x(), 780);
  EXPECT_LT(cursor_chip->y(), 580);

  // Mouse leaving hides the cursor chip.
  ui::MouseEvent exit_event(ui::EventType::kMouseExited, gfx::Point(),
                            gfx::Point(), base::TimeTicks::Now(), 0, 0);
  contents_view->OnMouseExited(exit_event);
  EXPECT_FALSE(cursor_chip->GetVisible());

  // Mouse press starts drag and hides both toast and cursor chips.
  ui::MouseEvent press_event(ui::EventType::kMousePressed, gfx::Point(50, 50),
                             gfx::Point(50, 50), base::TimeTicks::Now(),
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  contents_view->OnMousePressed(press_event);
  EXPECT_FALSE(toast_chip->GetVisible());
  EXPECT_FALSE(cursor_chip->GetVisible());
}

TEST_F(OmniboxEverywhereRegionSelectOverlayTest,
       ToastPositioning_MultiDisplayNegativeCoordinates) {
  display::Display display1(1, gfx::Rect(0, 0, 800, 600));
  display::Display display2(2, gfx::Rect(-800, 0, 800, 600));
  SetDisplays({display1, display2});

  SkBitmap bitmap;
  bitmap.allocN32Pixels(1600, 600);
  bitmap.eraseColor(SK_ColorBLUE);

  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      bitmap, RegionCaptureSource::AllDisplays(), future.GetCallback(),
      GetContext());
  ASSERT_TRUE(overlay);
  ASSERT_EQ(overlay->widgets_for_testing().size(), 2u);

  // Both widgets have an instruction toast chip visible and centered on its
  // display.
  for (const auto& w : overlay->widgets_for_testing()) {
    ASSERT_TRUE(w);
    views::View* contents_view = w->GetContentsView();
    ASSERT_TRUE(contents_view);
    ASSERT_GE(contents_view->children().size(), 1u);
    views::View* toast_chip = contents_view->children()[0];
    ASSERT_TRUE(toast_chip);
    EXPECT_TRUE(toast_chip->GetVisible());
    EXPECT_EQ(toast_chip->y(), kExpectedTopMargin);
    EXPECT_EQ(toast_chip->x(),
              (contents_view->width() - toast_chip->width()) / 2);
  }
}

TEST_F(OmniboxEverywhereRegionSelectOverlayTest,
       ToastPositioning_PortraitPrimaryAndLandscapeSecondaryMixedDpi) {
  display::Display display1(1, gfx::Rect(0, 0, 1440, 2560));
  display1.set_device_scale_factor(1.5f);

  display::Display display2(2, gfx::Rect(1440, 864, 2048, 1153));
  display2.set_device_scale_factor(1.25f);

  SetDisplays({display1, display2});

  SkBitmap bitmap;
  bitmap.allocN32Pixels(4720, 3840);
  bitmap.eraseColor(SK_ColorBLUE);

  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      bitmap, RegionCaptureSource::AllDisplays(), future.GetCallback(),
      GetContext());
  ASSERT_TRUE(overlay);
  ASSERT_EQ(overlay->widgets_for_testing().size(), 2u);

  // Widget 1 on portrait primary monitor.
  views::View* contents_view1 =
      overlay->widgets_for_testing()[0]->GetContentsView();
  ASSERT_TRUE(contents_view1);
  views::View* toast1 = contents_view1->children()[0];
  ASSERT_TRUE(toast1);
  EXPECT_TRUE(toast1->GetVisible());
  EXPECT_EQ(toast1->y(), kExpectedTopMargin);
  EXPECT_EQ(toast1->x(), (1440 - toast1->width()) / 2);

  // Widget 2 on landscape secondary monitor.
  views::View* contents_view2 =
      overlay->widgets_for_testing()[1]->GetContentsView();
  ASSERT_TRUE(contents_view2);
  views::View* toast2 = contents_view2->children()[0];
  ASSERT_TRUE(toast2);
  EXPECT_TRUE(toast2->GetVisible());
  EXPECT_EQ(toast2->y(), kExpectedTopMargin);
  EXPECT_EQ(toast2->x(), (2048 - toast2->width()) / 2);
}

TEST_F(OmniboxEverywhereRegionSelectOverlayTest,
       MultiDisplay_CloseOnOneWidgetClosesAllInUnison) {
  SetDisplays({display::Display(1, gfx::Rect(0, 0, 800, 600)),
               display::Display(2, gfx::Rect(800, 0, 800, 600))});

  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      CreateTestBitmap(1600, 600), RegionCaptureSource::AllDisplays(),
      future.GetCallback(), GetContext());
  ASSERT_TRUE(overlay);
  ASSERT_EQ(overlay->widgets_for_testing().size(), 2u);

  // Closing the first widget closes all widgets in unison.
  overlay->widgets_for_testing()[0]->CloseWithReason(
      views::Widget::ClosedReason::kEscKeyPressed);

  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
  EXPECT_TRUE(overlay->widgets_for_testing()[0]->IsClosed());
  EXPECT_TRUE(overlay->widgets_for_testing()[1]->IsClosed());
}

TEST_F(OmniboxEverywhereRegionSelectOverlayTest,
       MultiDisplay_DragSelectionOnSecondaryMonitorConfirmsAndClosesAll) {
  SetDisplays({display::Display(1, gfx::Rect(0, 0, 800, 600)),
               display::Display(2, gfx::Rect(800, 0, 800, 600))});

  // Create a 1600x600 bitmap: left half green, right half blue.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1600, 600);
  bitmap.eraseColor(SK_ColorGREEN);
  for (int x = 800; x < 1600; ++x) {
    for (int y = 0; y < 600; ++y) {
      *bitmap.getAddr32(x, y) = SK_ColorBLUE;
    }
  }

  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      bitmap, RegionCaptureSource::AllDisplays(), future.GetCallback(),
      GetContext());
  ASSERT_TRUE(overlay);
  ASSERT_EQ(overlay->widgets_for_testing().size(), 2u);

  // Drag select a 50x50 box on the secondary monitor (widget 1).
  views::View* contents_view1 =
      overlay->widgets_for_testing()[1]->GetContentsView();
  ASSERT_TRUE(contents_view1);

  SimulateMouseDrag(contents_view1, gfx::Point(10, 10), gfx::Point(60, 60));

  EXPECT_TRUE(future.IsReady());
  EXPECT_FALSE(future.Get().empty());
  EXPECT_EQ(future.Get().width(), 50);
  EXPECT_EQ(future.Get().height(), 50);
  EXPECT_EQ(future.Get().getColor(0, 0), SK_ColorBLUE);

  // Both widgets closed in unison.
  EXPECT_TRUE(overlay->widgets_for_testing()[0]->IsClosed());
  EXPECT_TRUE(overlay->widgets_for_testing()[1]->IsClosed());
}

TEST_F(OmniboxEverywhereRegionSelectOverlayTest,
       MultiDisplay_BitmapSliceVerifiedPerDisplay) {
  SetDisplays({display::Display(1, gfx::Rect(0, 0, 800, 600)),
               display::Display(2, gfx::Rect(800, 0, 800, 600))});

  // Create a 1600x600 bitmap: left half Red, right half Blue.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1600, 600);
  bitmap.eraseColor(SK_ColorRED);
  for (int x = 800; x < 1600; ++x) {
    for (int y = 0; y < 600; ++y) {
      *bitmap.getAddr32(x, y) = SK_ColorBLUE;
    }
  }

  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      bitmap, RegionCaptureSource::AllDisplays(), future.GetCallback(),
      GetContext());
  ASSERT_TRUE(overlay);
  ASSERT_EQ(overlay->widgets_for_testing().size(), 2u);

  // Widget 0 received the 800x600 Red slice.
  const SkBitmap& slice0 = overlay->GetBitmapForWidgetForTesting(0);
  EXPECT_EQ(slice0.width(), 800);
  EXPECT_EQ(slice0.height(), 600);
  EXPECT_EQ(slice0.getColor(0, 0), SK_ColorRED);
  EXPECT_EQ(slice0.getColor(799, 599), SK_ColorRED);

  // Widget 1 received the 800x600 Blue slice.
  const SkBitmap& slice1 = overlay->GetBitmapForWidgetForTesting(1);
  EXPECT_EQ(slice1.width(), 800);
  EXPECT_EQ(slice1.height(), 600);
  EXPECT_EQ(slice1.getColor(0, 0), SK_ColorBLUE);
  EXPECT_EQ(slice1.getColor(799, 599), SK_ColorBLUE);
}

TEST_F(OmniboxEverywhereRegionSelectOverlayTest,
       EmptyDisplays_EarlyExitDoesNotCrashOrLeak) {
  SetDisplays({});

  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      CreateTestBitmap(100, 100), RegionCaptureSource::AllDisplays(),
      future.GetCallback(), GetContext());
  ASSERT_TRUE(overlay);
  EXPECT_TRUE(overlay->widgets_for_testing().empty());
  EXPECT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get().empty());
}

TEST_F(OmniboxEverywhereRegionSelectOverlayTest,
       MultiDisplay_OnlyWidgetWithCursorIsActive) {
  SetDisplays({display::Display(1, gfx::Rect(0, 0, 800, 600)),
               display::Display(2, gfx::Rect(800, 0, 800, 600))});

  // Position cursor on secondary display (Display 2).
  SetCursorScreenPoint(gfx::Point(1000, 300));

  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      CreateTestBitmap(1600, 600), RegionCaptureSource::AllDisplays(),
      future.GetCallback(), GetContext());
  ASSERT_TRUE(overlay);
  ASSERT_EQ(overlay->widgets_for_testing().size(), 2u);

  // Widget 1 (Display 2) contains the cursor and should be the active widget.
  EXPECT_EQ(overlay->GetActiveWidgetForTesting(),
            overlay->widgets_for_testing()[1].get());
  EXPECT_TRUE(overlay->widgets_for_testing()[1]->IsActive());
  EXPECT_FALSE(overlay->widgets_for_testing()[0]->IsActive());
}

}  // namespace omnibox_everywhere
