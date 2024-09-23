// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/video_overlay_window_views.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/picture_in_picture/auto_pip_setting_overlay_view.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_tracker.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/ui/views/overlay/back_to_tab_button.h"
#include "chrome/browser/ui/views/overlay/close_image_button.h"
#include "chrome/browser/ui/views/overlay/minimize_button.h"
#include "chrome/browser/ui/views/overlay/simple_overlay_window_image_button.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/video_picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_web_contents_factory.h"
#include "media/base/media_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/compositor/layer.h"
#include "ui/display/test/test_screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget_utils.h"

namespace {

constexpr gfx::Size kMinWindowSize(200, 100);

// Reported minimum bubble size for the setting view.  The pip window should be
// larger than this when the setting view is shown.
constexpr gfx::Size kBubbleSize(300, 200);

// Size that's big enough to accommodate a `kBubbleSize`-sized bubble without
// being further adjusted upwards for margin.
constexpr gfx::Size kSizeBigEnoughForBubble(400, 300);

}  // namespace

using testing::_;
using ::testing::Return;

// Mock of AutoPipSettingOverlayView. Used for injection during tests.
class MockOverlayView : public AutoPipSettingOverlayView {
 public:
  explicit MockOverlayView(views::View* anchor_view)
      : AutoPipSettingOverlayView(base::DoNothing(),
                                  GURL{"https://example.com"},
                                  anchor_view,
                                  views::BubbleBorder::Arrow::FLOAT) {}
  MOCK_METHOD(void, ShowBubble, (gfx::NativeView parent), (override));

  void SetWantsEvent(bool wants_event) { wants_event_ = wants_event; }

  bool WantsEvent(const gfx::Point& point_in_screen) override {
    // Consume any event we're given.  The goal is to make sure we're given the
    // opportunity to take an event.
    return wants_event_;
  }

  gfx::Size GetBubbleSize() const override {
    // Return something that's bigger than the minimum.
    return kBubbleSize;
  }

 private:
  bool wants_event_ = false;
};

class TestVideoPictureInPictureWindowController
    : public content::VideoPictureInPictureWindowController {
 public:
  TestVideoPictureInPictureWindowController() = default;

  // PictureInPictureWindowController:
  void Show() override {}
  void FocusInitiator() override {}
  MOCK_METHOD(void, Close, (bool));
  MOCK_METHOD(void, CloseAndFocusInitiator, ());
  MOCK_METHOD(void, OnWindowDestroyed, (bool));
  content::VideoOverlayWindow* GetWindowForTesting() override {
    return nullptr;
  }
  void UpdateLayerBounds() override {}
  bool IsPlayerActive() override { return false; }
  void set_web_contents(content::WebContents* web_contents) {
    web_contents_ = web_contents;
  }
  content::WebContents* GetWebContents() override { return web_contents_; }
  content::WebContents* GetChildWebContents() override { return nullptr; }
  bool TogglePlayPause() override { return false; }
  void SkipAd() override {}
  void NextTrack() override {}
  void PreviousTrack() override {}
  void NextSlide() override {}
  void PreviousSlide() override {}
  void ToggleMicrophone() override {}
  void ToggleCamera() override {}
  void HangUp() override {}
  const gfx::Rect& GetSourceBounds() const override { return source_bounds_; }
  std::optional<gfx::Rect> GetWindowBounds() override { return std::nullopt; }
  std::optional<url::Origin> GetOrigin() override { return std::nullopt; }
  void SetOnWindowCreatedNotifyObserversCallback(base::OnceClosure) override {}

 private:
  raw_ptr<content::WebContents> web_contents_;
  gfx::Rect source_bounds_;
};

class VideoOverlayWindowViewsTest : public ChromeViewsTestBase {
 public:
  VideoOverlayWindowViewsTest() = default;
  // ChromeViewsTestBase:
  void SetUp() override {
    enabled_features_.push_back(media::kPictureInPictureOcclusionTracking);
    feature_list_.InitWithFeatures(enabled_features_, {});
    display::Screen::SetScreenInstance(&test_screen_);

    // Purposely skip ChromeViewsTestBase::SetUp() as that creates ash::Shell
    // on ChromeOS, which we don't want.
    ViewsTestBase::SetUp();
    // web_contents_ needs to be created after the constructor, so that
    // |feature_list_| can be initialized before other threads check if a
    // feature is enabled.
    web_contents_ = web_contents_factory_.CreateWebContents(&profile_);
    pip_window_controller_.set_web_contents(web_contents_);

#if BUILDFLAG(IS_CHROMEOS)
    test_views_delegate()->set_context(GetContext());
#endif
    test_views_delegate()->set_use_desktop_native_widgets(true);

    // The default work area must be big enough to fit the minimum
    // VideoOverlayWindowViews size.
    SetDisplayWorkArea({0, 0, 1000, 1000});

    overlay_window_ = VideoOverlayWindowViews::Create(&pip_window_controller_);
    overlay_window_->set_overlay_view_cb_for_testing(
        base::BindRepeating(&VideoOverlayWindowViewsTest::GetOverlayViewImpl,
                            base::Unretained(this)));

    // On some platforms, OnNativeWidgetMove is invoked on creation.
    WaitForMove();
    overlay_window_->set_minimum_size_for_testing(kMinWindowSize);

    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        views::GetRootWindow(overlay_window_.get()));
  }

  void TearDown() override {
    overlay_window_.reset();
    ViewsTestBase::TearDown();
    display::Screen::SetScreenInstance(nullptr);
  }

  void SetDisplayWorkArea(const gfx::Rect& work_area) {
    display::Display display = test_screen_.GetPrimaryDisplay();
    display.set_work_area(work_area);
    test_screen_.display_list().UpdateDisplay(display);
  }

  VideoOverlayWindowViews& overlay_window() { return *overlay_window_; }

  content::WebContents* web_contents() { return web_contents_; }

  TestVideoPictureInPictureWindowController& pip_window_controller() {
    return pip_window_controller_;
  }

  MockOverlayView* SetOverlayView() {
    std::unique_ptr<MockOverlayView> mock_overlay_view =
        std::make_unique<MockOverlayView>(
            overlay_window().window_background_view_for_testing());
    overlay_view_ = std::move(mock_overlay_view);
    return overlay_view_.get();
  }

  ui::test::EventGenerator* event_generator() { return event_generator_.get(); }

 protected:
  void WaitForMove() {
    task_environment()->FastForwardBy(
        VideoOverlayWindowViews::kControlHideDelayAfterMove +
        base::Milliseconds(1));
  }

  void DestroyOverlayWindow() { overlay_window_.reset(); }

  void AddEnabledFeature(base::test::FeatureRef feature) {
    enabled_features_.push_back(feature);
  }

 private:
  std::unique_ptr<AutoPipSettingOverlayView> GetOverlayViewImpl() {
    return std::move(overlay_view_);
  }

  TestingProfile profile_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_;
  TestVideoPictureInPictureWindowController pip_window_controller_;

  display::test::TestScreen test_screen_;

  // Overlay view that we'll send to the window.  May be null.
  std::unique_ptr<MockOverlayView> overlay_view_;

  std::unique_ptr<ui::test::EventGenerator> event_generator_;

  std::unique_ptr<VideoOverlayWindowViews> overlay_window_;

  std::vector<base::test::FeatureRef> enabled_features_;

  base::test::ScopedFeatureList feature_list_;
};

TEST_F(VideoOverlayWindowViewsTest, InitialWindowSize_Square) {
  // Fit the window taking 1/5 (both dimensions) of the work area as the
  // starting size, and applying the size and aspect ratio constraints.
  overlay_window().UpdateNaturalSize({400, 400});
  EXPECT_EQ(gfx::Size(200, 200), overlay_window().GetBounds().size());
  EXPECT_EQ(gfx::Size(200, 200),
            overlay_window().video_layer_for_testing()->size());
}

TEST_F(VideoOverlayWindowViewsTest, InitialWindowSize_Horizontal) {
  // Fit the window taking 1/5 (both dimensions) of the work area as the
  // starting size, and applying the size and aspect ratio constraints.
  overlay_window().UpdateNaturalSize({400, 200});
  EXPECT_EQ(gfx::Size(400, 200), overlay_window().GetBounds().size());
  EXPECT_EQ(gfx::Size(400, 200),
            overlay_window().video_layer_for_testing()->size());
}

TEST_F(VideoOverlayWindowViewsTest, InitialWindowSize_Vertical) {
  // Fit the window taking 1/5 (both dimensions) of the work area as the
  // starting size, and applying the size and aspect ratio constraints.
  overlay_window().UpdateNaturalSize({400, 500});
  EXPECT_EQ(gfx::Size(200, 250), overlay_window().GetBounds().size());
  EXPECT_EQ(gfx::Size(200, 250),
            overlay_window().video_layer_for_testing()->size());
}

TEST_F(VideoOverlayWindowViewsTest, Letterboxing) {
  overlay_window().UpdateNaturalSize({400, 10});

  // Must fit within the minimum height of 146. But with the aspect ratio of
  // 40:1 the width gets exceedingly big and must be limited to the maximum of
  // 800. Thus, letterboxing is unavoidable.
  EXPECT_EQ(gfx::Size(800, 100), overlay_window().GetBounds().size());
  EXPECT_EQ(gfx::Size(800, 20),
            overlay_window().video_layer_for_testing()->size());
}

TEST_F(VideoOverlayWindowViewsTest, Pillarboxing) {
  overlay_window().UpdateNaturalSize({10, 400});

  // Must fit within the minimum width of 260. But with the aspect ratio of
  // 1:40 the height gets exceedingly big and must be limited to the maximum of
  // 800. Thus, pillarboxing is unavoidable.
  EXPECT_EQ(gfx::Size(200, 800), overlay_window().GetBounds().size());
  EXPECT_EQ(gfx::Size(20, 800),
            overlay_window().video_layer_for_testing()->size());
}

TEST_F(VideoOverlayWindowViewsTest, Pillarboxing_Square) {
  overlay_window().UpdateNaturalSize({100, 100});

  // Pillarboxing also occurs on Linux even with the square aspect ratio,
  // because the user is allowed to size the window to the rectangular minimum
  // size.
  overlay_window().SetSize({200, 100});
  EXPECT_EQ(gfx::Size(100, 100),
            overlay_window().video_layer_for_testing()->size());
}

TEST_F(VideoOverlayWindowViewsTest, ApproximateAspectRatio_Horizontal) {
  // "Horizontal" video.
  overlay_window().UpdateNaturalSize({320, 240});

  // The user drags the window resizer horizontally and now the integer window
  // dimensions can't reproduce the video aspect ratio exactly. The video
  // should still fill the entire window area.
  overlay_window().SetSize({320, 240});
  EXPECT_EQ(gfx::Size(320, 240),
            overlay_window().video_layer_for_testing()->size());

  overlay_window().SetSize({321, 241});
  EXPECT_EQ(gfx::Size(321, 241),
            overlay_window().video_layer_for_testing()->size());

  // Wide video.
  overlay_window().UpdateNaturalSize({1600, 900});

  overlay_window().SetSize({444, 250});
  EXPECT_EQ(gfx::Size(444, 250),
            overlay_window().video_layer_for_testing()->size());

  overlay_window().SetSize({445, 250});
  EXPECT_EQ(gfx::Size(445, 250),
            overlay_window().video_layer_for_testing()->size());

  // Very wide video.
  overlay_window().UpdateNaturalSize({400, 100});

  overlay_window().SetSize({478, 120});
  EXPECT_EQ(gfx::Size(478, 120),
            overlay_window().video_layer_for_testing()->size());

  overlay_window().SetSize({481, 120});
  EXPECT_EQ(gfx::Size(481, 120),
            overlay_window().video_layer_for_testing()->size());
}

TEST_F(VideoOverlayWindowViewsTest, ApproximateAspectRatio_Vertical) {
  // "Vertical" video.
  overlay_window().UpdateNaturalSize({240, 320});

  // The user dragged the window resizer vertically and now the integer window
  // dimensions can't reproduce the video aspect ratio exactly. The video
  // should still fill the entire window area.
  overlay_window().SetSize({240, 320});
  EXPECT_EQ(gfx::Size(240, 320),
            overlay_window().video_layer_for_testing()->size());

  overlay_window().SetSize({239, 319});
  EXPECT_EQ(gfx::Size(239, 319),
            overlay_window().video_layer_for_testing()->size());

  // Narrow video.
  overlay_window().UpdateNaturalSize({900, 1600});

  overlay_window().SetSize({250, 444});
  EXPECT_EQ(gfx::Size(250, 444),
            overlay_window().video_layer_for_testing()->size());

  overlay_window().SetSize({250, 445});
  EXPECT_EQ(gfx::Size(250, 445),
            overlay_window().video_layer_for_testing()->size());

  // Very narrow video.
  // NOTE: Window width is bounded by the minimum size.
  overlay_window().UpdateNaturalSize({100, 400});

  overlay_window().SetSize({200, 478});
  EXPECT_EQ(gfx::Size(120, 478),
            overlay_window().video_layer_for_testing()->size());

  overlay_window().SetSize({200, 481});
  EXPECT_EQ(gfx::Size(120, 481),
            overlay_window().video_layer_for_testing()->size());
}

TEST_F(VideoOverlayWindowViewsTest, UpdateMaximumSize) {
  SetDisplayWorkArea({0, 0, 4000, 4000});

  overlay_window().UpdateNaturalSize({480, 320});

  // The initial size is determined by the work area and the video natural size
  // (aspect ratio).
  EXPECT_EQ(gfx::Size(1200, 800), overlay_window().GetBounds().size());
  // The initial maximum size is 80% of the work area.
  EXPECT_EQ(gfx::Size(3200, 3200), overlay_window().GetMaximumSize());

  // If the maximum size increases then we should keep the existing window size.
  SetDisplayWorkArea({0, 0, 8000, 8000});
  EXPECT_EQ(gfx::Size(1200, 800), overlay_window().GetBounds().size());
  EXPECT_EQ(gfx::Size(6400, 6400), overlay_window().GetMaximumSize());

  // If the maximum size decreases then we should shrink to fit.
  SetDisplayWorkArea({0, 0, 1000, 2000});
  EXPECT_EQ(gfx::Size(800, 800), overlay_window().GetBounds().size());
  EXPECT_EQ(gfx::Size(800, 1600), overlay_window().GetMaximumSize());
}

TEST_F(VideoOverlayWindowViewsTest, IgnoreInvalidMaximumSize) {
  ASSERT_EQ(gfx::Size(800, 800), overlay_window().GetMaximumSize());

  SetDisplayWorkArea({0, 0, 0, 0});
  EXPECT_EQ(gfx::Size(800, 800), overlay_window().GetMaximumSize());
}

// Tests that Next Track button bounds are updated right away when window
// controls are hidden.
TEST_F(VideoOverlayWindowViewsTest, NextTrackButtonAddedWhenControlsHidden) {
  ASSERT_FALSE(overlay_window().AreControlsVisible());
  ASSERT_TRUE(overlay_window()
                  .next_track_controls_view_for_testing()
                  ->size()
                  .IsEmpty());

  const auto origin_before_layout =
      overlay_window().next_track_controls_view_for_testing()->origin();

  overlay_window().SetNextTrackButtonVisibility(true);
  EXPECT_NE(overlay_window().next_track_controls_view_for_testing()->origin(),
            origin_before_layout);
  EXPECT_FALSE(overlay_window().IsLayoutPendingForTesting());
}

// Tests that Previous Track button bounds are updated right away when window
// controls are hidden.
TEST_F(VideoOverlayWindowViewsTest,
       PreviousTrackButtonAddedWhenControlsHidden) {
  ASSERT_FALSE(overlay_window().AreControlsVisible());
  ASSERT_TRUE(overlay_window()
                  .previous_track_controls_view_for_testing()
                  ->size()
                  .IsEmpty());

  const auto origin_before_layout =
      overlay_window().previous_track_controls_view_for_testing()->origin();

  overlay_window().SetPreviousTrackButtonVisibility(true);
  EXPECT_NE(
      overlay_window().previous_track_controls_view_for_testing()->origin(),
      origin_before_layout);
  EXPECT_FALSE(overlay_window().IsLayoutPendingForTesting());
}

TEST_F(VideoOverlayWindowViewsTest, UpdateNaturalSizeDoesNotMoveWindow) {
  // Enter PiP.
  overlay_window().UpdateNaturalSize({300, 200});
  overlay_window().ShowInactive();

  // Resize the window and move it toward the top-left corner of the work area.
  // In production, resizing preserves the aspect ratio if possible, so we
  // preserve it here too.
  overlay_window().SetBounds({100, 100, 450, 300});

  // Simulate a new surface layer and a change in the aspect ratio.
  overlay_window().UpdateNaturalSize({400, 200});

  // The window should not move.
  // The window size will be adjusted according to the new aspect ratio, and
  // clamped to 600x300 to fit within the maximum size for the work area of
  // 1000x1000.
  EXPECT_EQ(gfx::Rect(100, 100, 600, 300), overlay_window().GetBounds());
}

// Tests that the OverlayWindowFrameView does not accept events so they can
// propagate to the overlay.
TEST_F(VideoOverlayWindowViewsTest, HitTestFrameView) {
  // Since the NonClientFrameView is the only non-custom direct descendent of
  // the NonClientView, we can assume that if the frame does not accept the
  // point but the NonClientView does, then it will be handled by one of the
  // custom overlay views.
  auto point = gfx::Point(50, 50);
  views::NonClientView* non_client_view = overlay_window().non_client_view();
  EXPECT_EQ(non_client_view->frame_view()->HitTestPoint(point), false);
  EXPECT_EQ(non_client_view->HitTestPoint(point), true);
}

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
// With pillarboxing, the close button doesn't cover the video area. Make sure
// hovering the button doesn't get handled like normal mouse exit events
// causing the controls to hide.
// TODO(http://crbug/1509791): Fix and re-enable.
TEST_F(VideoOverlayWindowViewsTest, DISABLED_NoMouseExitWithinWindowBounds) {
  overlay_window().UpdateNaturalSize({10, 400});
  WaitForMove();

  const auto close_button_bounds = overlay_window().GetCloseControlsBounds();
  const auto video_bounds =
      overlay_window().video_layer_for_testing()->bounds();
  ASSERT_FALSE(video_bounds.Contains(close_button_bounds));

  const gfx::Point moved_location(video_bounds.origin() + gfx::Vector2d(5, 5));
  ui::MouseEvent moved_event(ui::EventType::kMouseMoved, moved_location,
                             moved_location, ui::EventTimeForNow(), ui::EF_NONE,
                             ui::EF_NONE);
  overlay_window().OnMouseEvent(&moved_event);
  ASSERT_TRUE(overlay_window().AreControlsVisible());

  const gfx::Point exited_location(close_button_bounds.CenterPoint());
  ui::MouseEvent exited_event(ui::EventType::kMouseExited, exited_location,
                              exited_location, ui::EventTimeForNow(),
                              ui::EF_NONE, ui::EF_NONE);
  overlay_window().OnMouseEvent(&exited_event);
  EXPECT_TRUE(overlay_window().AreControlsVisible());
}

#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

TEST_F(VideoOverlayWindowViewsTest, ShowControlsOnFocus) {
  EXPECT_FALSE(overlay_window().AreControlsVisible());
  overlay_window().OnNativeFocus();
  EXPECT_TRUE(overlay_window().AreControlsVisible());
}

TEST_F(VideoOverlayWindowViewsTest, OnlyPauseOnCloseWhenPauseIsAvailable) {
  views::test::ButtonTestApi close_button_clicker(
      overlay_window().close_button_for_testing());
  ui::MouseEvent dummy_event(ui::EventType::kMousePressed, gfx::Point(0, 0),
                             gfx::Point(0, 0), ui::EventTimeForNow(), 0, 0);

  // When the play/pause controls are visible, closing via the close button
  // should pause the video.
  overlay_window().SetPlayPauseButtonVisibility(true);
  PictureInPictureWindowManager::GetInstance()
      ->set_window_controller_for_testing(&pip_window_controller());
  EXPECT_CALL(pip_window_controller(), Close(true));
  close_button_clicker.NotifyClick(dummy_event);
  testing::Mock::VerifyAndClearExpectations(&pip_window_controller());

  // Same for tapping the close button. Note that the controls must be visible
  // for the tap to work, otherwise the tap will just show the controls.
  overlay_window().ForceControlsVisibleForTesting(true);
  gfx::Point close_button_center =
      overlay_window().GetCloseControlsBounds().CenterPoint();
  ui::GestureEvent tap_event(
      close_button_center.x(), close_button_center.y(), 0,
      base::TimeTicks::Now(),
      ui::GestureEventDetails(ui::EventType::kGestureTap));
  EXPECT_CALL(pip_window_controller(), Close(true));
  overlay_window().OnGestureEvent(&tap_event);
  testing::Mock::VerifyAndClearExpectations(&pip_window_controller());

  // When the play/pause controls are not visible, closing via the close button
  // should not pause the video.
  overlay_window().SetPlayPauseButtonVisibility(false);
  EXPECT_CALL(pip_window_controller(), Close(false));
  close_button_clicker.NotifyClick(dummy_event);
  testing::Mock::VerifyAndClearExpectations(&pip_window_controller());

  // Same for tapping the close button.
  EXPECT_CALL(pip_window_controller(), Close(false));
  overlay_window().OnGestureEvent(&tap_event);
  testing::Mock::VerifyAndClearExpectations(&pip_window_controller());

  PictureInPictureWindowManager::GetInstance()
      ->set_window_controller_for_testing(nullptr);
}

TEST_F(VideoOverlayWindowViewsTest, PauseOnWidgetCloseWhenPauseAvailable) {
  // When the play/pause controls are visible, when the native widget is
  // destroyed we should pause the underlying video.
  overlay_window().SetPlayPauseButtonVisibility(true);
  EXPECT_CALL(pip_window_controller(), OnWindowDestroyed(true));
  overlay_window().CloseNow();
  testing::Mock::VerifyAndClearExpectations(&pip_window_controller());
}

TEST_F(VideoOverlayWindowViewsTest,
       DontPauseOnWidgetCloseWhenPauseNotAvailable) {
  // When the play/pause controls are not visible, when the native widget is
  // destroyed we should not pause the underlying video.
  overlay_window().SetPlayPauseButtonVisibility(false);
  EXPECT_CALL(pip_window_controller(), OnWindowDestroyed(false));
  overlay_window().CloseNow();
  testing::Mock::VerifyAndClearExpectations(&pip_window_controller());
}

TEST_F(VideoOverlayWindowViewsTest, SmallDisplayWorkAreaDoesNotCrash) {
  SetDisplayWorkArea({0, 0, 240, 120});
  overlay_window().UpdateNaturalSize({400, 300});

  // Since the work area would force a max size smaller than the minimum size,
  // the size is fixed at the minimum size.
  EXPECT_EQ(kMinWindowSize, overlay_window().GetBounds().size());
  EXPECT_EQ(kMinWindowSize, overlay_window().GetMaximumSize());

  // The video should still be letterboxed to the correct aspect ratio.
  EXPECT_EQ(gfx::Size(133, 100),
            overlay_window().video_layer_for_testing()->size());
}

// TODO(http://crbug/1509791): Fix and re-enable.
TEST_F(VideoOverlayWindowViewsTest, DISABLED_ControlsAreHiddenDuringMove) {
  // Set the initial position.
  overlay_window().SetBounds({0, 0, 100, 100});
  WaitForMove();

  // Make the controls visible.
  overlay_window().UpdateControlsVisibility(true);
  ASSERT_TRUE(overlay_window().AreControlsVisible());

  // Now move the window, this should cause the controls to be hidden.
  overlay_window().SetBounds({50, 0, 100, 100});
  EXPECT_FALSE(overlay_window().AreControlsVisible());

  // Should still be hidden with mouse event.
  overlay_window().UpdateControlsVisibility(true);
  EXPECT_FALSE(overlay_window().AreControlsVisible());

  // After moving, overlay should be visible again because of the previous
  // mouse event.
  WaitForMove();
  EXPECT_TRUE(overlay_window().AreControlsVisible());
}

TEST_F(VideoOverlayWindowViewsTest,
       ControlsAreHiddenDuringMove_MultipleUpdates) {
  overlay_window().SetBounds({0, 0, 100, 100});
  WaitForMove();

  // Move the window.
  overlay_window().SetBounds({50, 0, 100, 100});
  EXPECT_FALSE(overlay_window().AreControlsVisible());

  overlay_window().UpdateControlsVisibility(true);
  overlay_window().UpdateControlsVisibility(false);
  overlay_window().UpdateControlsVisibility(true);
  overlay_window().UpdateControlsVisibility(false);

  // Only the last one should have any effect.
  EXPECT_FALSE(overlay_window().AreControlsVisible());
}

TEST_F(VideoOverlayWindowViewsTest, OverlayViewIsSizedCorrectly) {
  // Set the bound of the window before showing it, to make sure the size
  // propagates to the overlay view.  We use the larger-than-bubble size so that
  // it should be an exact match.  If it were too small, then the overlay window
  // might have to be even larger than we request to fit the bubble.

  // Setting the overlay view before show should be sufficient for it to take
  // effect when shown.
  auto* overlay_view = SetOverlayView();
  overlay_window().ShowInactive();
  // Do this after showing it, else the window will size to a default size,
  // rather than the bounds we request.
  const gfx::Rect bounds(gfx::Point(0, 0), kSizeBigEnoughForBubble);
  overlay_window().UpdateNaturalSize(bounds.size());
  overlay_window().SetBounds(bounds);
  EXPECT_TRUE(overlay_view->GetVisible());
  EXPECT_EQ(overlay_view->bounds(), bounds);
}

TEST_F(VideoOverlayWindowViewsTest, OverlayViewCanBeClicked) {
  // Make sure that the overlay view is z-ordered to get input events.
  auto* overlay_view = SetOverlayView();
  overlay_view->SetWantsEvent(true);

  // Add a button!
  base::MockRepeatingCallback<void(const ui::Event&)> cb;
  auto* button = overlay_view->AddChildView(
      std::make_unique<views::LabelButton>(cb.Get()));
  button->SetBounds(0, 0, 50, 50);

  // Show the window and click the button.
  overlay_window().ShowInactive();
  EXPECT_CALL(cb, Run(_));
  event_generator()->MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
  event_generator()->ClickLeftButton();

  // Clear the callback since `cb` is going away.  Note that `DoNothing()`
  // doesn't work here because type inference fails.
  button->SetCallback(base::BindRepeating([](const ui::Event&) {}));
}

TEST_F(VideoOverlayWindowViewsTest, OverlayWindowBlocksInput) {
  // Make sure that the playback controls don't receive input events while the
  // overlay view is visible.
  auto* overlay_view = SetOverlayView();
  overlay_view->SetWantsEvent(true);
  overlay_window().ShowInactive();

  // When the play/pause controls are visible, closing via the close button
  // should pause the video.
  overlay_window().SetPlayPauseButtonVisibility(true);
  EXPECT_CALL(pip_window_controller(), Close(true)).Times(0);
  event_generator()->MoveMouseTo(
      overlay_window().GetCloseControlsBounds().CenterPoint());
  event_generator()->ClickLeftButton();
}

TEST_F(VideoOverlayWindowViewsTest, OverlayWindowFitsInMinimumSize) {
  auto* overlay_view = SetOverlayView();
  overlay_window().ShowInactive();

  // The window size should be strictly greater than the bubble size so that
  // there's some nonzero margin.
  auto window_min_size = overlay_window().GetMinimumSize();
  auto bubble_min_size = overlay_view->GetBubbleSize();
  EXPECT_GT(window_min_size.width(), bubble_min_size.width());
  EXPECT_GT(window_min_size.height(), bubble_min_size.height());

  // When the overlay view is hidden, the minimum size should return to normal.
  overlay_view->SetVisible(false);
  EXPECT_EQ(overlay_window().GetMinimumSize(), kMinWindowSize);
}

TEST_F(VideoOverlayWindowViewsTest, OverlayWindowStopsBlockingInput) {
  auto* overlay_view = SetOverlayView();
  overlay_window().ShowInactive();

  // Make sure that the overlay window blocks input, when the overlay view does
  // not want events.
  const auto close_controls_center_point =
      overlay_window().GetCloseControlsBounds().CenterPoint();
  overlay_view->SetWantsEvent(false);
  EXPECT_FALSE(overlay_window().ControlsHitTestContainsPoint(
      close_controls_center_point));

  // Make sure that the overlay window stops blocking input, when the overlay
  // view wants event.
  overlay_view->SetWantsEvent(true);
  EXPECT_TRUE(overlay_window().ControlsHitTestContainsPoint(
      close_controls_center_point));
}

TEST_F(VideoOverlayWindowViewsTest, IsTrackedByTheOcclusionObserver) {
  overlay_window().ShowInactive();

  PictureInPictureOcclusionTracker* tracker =
      PictureInPictureWindowManager::GetInstance()->GetOcclusionTracker();

  // Check that the PictureInPictureOcclusionTracker is observing the
  // VideoOverlayWindowViews.
  EXPECT_TRUE(base::Contains(tracker->GetPictureInPictureWidgetsForTesting(),
                             &overlay_window()));

  // Check that it's no longer observed when the widget is destroyed.
  DestroyOverlayWindow();
  EXPECT_EQ(0u, tracker->GetPictureInPictureWidgetsForTesting().size());
}

class VideoOverlayWindowViewsWith2024UITest
    : public VideoOverlayWindowViewsTest {
 public:
  void SetUp() override {
    AddEnabledFeature(media::kVideoPictureInPictureControlsUpdate2024);
    VideoOverlayWindowViewsTest::SetUp();
  }
};

TEST_F(VideoOverlayWindowViewsWith2024UITest,
       MinimizeButtonClosesWithoutPausing) {
  views::test::ButtonTestApi minimize_button_clicker(
      overlay_window().minimize_button_for_testing());
  ui::MouseEvent dummy_event(ui::EventType::kMousePressed, gfx::Point(0, 0),
                             gfx::Point(0, 0), ui::EventTimeForNow(), 0, 0);

  // Even when play/pause is available, the minimize button should not pause the
  // video.
  overlay_window().SetPlayPauseButtonVisibility(true);
  PictureInPictureWindowManager::GetInstance()
      ->set_window_controller_for_testing(&pip_window_controller());
  EXPECT_CALL(pip_window_controller(), Close(false));
  minimize_button_clicker.NotifyClick(dummy_event);
  testing::Mock::VerifyAndClearExpectations(&pip_window_controller());
}

TEST_F(VideoOverlayWindowViewsWith2024UITest, ShowsBackToTabImageButton) {
  overlay_window().ForceControlsVisibleForTesting(true);
  OverlayWindowBackToTabButton* back_to_tab_image_button =
      overlay_window().back_to_tab_button_for_testing();
  ASSERT_NE(nullptr, back_to_tab_image_button);
  EXPECT_TRUE(back_to_tab_image_button->IsDrawn());
  views::test::ButtonTestApi button_clicker(back_to_tab_image_button);
  ui::MouseEvent dummy_event(ui::EventType::kMousePressed, gfx::Point(0, 0),
                             gfx::Point(0, 0), ui::EventTimeForNow(), 0, 0);

  PictureInPictureWindowManager::GetInstance()
      ->set_window_controller_for_testing(&pip_window_controller());
  EXPECT_CALL(pip_window_controller(), CloseAndFocusInitiator());
  button_clicker.NotifyClick(dummy_event);
  testing::Mock::VerifyAndClearExpectations(&pip_window_controller());
}
