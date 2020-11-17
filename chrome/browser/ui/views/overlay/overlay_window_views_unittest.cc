// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "chrome/browser/ui/views/overlay/overlay_window_views.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "content/public/test/test_web_contents_factory.h"
#include "ui/display/test/scoped_screen_override.h"
#include "ui/display/test/test_screen.h"

class TestPictureInPictureWindowController
    : public content::PictureInPictureWindowController {
 public:
  explicit TestPictureInPictureWindowController(
      content::WebContents* web_contents)
      : web_contents_(web_contents) {}

  // PictureInPictureWindowController:
  void Show() override {}
  void Close(bool) override {}
  void CloseAndFocusInitiator() override {}
  void OnWindowDestroyed() override {}
  content::OverlayWindow* GetWindowForTesting() override { return nullptr; }
  void UpdateLayerBounds() override {}
  bool IsPlayerActive() override { return false; }
  content::WebContents* GetWebContents() override { return web_contents_; }
  void UpdatePlaybackState(bool, bool) override {}
  bool TogglePlayPause() override { return false; }
  void SkipAd() override {}
  void NextTrack() override {}
  void PreviousTrack() override {}

 private:
  content::WebContents* const web_contents_;
};

// When running on ChromeOS, NativeWidgetAura requires the parent and/or
// context to be non-null. OverlayWindowViews provides neither, so we do it
// here. Normally this is done by the browser-specific ViewsDelegate.
class TestViewsDelegateWithContext : public views::TestViewsDelegate {
 public:
  // ViewsDelegate:
  void OnBeforeWidgetInit(
      views::Widget::InitParams* params,
      views::internal::NativeWidgetDelegate* delegate) override {
    views::TestViewsDelegate::OnBeforeWidgetInit(params, delegate);
    if (!params->context)
      params->context = context_;
  }

  void set_context(gfx::NativeWindow context) { context_ = context; }

 private:
  gfx::NativeWindow context_;
};

class OverlayWindowViewsTest : public ChromeViewsTestBase {
 public:
  // ChromeViewsTestBase:
  void SetUp() override {
    // set_views_delegate() must be called before SetUp(), and GetContext() is
    // null before that, hence the unobvious initialization order.
    auto views_delegate = std::make_unique<TestViewsDelegateWithContext>();
    auto* views_delegate_with_context = views_delegate.get();
    set_views_delegate(std::move(views_delegate));
    // Purposely skip ChromeViewsTestBase::SetUp() as that creates ash::Shell
    // on ChromeOS, which we don't want.
    ViewsTestBase::SetUp();
    views_delegate_with_context->set_context(GetContext());

    test_views_delegate()->set_use_desktop_native_widgets(true);

    // The default work area must be big enough to fit the minimum
    // OverlayWindowViews size.
    SetDisplayWorkArea({0, 0, 1000, 1000});

    overlay_window_ = OverlayWindowViews::Create(&pip_window_controller_);
  }

  void TearDown() override {
    overlay_window_.reset();
    ViewsTestBase::TearDown();
  }

  void SetDisplayWorkArea(const gfx::Rect& work_area) {
    display::Display display = test_screen_.GetPrimaryDisplay();
    display.set_work_area(work_area);
    test_screen_.display_list().UpdateDisplay(display);
  }

  OverlayWindowViews& overlay_window() { return *overlay_window_; }

 private:
  TestingProfile profile_;
  content::TestWebContentsFactory web_contents_factory_;
  TestPictureInPictureWindowController pip_window_controller_{
      web_contents_factory_.CreateWebContents(&profile_)};

  display::test::TestScreen test_screen_;
  display::test::ScopedScreenOverride scoped_screen_override_{&test_screen_};

  std::unique_ptr<OverlayWindowViews> overlay_window_;
};

TEST_F(OverlayWindowViewsTest, UpdateMaximumSize) {
  SetDisplayWorkArea({0, 0, 4000, 4000});

  overlay_window().UpdateVideoSize({480, 320});

  // The initial size is determined by the work area and the video natural size
  // (aspect ratio).
  EXPECT_EQ(gfx::Size(1200, 800), overlay_window().GetBounds().size());
  // The initial maximum size is a quarter of the work area.
  EXPECT_EQ(gfx::Size(2000, 2000), overlay_window().GetMaximumSize());

  // If the maximum size increases then we should keep the existing window size.
  SetDisplayWorkArea({0, 0, 8000, 8000});
  overlay_window().OnNativeWidgetMove();
  EXPECT_EQ(gfx::Size(1200, 800), overlay_window().GetBounds().size());
  EXPECT_EQ(gfx::Size(4000, 4000), overlay_window().GetMaximumSize());

  // If the maximum size decreases then we should shrink to fit.
  SetDisplayWorkArea({0, 0, 1000, 1000});
  overlay_window().OnNativeWidgetMove();
  EXPECT_EQ(gfx::Size(500, 500), overlay_window().GetBounds().size());
  EXPECT_EQ(gfx::Size(500, 500), overlay_window().GetMaximumSize());
}
