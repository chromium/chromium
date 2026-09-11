// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organizer/organizer_panel_controller.h"

#include <memory>

#include "chrome/browser/ui/animation/browser_animation_controller.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/extensions/extension_side_panel_utils.h"
#include "chrome/browser/ui/views/animations/organizer_panel_animations.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

class OrganizerPanelControllerTest : public testing::Test {
 public:
  OrganizerPanelControllerTest() = default;
  ~OrganizerPanelControllerTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();

    EXPECT_CALL(mock_browser_window_interface_, GetUnownedUserDataHost)
        .WillRepeatedly(testing::ReturnRef(unowned_user_data_host_));
    EXPECT_CALL(mock_browser_window_interface_, GetProfile())
        .WillRepeatedly(testing::Return(&profile_));

    animation_controller_ = std::make_unique<BrowserAnimationController>(
        mock_browser_window_interface_);
    animation_controller_->AddAnimationProvider(
        std::make_unique<OrganizerPanelAnimations>());

    // Action items like ToggleOrganizerPanel are tested in interactive ui
    // tests.
    controller_ = std::make_unique<OrganizerPanelController>(
        mock_browser_window_interface_, /*root_action_item=*/nullptr);
  }

  void TearDown() override {
    controller_.reset();
    animation_controller_.reset();
    testing::Test::TearDown();
  }

  OrganizerPanelController* controller() { return controller_.get(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<BrowserAnimationController> animation_controller_;
  std::unique_ptr<OrganizerPanelController> controller_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  MockBrowserWindowInterface mock_browser_window_interface_;
};

TEST_F(OrganizerPanelControllerTest, Initial) {
  EXPECT_FALSE(controller()->IsOrganizerPanelVisible());
}

TEST_F(OrganizerPanelControllerTest, OrganizerPanelEnabled) {
  controller()->SetOrganizerVisible(true);
  EXPECT_TRUE(controller()->IsOrganizerPanelVisible());

  controller()->SetOrganizerVisible(false);
  EXPECT_FALSE(controller()->IsOrganizerPanelVisible());
}

TEST_F(OrganizerPanelControllerTest, Subscription) {
  UNCALLED_MOCK_CALLBACK(OrganizerPanelController::StateChangedCallback,
                         callback);
  const auto subscription =
      controller()->RegisterOnStateChanged(callback.Get());

  EXPECT_CALL_IN_SCOPE(callback, Run, controller()->SetOrganizerVisible(true));
  EXPECT_TRUE(controller()->IsOrganizerPanelVisible());

  // Setting to same value should not trigger a notification.
  controller()->SetOrganizerVisible(true);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(OrganizerPanelControllerTest, ExtensionOpenToggleClose) {
  UNCALLED_MOCK_CALLBACK(OrganizerPanelController::StateChangedCallback,
                         callback);
  const auto subscription =
      controller()->RegisterOnStateChanged(callback.Get());

  const extensions::ExtensionId kExt1 = "abcdefghijklmnopabcdefghijklmnop";
  const extensions::ExtensionId kExt2 = "ponmlkjihgfedcbaponmlkjihgfedcba";

  // Opening for extension opens panel and sets active extension ID.
  EXPECT_CALL_IN_SCOPE(callback, Run, controller()->OpenForExtension(kExt1));
  EXPECT_TRUE(controller()->IsOrganizerPanelVisible());
  EXPECT_EQ(controller()->active_extension_id(), kExt1);

  // Calling OpenForExtension with the same extension when already open
  // should not notify again.
  controller()->OpenForExtension(kExt1);
  EXPECT_TRUE(controller()->IsOrganizerPanelVisible());

  // Opening for another extension keeps it open and updates active extension.
  controller()->OpenForExtension(kExt2);
  EXPECT_TRUE(controller()->IsOrganizerPanelVisible());
  EXPECT_EQ(controller()->active_extension_id(), kExt2);

  // Toggling same extension closes the panel.
  EXPECT_CALL_IN_SCOPE(callback, Run, controller()->ToggleForExtension(kExt2));
  EXPECT_FALSE(controller()->IsOrganizerPanelVisible());
  EXPECT_FALSE(controller()->active_extension_id().has_value());

  // Toggling while closed opens it.
  EXPECT_CALL_IN_SCOPE(callback, Run, controller()->ToggleForExtension(kExt1));
  EXPECT_TRUE(controller()->IsOrganizerPanelVisible());
  EXPECT_EQ(controller()->active_extension_id(), kExt1);

  // Closing matching extension closes the panel.
  EXPECT_CALL_IN_SCOPE(callback, Run, controller()->CloseForExtension(kExt1));
  EXPECT_FALSE(controller()->IsOrganizerPanelVisible());
  EXPECT_FALSE(controller()->active_extension_id().has_value());
}
#endif
