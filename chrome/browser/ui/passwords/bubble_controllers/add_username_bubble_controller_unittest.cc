// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/add_username_bubble_controller.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Return;
using ::testing::ReturnRef;

namespace {
constexpr char kUIDismissalReasonGeneralMetric[] =
    "PasswordManager.UIDismissalReason";
constexpr char kUsernameAddedMetric[] =
    "PasswordBubble.AddUsernameBubble.UsernameAdded";
constexpr char16_t kUsername[] = u"Admin";
constexpr char16_t kPassword[] = u"AdminPass";
}  // namespace

class AddUsernameBubbleControllerTest : public ::testing::Test {
 public:
  AddUsernameBubbleControllerTest() {
    mock_delegate_ =
        std::make_unique<testing::NiceMock<PasswordsModelDelegateMock>>();

    pending_password_.username_value = kUsername;
    pending_password_.password_value = kPassword;
  }

  ~AddUsernameBubbleControllerTest() override = default;

  PasswordsModelDelegateMock* delegate() { return mock_delegate_.get(); }
  AddUsernameBubbleController* controller() { return controller_.get(); }

  password_manager::PasswordForm& pending_password() {
    return pending_password_;
  }
  const password_manager::PasswordForm& pending_password() const {
    return pending_password_;
  }

  void CreateController() {
    EXPECT_CALL(*delegate(), GetState())
        .WillRepeatedly(Return(
            password_manager::ui::GENERATED_PASSWORD_CONFIRMATION_STATE));
    EXPECT_CALL(*delegate(), GetPendingPassword())
        .WillOnce(ReturnRef(pending_password()));
    EXPECT_CALL(*delegate(), OnBubbleShown());
    controller_ = std::make_unique<AddUsernameBubbleController>(
        mock_delegate_->AsWeakPtr(),
        PasswordBubbleControllerBase::DisplayReason::kAutomatic);
    EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(delegate()));
  }

 private:
  std::unique_ptr<AddUsernameBubbleController> controller_;
  std::unique_ptr<PasswordsModelDelegateMock> mock_delegate_;
  password_manager::PasswordForm pending_password_;
};

TEST_F(AddUsernameBubbleControllerTest, Destroy) {
  base::HistogramTester histogram_tester;
  CreateController();

  EXPECT_CALL(*delegate(), OnBubbleHidden());
  controller()->OnBubbleClosing();
  histogram_tester.ExpectUniqueSample(
      kUIDismissalReasonGeneralMetric,
      password_manager::metrics_util::NO_DIRECT_INTERACTION, 1);
  histogram_tester.ExpectUniqueSample(kUsernameAddedMetric, false, 1);
}

TEST_F(AddUsernameBubbleControllerTest, SavePassword) {
  base::HistogramTester histogram_tester;
  CreateController();

  EXPECT_CALL(*delegate(), OnAddUsernameSaveClicked(Eq(kUsername), _));
  controller()->OnSaveClicked();

  EXPECT_CALL(*delegate(), OnBubbleHidden());
  controller()->OnBubbleClosing();
  histogram_tester.ExpectUniqueSample(
      kUIDismissalReasonGeneralMetric,
      password_manager::metrics_util::CLICKED_ACCEPT, 1);
  histogram_tester.ExpectUniqueSample(kUsernameAddedMetric, true, 1);
}

TEST_F(AddUsernameBubbleControllerTest, Title) {
  CreateController();
  EXPECT_NE(std::u16string(), controller()->GetTitle());
}
