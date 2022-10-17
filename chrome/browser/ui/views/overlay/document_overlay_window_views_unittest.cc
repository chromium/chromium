// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/document_overlay_window_views.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/views/overlay/close_image_button.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/browser/document_picture_in_picture_window_controller.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/display/test/scoped_screen_override.h"
#include "ui/display/test/test_screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/test/button_test_api.h"

namespace {

constexpr gfx::Size kMinWindowSize(200, 100);

}  // namespace

class FakeOverlayLocationBarView : public OverlayLocationBarViewProxy {
 public:
  FakeOverlayLocationBarView() {
    view_holder_ = std::make_unique<views::View>();
  }
  ~FakeOverlayLocationBarView() override = default;
  void Init() override {}
  std::unique_ptr<views::View> ReleaseView() override {
    return std::move(view_holder_);
  }

 private:
  std::unique_ptr<views::View> view_holder_;
};

class TestDocumentPictureInPictureWindowController
    : public content::DocumentPictureInPictureWindowController {
 public:
  TestDocumentPictureInPictureWindowController() = default;

  // PictureInPictureWindowController:
  void Show() override {}
  void FocusInitiator() override {}
  MOCK_METHOD(void, Close, (bool));
  void CloseAndFocusInitiator() override {}
  MOCK_METHOD(void, OnWindowDestroyed, (bool));
  content::WebContents* GetWebContents() override { return web_contents_; }
  content::WebContents* GetChildWebContents() override {
    return child_web_contents_;
  }

  // DocumentPictureInPictureWindowController
  void SetChildWebContents(content::WebContents* child) override {
    child_web_contents_ = child;
  }

  void set_web_contents(content::WebContents* web_contents) {
    web_contents_ = web_contents;
  }

  void destroy() { child_web_contents_ = nullptr; }
  absl::optional<gfx::Rect> GetWindowBounds() override { return absl::nullopt; }

 private:
  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<content::WebContents> child_web_contents_;
};

class DocumentOverlayWindowViewsTest : public ChromeViewsTestBase {
 public:
  DocumentOverlayWindowViewsTest() = default;
  // ChromeViewsTestBase:
  void SetUp() override {
    // Purposely skip ChromeViewsTestBase::SetUp() as that creates ash::Shell
    // on ChromeOS, which we don't want.
    ViewsTestBase::SetUp();
    // web_contents_ needs to be created after the constructor, so that
    // |feature_list_| can be initialized before other threads check if a
    // feature is enabled.
    web_contents_ = web_contents_factory_.CreateWebContents(&profile_);
    pip_window_controller_.set_web_contents(web_contents_);

    // The child web contents will be owned by the WebView, so create them
    // separately. (WebContentsFactory owns its created WebContents which isn't
    // compatible with this usage.)
    auto child =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);

    pip_window_controller_.SetChildWebContents(child.get());

#if BUILDFLAG(IS_CHROMEOS)
    test_views_delegate()->set_context(GetContext());
#endif
    test_views_delegate()->set_use_desktop_native_widgets(true);

    // The default work area must be big enough to fit the minimum
    // DocumentOverlayWindowViews size.
    SetDisplayWorkArea({0, 0, 1000, 1000});

    auto fake_location_bar = std::make_unique<FakeOverlayLocationBarView>();
    overlay_window_ = DocumentOverlayWindowViews::Create(
        &pip_window_controller_, std::move(fake_location_bar));
    overlay_window_->set_minimum_size_for_testing(kMinWindowSize);
  }

  void TearDown() override {
    // AutocompleteClassifierFactory::GetInstance()->Disassociate(&profile_);
    base::RunLoop().RunUntilIdle();
    pip_window_controller_.destroy();
    overlay_window_.reset();
    ViewsTestBase::TearDown();
  }

  void SetDisplayWorkArea(const gfx::Rect& work_area) {
    display::Display display = test_screen_.GetPrimaryDisplay();
    display.set_work_area(work_area);
    test_screen_.display_list().UpdateDisplay(display);
  }

  DocumentOverlayWindowViews& overlay_window() { return *overlay_window_; }

  content::WebContents* web_contents() { return web_contents_; }

  TestDocumentPictureInPictureWindowController& pip_window_controller() {
    return pip_window_controller_;
  }

 private:
  TestingProfile profile_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_;
  TestDocumentPictureInPictureWindowController pip_window_controller_;

  display::test::TestScreen test_screen_;
  display::test::ScopedScreenOverride scoped_screen_override_{&test_screen_};

  std::unique_ptr<DocumentOverlayWindowViews> overlay_window_;
};

TEST_F(DocumentOverlayWindowViewsTest, InitialWindowSize_Square) {
  // Fit the window taking 1/5 (both dimensions) of the work area as the
  // starting size, and applying the size and aspect ratio constraints.
  overlay_window().UpdateNaturalSize({400, 400});
  EXPECT_EQ(gfx::Size(200, 200), overlay_window().GetBounds().size());
  EXPECT_EQ(gfx::Size(200, 170),
            overlay_window().document_layer_for_testing()->size());
}

TEST_F(DocumentOverlayWindowViewsTest, InitialWindowSize_Horizontal) {
  // Set an arbitrary starting position in the top left corner. Otherwise, since
  // OnRootViewReady has already set a content size, the default window position
  // would be at the bottom right which results in a ResizeEdge::kTopLeft that
  // is considered a horizontal resize which shrinks the window. If it starts
  // out at the top left, we get ResizeEdge::kBottomRight which is a vertical
  // resize that enlarges the window.
  overlay_window().SetBounds({0, 0, 200, 200});

  // Fit the window taking 1/5 (both dimensions) of the work area as the
  // starting size, and applying the size and aspect ratio constraints.
  overlay_window().UpdateNaturalSize({400, 200});
  EXPECT_EQ(gfx::Size(400, 200), overlay_window().GetBounds().size());
  EXPECT_EQ(gfx::Size(400, 170),
            overlay_window().document_layer_for_testing()->size());
}

TEST_F(DocumentOverlayWindowViewsTest, InitialWindowSize_Vertical) {
  // Fit the window taking 1/5 (both dimensions) of the work area as the
  // starting size, and applying the size and aspect ratio constraints.
  overlay_window().UpdateNaturalSize({400, 500});
  EXPECT_EQ(gfx::Size(200, 250), overlay_window().GetBounds().size());
  EXPECT_EQ(gfx::Size(200, 220),
            overlay_window().document_layer_for_testing()->size());
}

TEST_F(DocumentOverlayWindowViewsTest, Letterboxing) {
  overlay_window().UpdateNaturalSize({400, 10});

  // Must fit within the minimum height of 146. But with the aspect ratio of
  // 40:1 the width gets exceedingly big and must be limited to the maximum of
  // 800. Thus, letterboxing is unavoidable.
  EXPECT_EQ(gfx::Size(800, 100), overlay_window().GetBounds().size());
  EXPECT_EQ(gfx::Size(800, 0),
            overlay_window().document_layer_for_testing()->size());
}

TEST_F(DocumentOverlayWindowViewsTest, Pillarboxing) {
  overlay_window().UpdateNaturalSize({10, 400});

  // Must fit within the minimum width of 260. But with the aspect ratio of
  // 1:40 the height gets exceedingly big and must be limited to the maximum of
  // 800. Thus, pillarboxing is unavoidable.
  EXPECT_EQ(gfx::Size(200, 800), overlay_window().GetBounds().size());
  EXPECT_EQ(gfx::Size(20, 770),
            overlay_window().document_layer_for_testing()->size());
}

TEST_F(DocumentOverlayWindowViewsTest, Pillarboxing_Square) {
  overlay_window().UpdateNaturalSize({100, 100});

  // Pillarboxing also occurs on Linux even with the square aspect ratio,
  // because the user is allowed to size the window to the rectangular minimum
  // size.
  overlay_window().SetSize({200, 100});
  EXPECT_EQ(gfx::Size(100, 70),
            overlay_window().document_layer_for_testing()->size());
}

TEST_F(DocumentOverlayWindowViewsTest, ApproximateAspectRatio_Horizontal) {
  // "Horizontal" video.
  overlay_window().UpdateNaturalSize({320, 240});

  // The user drags the window resizer horizontally and now the integer window
  // dimensions can't reproduce the video aspect ratio exactly. The video
  // should still fill the entire window area.
  overlay_window().SetSize({320, 240});
  EXPECT_EQ(gfx::Size(320, 210),
            overlay_window().document_layer_for_testing()->size());

  overlay_window().SetSize({321, 241});
  EXPECT_EQ(gfx::Size(321, 211),
            overlay_window().document_layer_for_testing()->size());

  // Wide video.
  overlay_window().UpdateNaturalSize({1600, 900});

  overlay_window().SetSize({444, 250});
  EXPECT_EQ(gfx::Size(444, 220),
            overlay_window().document_layer_for_testing()->size());

  overlay_window().SetSize({445, 250});
  EXPECT_EQ(gfx::Size(445, 220),
            overlay_window().document_layer_for_testing()->size());

  // Very wide video.
  overlay_window().UpdateNaturalSize({400, 100});

  overlay_window().SetSize({478, 120});
  EXPECT_EQ(gfx::Size(478, 90),
            overlay_window().document_layer_for_testing()->size());

  overlay_window().SetSize({481, 120});
  EXPECT_EQ(gfx::Size(481, 90),
            overlay_window().document_layer_for_testing()->size());
}

TEST_F(DocumentOverlayWindowViewsTest, ApproximateAspectRatio_Vertical) {
  // "Vertical" video.
  overlay_window().UpdateNaturalSize({240, 320});

  // The user dragged the window resizer vertically and now the integer window
  // dimensions can't reproduce the video aspect ratio exactly. The video
  // should still fill the entire window area.
  overlay_window().SetSize({240, 320});
  EXPECT_EQ(gfx::Size(240, 290),
            overlay_window().document_layer_for_testing()->size());

  overlay_window().SetSize({239, 319});
  EXPECT_EQ(gfx::Size(239, 289),
            overlay_window().document_layer_for_testing()->size());

  // Narrow video.
  overlay_window().UpdateNaturalSize({900, 1600});

  overlay_window().SetSize({250, 444});
  EXPECT_EQ(gfx::Size(250, 414),
            overlay_window().document_layer_for_testing()->size());

  overlay_window().SetSize({250, 445});
  EXPECT_EQ(gfx::Size(250, 415),
            overlay_window().document_layer_for_testing()->size());

  // Very narrow video.
  // NOTE: Window width is bounded by the minimum size.
  overlay_window().UpdateNaturalSize({100, 400});

  overlay_window().SetSize({200, 478});
  EXPECT_EQ(gfx::Size(120, 448),
            overlay_window().document_layer_for_testing()->size());

  overlay_window().SetSize({200, 481});
  EXPECT_EQ(gfx::Size(120, 451),
            overlay_window().document_layer_for_testing()->size());
}

TEST_F(DocumentOverlayWindowViewsTest, UpdateMaximumSize) {
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

TEST_F(DocumentOverlayWindowViewsTest, IgnoreInvalidMaximumSize) {
  ASSERT_EQ(gfx::Size(800, 800), overlay_window().GetMaximumSize());

  SetDisplayWorkArea({0, 0, 0, 0});
  EXPECT_EQ(gfx::Size(800, 800), overlay_window().GetMaximumSize());
}

TEST_F(DocumentOverlayWindowViewsTest, UpdateNaturalSizeDoesNotMoveWindow) {
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
TEST_F(DocumentOverlayWindowViewsTest, HitTestFrameView) {
  // Since the NonClientFrameView is the only non-custom direct descendent of
  // the NonClientView, we can assume that if the frame does not accept the
  // point but the NonClientView does, then it will be handled by one of the
  // custom overlay views.
  auto point = gfx::Point(50, 50);
  views::NonClientView* non_client_view = overlay_window().non_client_view();
  EXPECT_EQ(non_client_view->frame_view()->HitTestPoint(point), false);
  EXPECT_EQ(non_client_view->HitTestPoint(point), true);
}

// Tests that hit tests on various areas of the window have the expected
// hit test type.
TEST_F(DocumentOverlayWindowViewsTest, HitTestTypesByLocation) {
  views::NonClientView* view = overlay_window().non_client_view();

  overlay_window().UpdateNaturalSize({200, 200});

  // The corners and edges of the frame support resizing.
  EXPECT_EQ(view->NonClientHitTest({1, 1}), HTTOPLEFT);
  EXPECT_EQ(view->NonClientHitTest({198, 1}), HTTOPRIGHT);
  EXPECT_EQ(view->NonClientHitTest({1, 100}), HTLEFT);
  EXPECT_EQ(view->NonClientHitTest({198, 100}), HTRIGHT);
  EXPECT_EQ(view->NonClientHitTest({1, 198}), HTBOTTOMLEFT);
  EXPECT_EQ(view->NonClientHitTest({198, 198}), HTBOTTOMRIGHT);

  // The middle of the controls bar allows dragging the window.
  EXPECT_EQ(view->NonClientHitTest({100, 15}), HTCAPTION);

  // The right side of the control bar contains the close button
  // which is interactive.
  EXPECT_EQ(view->NonClientHitTest({185, 15}), HTNOWHERE);

  // Clicks on the web content area must not be intercepted so that
  // interactions remain possible.
  EXPECT_EQ(view->NonClientHitTest({100, 100}), HTNOWHERE);
}

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
// With pillarboxing, the close button doesn't cover the video area. Make sure
// hovering the button doesn't get handled like normal mouse exit events
// causing the controls to hide.
TEST_F(DocumentOverlayWindowViewsTest, NoMouseExitWithinWindowBounds) {
  overlay_window().UpdateNaturalSize({10, 400});

  const auto close_button_bounds = overlay_window().GetCloseControlsBounds();
  const auto video_bounds =
      overlay_window().document_layer_for_testing()->bounds();
  ASSERT_FALSE(video_bounds.Contains(close_button_bounds));

  const gfx::Point moved_location(video_bounds.origin() + gfx::Vector2d(5, 5));
  ui::MouseEvent moved_event(ui::ET_MOUSE_MOVED, moved_location, moved_location,
                             ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
  overlay_window().OnMouseEvent(&moved_event);
  ASSERT_TRUE(overlay_window().AreControlsVisible());

  const gfx::Point exited_location(close_button_bounds.CenterPoint());
  ui::MouseEvent exited_event(ui::ET_MOUSE_EXITED, exited_location,
                              exited_location, ui::EventTimeForNow(),
                              ui::EF_NONE, ui::EF_NONE);
  overlay_window().OnMouseEvent(&exited_event);
  EXPECT_TRUE(overlay_window().AreControlsVisible());
}

#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

TEST_F(DocumentOverlayWindowViewsTest, PauseOnCloseButton) {
  views::test::ButtonTestApi close_button_clicker(
      overlay_window().close_button_for_testing());
  ui::MouseEvent dummy_event(ui::ET_MOUSE_PRESSED, gfx::Point(0, 0),
                             gfx::Point(0, 0), ui::EventTimeForNow(), 0, 0);

  // Closing via the close button should pause the content.
  EXPECT_CALL(pip_window_controller(), Close(true));
  close_button_clicker.NotifyClick(dummy_event);
  testing::Mock::VerifyAndClearExpectations(&pip_window_controller());
}

TEST_F(DocumentOverlayWindowViewsTest, PauseOnWidgetClose) {
  // When the native widget is destroyed we should pause the underlying content.
  EXPECT_CALL(pip_window_controller(), OnWindowDestroyed(true));
  overlay_window().CloseNow();
  testing::Mock::VerifyAndClearExpectations(&pip_window_controller());
}

TEST_F(DocumentOverlayWindowViewsTest, SmallDisplayWorkAreaDoesNotCrash) {
  SetDisplayWorkArea({0, 0, 240, 120});
  overlay_window().UpdateNaturalSize({400, 300});

  // Since the work area would force a max size smaller than the minimum size,
  // the size is fixed at the minimum size.
  EXPECT_EQ(kMinWindowSize, overlay_window().GetBounds().size());
  EXPECT_EQ(kMinWindowSize, overlay_window().GetMaximumSize());

  // The video should still be letterboxed to the correct aspect ratio.
  EXPECT_EQ(gfx::Size(133, 70),
            overlay_window().document_layer_for_testing()->size());
}
