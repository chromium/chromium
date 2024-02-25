// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/common_saved_account_manager_bubble_controller.h"

#include "base/test/task_environment.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;
using ::testing::ReturnRef;

namespace {

constexpr char kSiteOrigin[] = "http://example.com/";
constexpr char16_t kUsername[] = u"Admin";
constexpr char16_t kPassword[] = u"AdminPass";

}  // namespace

class StubCommonSavedAccountBubbleControllerClient
    : public CommonSavedAccountManagerBubbleController {
 public:
  StubCommonSavedAccountBubbleControllerClient(
      base::WeakPtr<PasswordsModelDelegate> delegate,
      DisplayReason display_reason,
      password_manager::metrics_util::UIDisplayDisposition display_disposition)
      : CommonSavedAccountManagerBubbleController(delegate,
                                                  display_reason,
                                                  display_disposition) {}
  ~StubCommonSavedAccountBubbleControllerClient() override {
    OnBubbleClosing();
  }
  std::u16string GetTitle() const override { return std::u16string(); }
  void ReportInteractions() override {}
};

class CommonSavedAccountManagerBubbleControllerTest : public ::testing::Test {
 public:
  CommonSavedAccountManagerBubbleControllerTest() {
    mock_delegate_ =
        std::make_unique<testing::NiceMock<PasswordsModelDelegateMock>>();

    pending_password_.url = GURL(kSiteOrigin);
    pending_password_.signon_realm = kSiteOrigin;
    pending_password_.username_value = kUsername;
    pending_password_.password_value = kPassword;
  }

  ~CommonSavedAccountManagerBubbleControllerTest() override {
    // Reset the delegate first. It can happen if the user closes the tab.
    mock_delegate_.reset();
    controller_.reset();
  }

  PasswordsModelDelegateMock* delegate() { return mock_delegate_.get(); }

  password_manager::PasswordForm& pending_password() {
    return pending_password_;
  }
  const password_manager::PasswordForm& pending_password() const {
    return pending_password_;
  }

  StubCommonSavedAccountBubbleControllerClient* controller() {
    return controller_.get();
  }

  void SetUpWithState(password_manager::ui::State state,
                      PasswordBubbleControllerBase::DisplayReason reason);
  void PretendPasswordWaiting(
      PasswordBubbleControllerBase::DisplayReason reason =
          PasswordBubbleControllerBase::DisplayReason::kAutomatic);

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<StubCommonSavedAccountBubbleControllerClient> controller_;
  std::unique_ptr<PasswordsModelDelegateMock> mock_delegate_;
  password_manager::PasswordForm pending_password_;
};

void CommonSavedAccountManagerBubbleControllerTest::SetUpWithState(
    password_manager::ui::State state,
    PasswordBubbleControllerBase::DisplayReason reason) {
  url::Origin origin = url::Origin::Create(GURL(kSiteOrigin));
  EXPECT_CALL(*delegate(), GetOrigin()).WillOnce(Return(origin));
  EXPECT_CALL(*delegate(), GetState()).WillRepeatedly(Return(state));
  controller_ = std::make_unique<StubCommonSavedAccountBubbleControllerClient>(
      mock_delegate_->AsWeakPtr(), reason,
      password_manager::metrics_util::UIDisplayDisposition::
          AUTOMATIC_WITH_PASSWORD_PENDING);
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(delegate()));
}

void CommonSavedAccountManagerBubbleControllerTest::PretendPasswordWaiting(
    PasswordBubbleControllerBase::DisplayReason reason) {
  EXPECT_CALL(*delegate(), GetPendingPassword())
      .WillOnce(ReturnRef(pending_password()));
  SetUpWithState(password_manager::ui::PENDING_PASSWORD_UPDATE_STATE, reason);
}

TEST_F(CommonSavedAccountManagerBubbleControllerTest, OnNoThanksClicked) {
  PretendPasswordWaiting();

  EXPECT_CALL(*delegate(), SavePassword(_, _)).Times(0);
  EXPECT_CALL(*delegate(), OnNopeUpdateClicked()).Times(1);
  controller()->OnNoThanksClicked();

  EXPECT_EQ(controller()->GetDismissalReason(),
            password_manager::metrics_util::CLICKED_CANCEL);
}

TEST_F(CommonSavedAccountManagerBubbleControllerTest, TextInputFieldsChanged) {
  PretendPasswordWaiting();

  const std::u16string kExpectedUsername = u"new_username";
  const std::u16string kExpectedPassword = u"new_password";

  controller()->OnCredentialEdited(kExpectedUsername, kExpectedPassword);
  EXPECT_EQ(kExpectedUsername, controller()->pending_password().username_value);
  EXPECT_EQ(kExpectedPassword, controller()->pending_password().password_value);
}

TEST_F(CommonSavedAccountManagerBubbleControllerTest,
       NavigateToPasswordManagerSettingsPage) {
  PretendPasswordWaiting();
  EXPECT_CALL(*delegate(), NavigateToPasswordManagerSettingsPage);

  controller()->OnGooglePasswordManagerLinkClicked(
      password_manager::ManagePasswordsReferrer::kAddUsernameBubble);
}
