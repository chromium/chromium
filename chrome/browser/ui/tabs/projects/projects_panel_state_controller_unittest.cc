// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/projects/projects_panel_state_controller.h"

#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/base/testing_profile.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
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
    EXPECT_CALL(mock_browser_window_interface_, GetProfile())
        .WillRepeatedly(testing::Return(&profile_));

    mock_aim_eligibility_service_ =
        std::make_unique<testing::NiceMock<MockAimEligibilityService>>(
            *profile_.GetPrefs(), nullptr, nullptr, nullptr);
    ON_CALL(*mock_aim_eligibility_service_, IsAimEligible())
        .WillByDefault(testing::Return(false));

    // Action items like ToggleProjectsPanel are tested in interactive ui tests.
    controller_ = std::make_unique<ProjectsPanelStateController>(
        &mock_browser_window_interface_, /*root_action_item=*/nullptr,
        mock_aim_eligibility_service_.get(), /*glic_enabling=*/nullptr);
  }

  void TearDown() override {
    controller_.reset();
    testing::Test::TearDown();
  }

  ProjectsPanelStateController* controller() { return controller_.get(); }
  MockAimEligibilityService* mock_aim_eligibility_service() {
    return mock_aim_eligibility_service_.get();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<ProjectsPanelStateController> controller_;
  std::unique_ptr<MockAimEligibilityService> mock_aim_eligibility_service_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  MockBrowserWindowInterface mock_browser_window_interface_;
};

TEST_F(ProjectsPanelStateControllerTest, Initial) {
  EXPECT_FALSE(controller()->IsProjectsPanelVisible());
  EXPECT_FALSE(controller()->CanShowAimThreads());
  EXPECT_FALSE(controller()->CanShowGeminiThreads());
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

TEST_F(ProjectsPanelStateControllerTest, ThreadEligibilitySubscription) {
  int call_count = 0;
  base::RepeatingClosure aim_eligibility_changed_callback;
  EXPECT_CALL(*mock_aim_eligibility_service(),
              RegisterEligibilityChangedCallback(testing::_))
      .WillOnce(
          testing::DoAll(testing::SaveArg<0>(&aim_eligibility_changed_callback),
                         testing::Return(base::CallbackListSubscription())));

  controller_.reset();
  controller_ = std::make_unique<ProjectsPanelStateController>(
      &mock_browser_window_interface_, /*root_action_item=*/nullptr,
      mock_aim_eligibility_service(), /*glic_enabling=*/nullptr);

  auto subscription =
      controller()->RegisterOnThreadEligibilityChanged(base::BindRepeating(
          [](int* call_count, ProjectsPanelStateController* controller) {
            (*call_count)++;
          },
          &call_count));

  EXPECT_CALL(*mock_aim_eligibility_service(), IsAimEligible())
      .WillRepeatedly(testing::Return(true));
  aim_eligibility_changed_callback.Run();
  EXPECT_EQ(1, call_count);
}

TEST_F(ProjectsPanelStateControllerTest, CanShowAimThreads) {
  int call_count = 0;
  base::RepeatingClosure aim_eligibility_changed_callback;
  EXPECT_CALL(*mock_aim_eligibility_service(),
              RegisterEligibilityChangedCallback(testing::_))
      .WillOnce(
          testing::DoAll(testing::SaveArg<0>(&aim_eligibility_changed_callback),
                         testing::Return(base::CallbackListSubscription())));

  controller_.reset();
  controller_ = std::make_unique<ProjectsPanelStateController>(
      &mock_browser_window_interface_, /*root_action_item=*/nullptr,
      mock_aim_eligibility_service(), /*glic_enabling=*/nullptr);

  auto subscription =
      controller()->RegisterOnThreadEligibilityChanged(base::BindRepeating(
          [](int* call_count, ProjectsPanelStateController* controller) {
            (*call_count)++;
          },
          &call_count));

  EXPECT_FALSE(controller()->CanShowAimThreads());

  EXPECT_CALL(*mock_aim_eligibility_service(), IsAimEligible())
      .WillRepeatedly(testing::Return(true));
  aim_eligibility_changed_callback.Run();
  EXPECT_TRUE(controller()->CanShowAimThreads());
  EXPECT_EQ(1, call_count);

  EXPECT_CALL(*mock_aim_eligibility_service(), IsAimEligible())
      .WillRepeatedly(testing::Return(false));
  aim_eligibility_changed_callback.Run();
  EXPECT_FALSE(controller()->CanShowAimThreads());
  EXPECT_EQ(2, call_count);
}
