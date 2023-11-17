// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/relaunch_chrome_bubble_controller.h"

#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class RelaunchChromeBubbleControllerTest : public ::testing::Test {
 public:
  RelaunchChromeBubbleControllerTest() {
    test_pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    test_pref_service_->registry()->RegisterIntegerPref(
        password_manager::prefs::kRelaunchChromeBubbleDismissedCounter, 0);
    mock_delegate_ =
        std::make_unique<testing::NiceMock<PasswordsModelDelegateMock>>();
  }
  ~RelaunchChromeBubbleControllerTest() override = default;

  PasswordsModelDelegateMock* delegate() { return mock_delegate_.get(); }
  RelaunchChromeBubbleController* controller() { return controller_.get(); }

  TestingPrefServiceSimple* test_pref_service() {
    return test_pref_service_.get();
  }

  void CreateController() {
    EXPECT_CALL(*delegate(), OnBubbleShown());
    controller_ = std::make_unique<RelaunchChromeBubbleController>(
        mock_delegate_->AsWeakPtr(), test_pref_service_.get());
    EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(delegate()));
  }

 private:
  std::unique_ptr<PasswordsModelDelegateMock> mock_delegate_;
  std::unique_ptr<TestingPrefServiceSimple> test_pref_service_;
  std::unique_ptr<RelaunchChromeBubbleController> controller_;
};

TEST_F(RelaunchChromeBubbleControllerTest, Content) {
  CreateController();
  EXPECT_NE(std::u16string(), controller()->GetTitle());
  EXPECT_NE(std::u16string(), controller()->GetBody());
  EXPECT_NE(std::u16string(), controller()->GetContinueButtonText());
  EXPECT_NE(std::u16string(), controller()->GetNoThanksButtonText());
}

TEST_F(RelaunchChromeBubbleControllerTest, Cancel) {
  CreateController();

  controller()->OnCanceled();
  EXPECT_EQ(test_pref_service()->GetInteger(
                password_manager::prefs::kRelaunchChromeBubbleDismissedCounter),
            1);
}

TEST_F(RelaunchChromeBubbleControllerTest, Restart) {
  CreateController();

  EXPECT_CALL(*delegate(), RelaunchChrome);
  controller()->OnAccepted();
}
