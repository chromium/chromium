// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organizer/organizer_panel_state_controller.h"

#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

class OrganizerPanelStateControllerTest : public testing::Test {
 public:
  OrganizerPanelStateControllerTest() = default;
  ~OrganizerPanelStateControllerTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();

    EXPECT_CALL(mock_browser_window_interface_, GetUnownedUserDataHost)
        .WillRepeatedly(testing::ReturnRef(unowned_user_data_host_));
    EXPECT_CALL(mock_browser_window_interface_, GetProfile())
        .WillRepeatedly(testing::Return(&profile_));

    // Action items like ToggleOrganizerPanel are tested in interactive ui
    // tests.
    controller_ = std::make_unique<OrganizerPanelStateController>(
        &mock_browser_window_interface_, /*root_action_item=*/nullptr);
  }

  void TearDown() override {
    controller_.reset();
    testing::Test::TearDown();
  }

  OrganizerPanelStateController* controller() { return controller_.get(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<OrganizerPanelStateController> controller_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  MockBrowserWindowInterface mock_browser_window_interface_;
};

TEST_F(OrganizerPanelStateControllerTest, Initial) {
  EXPECT_FALSE(controller()->IsOrganizerPanelVisible());
}

TEST_F(OrganizerPanelStateControllerTest, OrganizerPanelEnabled) {
  controller()->SetOrganizerVisible(true);
  EXPECT_TRUE(controller()->IsOrganizerPanelVisible());

  controller()->SetOrganizerVisible(false);
  EXPECT_FALSE(controller()->IsOrganizerPanelVisible());
}

TEST_F(OrganizerPanelStateControllerTest, Subscription) {
  int call_count = 0;
  auto subscription = controller()->RegisterOnStateChanged(base::BindRepeating(
      [](int* call_count, OrganizerPanelStateController* controller) {
        (*call_count)++;
        EXPECT_TRUE(controller->IsOrganizerPanelVisible());
      },
      &call_count));

  controller()->SetOrganizerVisible(true);

  EXPECT_TRUE(controller()->IsOrganizerPanelVisible());
  EXPECT_EQ(1, call_count);

  // Setting to same value should not trigger a notification.
  controller()->SetOrganizerVisible(true);
  EXPECT_EQ(1, call_count);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(OrganizerPanelStateControllerTest, ExtensionOpenToggleClose) {
  const extensions::ExtensionId kExt1 = "abcdefghijklmnopabcdefghijklmnop";
  const extensions::ExtensionId kExt2 = "ponmlkjihgfedcbaponmlkjihgfedcba";

  int call_count = 0;
  auto subscription = controller()->RegisterOnStateChanged(base::BindRepeating(
      [](int* call_count, OrganizerPanelStateController* controller) {
        (*call_count)++;
      },
      &call_count));

  // Opening for extension opens panel and sets active extension ID.
  controller()->OpenForExtension(kExt1);
  EXPECT_TRUE(controller()->IsOrganizerPanelVisible());
  EXPECT_EQ(controller()->active_extension_id(), kExt1);
  EXPECT_EQ(1, call_count);

  // Calling OpenForExtension with the same extension when already open
  // should not notify again.
  controller()->OpenForExtension(kExt1);
  EXPECT_EQ(1, call_count);

  // Opening for another extension keeps it open and updates active extension.
  controller()->OpenForExtension(kExt2);
  EXPECT_TRUE(controller()->IsOrganizerPanelVisible());
  EXPECT_EQ(controller()->active_extension_id(), kExt2);
  EXPECT_EQ(2, call_count);

  // Toggling same extension closes the panel.
  controller()->ToggleForExtension(kExt2);
  EXPECT_FALSE(controller()->IsOrganizerPanelVisible());
  EXPECT_FALSE(controller()->active_extension_id().has_value());
  EXPECT_EQ(3, call_count);

  // Toggling while closed opens it.
  controller()->ToggleForExtension(kExt1);
  EXPECT_TRUE(controller()->IsOrganizerPanelVisible());
  EXPECT_EQ(controller()->active_extension_id(), kExt1);
  EXPECT_EQ(4, call_count);

  // Closing matching extension closes the panel.
  controller()->CloseForExtension(kExt1);
  EXPECT_FALSE(controller()->IsOrganizerPanelVisible());
  EXPECT_FALSE(controller()->active_extension_id().has_value());
  EXPECT_EQ(5, call_count);
}
#endif
