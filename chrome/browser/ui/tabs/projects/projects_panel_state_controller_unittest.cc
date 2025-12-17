// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/projects/projects_panel_state_controller.h"

#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

class ProjectsPanelStateControllerTest : public testing::Test {
 public:
  ProjectsPanelStateControllerTest() = default;
  ~ProjectsPanelStateControllerTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();

    EXPECT_CALL(mock_browser_window_interface_, GetUnownedUserDataHost)
        .WillRepeatedly(testing::ReturnRef(unowned_user_data_host_));

    // Action items like ToggleProjectsPanel are tested in interactive ui tests.
    controller_ = std::make_unique<ProjectsPanelStateController>(
        &mock_browser_window_interface_, /*root_action_item=*/nullptr);
  }

  void TearDown() override {
    controller_.reset();
    testing::Test::TearDown();
  }

  ProjectsPanelStateController* controller() { return controller_.get(); }

 private:
  std::unique_ptr<ProjectsPanelStateController> controller_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  MockBrowserWindowInterface mock_browser_window_interface_;
};

TEST_F(ProjectsPanelStateControllerTest, Initial) {
  EXPECT_FALSE(controller()->IsProjectsPanelVisible());
}

TEST_F(ProjectsPanelStateControllerTest, ProjectsPanelEnabled) {
  controller()->SetProjectsVisible(true);
  EXPECT_TRUE(controller()->IsProjectsPanelVisible());

  controller()->SetProjectsVisible(false);
  EXPECT_FALSE(controller()->IsProjectsPanelVisible());
}

TEST_F(ProjectsPanelStateControllerTest, Subscription) {
  int call_count = 0;
  auto subscription = controller()->RegisterOnStateChanged(base::BindRepeating(
      [](int* call_count, ProjectsPanelStateController* controller) {
        (*call_count)++;
        EXPECT_TRUE(controller->IsProjectsPanelVisible());
      },
      &call_count));

  controller()->SetProjectsVisible(true);

  EXPECT_TRUE(controller()->IsProjectsPanelVisible());
  EXPECT_EQ(1, call_count);

  // Setting to same value should not trigger a notification.
  controller()->SetProjectsVisible(true);
  EXPECT_EQ(1, call_count);
}
