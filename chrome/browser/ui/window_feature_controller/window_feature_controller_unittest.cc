// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/window_feature_controller/window_feature_controller.h"

#include <memory>

#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/fullscreen/browser_window_fullscreen_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

using WindowFeature = WindowFeatureController::WindowFeature;

class WindowFeatureControllerTest : public testing::Test {
 public:
  WindowFeatureControllerTest() {
    ON_CALL(mock_browser_window_interface_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(unowned_user_data_host_));
  }

  WindowFeatureControllerTest(const WindowFeatureControllerTest&) = delete;
  WindowFeatureControllerTest& operator=(const WindowFeatureControllerTest&) =
      delete;

  ~WindowFeatureControllerTest() override = default;

 protected:
  std::unique_ptr<WindowFeatureController> CreateController(
      BrowserWindowInterface::Type browser_type,
      bool is_trusted_source,
      BrowserWindowFullscreenController* fullscreen_controller) {
    return std::make_unique<WindowFeatureController>(
        fullscreen_controller, /*app_controller=*/nullptr, browser_type,
        is_trusted_source, unowned_user_data_host_);
  }

  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface_;
  ui::UnownedUserDataHost unowned_user_data_host_;
};

// Tests for TYPE_NORMAL browser.
TEST_F(WindowFeatureControllerTest, NormalBrowserFeatures) {
  BrowserWindowFullscreenController fullscreen_controller(
      mock_browser_window_interface_);
  auto controller =
      CreateController(BrowserWindowInterface::Type::TYPE_NORMAL,
                       /*is_trusted_source=*/false, &fullscreen_controller);

  // Case 1: Not in fullscreen.
  fullscreen_controller.set_should_hide_ui_for_fullscreen_for_testing(false);

  // Supported features.
  EXPECT_TRUE(
      controller->SupportsWindowFeature(WindowFeature::kFeatureBookmarkBar));
  EXPECT_TRUE(
      controller->SupportsWindowFeature(WindowFeature::kFeatureTabStrip));
  EXPECT_TRUE(
      controller->SupportsWindowFeature(WindowFeature::kFeatureToolbar));
  EXPECT_TRUE(
      controller->SupportsWindowFeature(WindowFeature::kFeatureLocationBar));

  EXPECT_TRUE(
      controller->CanSupportWindowFeature(WindowFeature::kFeatureBookmarkBar));
  EXPECT_TRUE(
      controller->CanSupportWindowFeature(WindowFeature::kFeatureTabStrip));
  EXPECT_TRUE(
      controller->CanSupportWindowFeature(WindowFeature::kFeatureToolbar));
  EXPECT_TRUE(
      controller->CanSupportWindowFeature(WindowFeature::kFeatureLocationBar));

  // Unsupported features.
  EXPECT_FALSE(
      controller->SupportsWindowFeature(WindowFeature::kFeatureTitleBar));
  EXPECT_FALSE(controller->SupportsWindowFeature(WindowFeature::kFeatureNone));

  EXPECT_FALSE(
      controller->CanSupportWindowFeature(WindowFeature::kFeatureTitleBar));
  EXPECT_FALSE(
      controller->CanSupportWindowFeature(WindowFeature::kFeatureNone));

  // Case 2: In fullscreen.
  fullscreen_controller.set_should_hide_ui_for_fullscreen_for_testing(true);

  // Bookmark bar is still supported.
  EXPECT_TRUE(
      controller->SupportsWindowFeature(WindowFeature::kFeatureBookmarkBar));
  EXPECT_TRUE(
      controller->CanSupportWindowFeature(WindowFeature::kFeatureBookmarkBar));

  // Other UI features are not supported in fullscreen.
  EXPECT_FALSE(
      controller->SupportsWindowFeature(WindowFeature::kFeatureTabStrip));
  EXPECT_FALSE(
      controller->SupportsWindowFeature(WindowFeature::kFeatureToolbar));
  EXPECT_FALSE(
      controller->SupportsWindowFeature(WindowFeature::kFeatureLocationBar));

  // The features can however be supported.
  EXPECT_TRUE(
      controller->CanSupportWindowFeature(WindowFeature::kFeatureTabStrip));
  EXPECT_TRUE(
      controller->CanSupportWindowFeature(WindowFeature::kFeatureToolbar));
  EXPECT_TRUE(
      controller->CanSupportWindowFeature(WindowFeature::kFeatureLocationBar));
}

// Tests for TYPE_POPUP browser.
TEST_F(WindowFeatureControllerTest, PopupBrowserFeatures_Untrusted) {
  BrowserWindowFullscreenController fullscreen_controller(
      mock_browser_window_interface_);
  // Untrusted popup (e.g. created by website).
  auto controller =
      CreateController(BrowserWindowInterface::Type::TYPE_POPUP,
                       /*is_trusted_source=*/false, &fullscreen_controller);

  // Case 1: Not in fullscreen.
  fullscreen_controller.set_should_hide_ui_for_fullscreen_for_testing(false);

  // Supported features.
  EXPECT_TRUE(
      controller->SupportsWindowFeature(WindowFeature::kFeatureTitleBar));
  EXPECT_TRUE(
      controller->SupportsWindowFeature(WindowFeature::kFeatureLocationBar));

  EXPECT_TRUE(
      controller->CanSupportWindowFeature(WindowFeature::kFeatureTitleBar));
  EXPECT_TRUE(
      controller->CanSupportWindowFeature(WindowFeature::kFeatureLocationBar));

  // Unsupported features.
  EXPECT_FALSE(
      controller->SupportsWindowFeature(WindowFeature::kFeatureTabStrip));
  EXPECT_FALSE(
      controller->SupportsWindowFeature(WindowFeature::kFeatureToolbar));
  EXPECT_FALSE(
      controller->SupportsWindowFeature(WindowFeature::kFeatureBookmarkBar));
  EXPECT_FALSE(controller->SupportsWindowFeature(WindowFeature::kFeatureNone));

  // Case 2: In fullscreen.
  fullscreen_controller.set_should_hide_ui_for_fullscreen_for_testing(true);

  // Features are not supported in fullscreen;
  EXPECT_FALSE(
      controller->SupportsWindowFeature(WindowFeature::kFeatureTitleBar));
  EXPECT_FALSE(
      controller->SupportsWindowFeature(WindowFeature::kFeatureLocationBar));

  // The features can however be supported.
  EXPECT_TRUE(
      controller->CanSupportWindowFeature(WindowFeature::kFeatureTitleBar));
  EXPECT_TRUE(
      controller->CanSupportWindowFeature(WindowFeature::kFeatureLocationBar));
}

TEST_F(WindowFeatureControllerTest, PopupBrowserFeatures_Trusted) {
  BrowserWindowFullscreenController fullscreen_controller(
      mock_browser_window_interface_);
  // Trusted popup (e.g. devtools, or internal chrome popup).
  auto controller =
      CreateController(BrowserWindowInterface::Type::TYPE_POPUP,
                       /*is_trusted_source=*/true, &fullscreen_controller);

  fullscreen_controller.set_should_hide_ui_for_fullscreen_for_testing(false);

  // Trusted popups do not support title bar or location bar.
  EXPECT_FALSE(
      controller->SupportsWindowFeature(WindowFeature::kFeatureTitleBar));
  EXPECT_FALSE(
      controller->SupportsWindowFeature(WindowFeature::kFeatureLocationBar));
  EXPECT_TRUE(
      controller->CanSupportWindowFeature(WindowFeature::kFeatureTitleBar));
  EXPECT_TRUE(
      controller->CanSupportWindowFeature(WindowFeature::kFeatureLocationBar));
}

// Tests for TYPE_PICTURE_IN_PICTURE browser.
TEST_F(WindowFeatureControllerTest, PictureInPictureBrowserFeatures) {
  BrowserWindowFullscreenController fullscreen_controller(
      mock_browser_window_interface_);
  auto controller =
      CreateController(BrowserWindowInterface::Type::TYPE_PICTURE_IN_PICTURE,
                       /*is_trusted_source=*/false, &fullscreen_controller);

  fullscreen_controller.set_should_hide_ui_for_fullscreen_for_testing(false);

  // Only title bar is supported.
  EXPECT_TRUE(
      controller->SupportsWindowFeature(WindowFeature::kFeatureTitleBar));
  EXPECT_TRUE(
      controller->CanSupportWindowFeature(WindowFeature::kFeatureTitleBar));

  EXPECT_FALSE(
      controller->SupportsWindowFeature(WindowFeature::kFeatureLocationBar));
  EXPECT_FALSE(
      controller->SupportsWindowFeature(WindowFeature::kFeatureTabStrip));
  EXPECT_FALSE(
      controller->SupportsWindowFeature(WindowFeature::kFeatureToolbar));
  EXPECT_FALSE(
      controller->SupportsWindowFeature(WindowFeature::kFeatureBookmarkBar));
}

// Tests for TYPE_APP_POPUP browser (with null app controller).
TEST_F(WindowFeatureControllerTest, AppPopupBrowserFeatures_NullController) {
  BrowserWindowFullscreenController fullscreen_controller(
      mock_browser_window_interface_);
  auto controller =
      CreateController(BrowserWindowInterface::Type::TYPE_APP_POPUP,
                       /*is_trusted_source=*/false, &fullscreen_controller);

  // Case 1: Not in fullscreen.
  fullscreen_controller.set_should_hide_ui_for_fullscreen_for_testing(false);

  // Title bar is supported.
  EXPECT_TRUE(
      controller->SupportsWindowFeature(WindowFeature::kFeatureTitleBar));
  EXPECT_TRUE(
      controller->CanSupportWindowFeature(WindowFeature::kFeatureTitleBar));

  // Location bar is not supported because app_controller is null.
  EXPECT_FALSE(
      controller->SupportsWindowFeature(WindowFeature::kFeatureLocationBar));
  EXPECT_FALSE(
      controller->CanSupportWindowFeature(WindowFeature::kFeatureLocationBar));

  // Case 2: In fullscreen.
  fullscreen_controller.set_should_hide_ui_for_fullscreen_for_testing(true);

  // Title bar is not supported but CAN be.
  EXPECT_FALSE(
      controller->SupportsWindowFeature(WindowFeature::kFeatureTitleBar));
  EXPECT_TRUE(
      controller->CanSupportWindowFeature(WindowFeature::kFeatureTitleBar));
}
