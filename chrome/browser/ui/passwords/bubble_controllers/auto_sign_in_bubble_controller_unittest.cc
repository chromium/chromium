// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/auto_sign_in_bubble_controller.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;
using ::testing::ReturnRef;

namespace {

constexpr char kSiteOrigin[] = "http://example.com/login/";
constexpr char16_t kUsername[] = u"Admin";
constexpr char16_t kPassword[] = u"AdminPass";
constexpr char kUIDismissalReasonGeneralMetric[] =
    "PasswordManager.UIDismissalReason";

}  // namespace

class AutoSignInBubbleControllerTest : public ::testing::Test {
 public:
  AutoSignInBubbleControllerTest() {
    mock_delegate_ =
        std::make_unique<testing::NiceMock<PasswordsModelDelegateMock>>();
    ON_CALL(*mock_delegate_, GetPasswordFormMetricsRecorder())
        .WillByDefault(Return(nullptr));
    pending_password_.url = GURL(kSiteOrigin);
    pending_password_.signon_realm = kSiteOrigin;
    pending_password_.username_value = kUsername;
    pending_password_.password_value = kPassword;
  }
  ~AutoSignInBubbleControllerTest() override = default;

  PasswordsModelDelegateMock* delegate() { return mock_delegate_.get(); }
  AutoSignInBubbleController* controller() { return controller_.get(); }

  const password_manager::PasswordForm& pending_password() const {
    return pending_password_;
  }

  void Init();
  void DestroyController();

 private:
  std::unique_ptr<PasswordsModelDelegateMock> mock_delegate_;
  std::unique_ptr<AutoSignInBubbleController> controller_;
  password_manager::PasswordForm pending_password_;
};

void AutoSignInBubbleControllerTest::Init() {
  EXPECT_CALL(*delegate(), GetPendingPassword())
      .WillOnce(ReturnRef(pending_password()));
  EXPECT_CALL(*delegate(), OnBubbleShown());
  controller_ =
      std::make_unique<AutoSignInBubbleController>(mock_delegate_->AsWeakPtr());
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(delegate()));
}

void AutoSignInBubbleControllerTest::DestroyController() {
  controller_.reset();
}

TEST_F(AutoSignInBubbleControllerTest, PopupAutoSigninToastWithBubbleClosing) {
  Init();

  controller()->OnAutoSignInToastTimeout();

  base::HistogramTester histogram_tester;

  EXPECT_CALL(*delegate(), OnBubbleHidden());
  controller()->OnBubbleClosing();

  DestroyController();

  histogram_tester.ExpectUniqueSample(
      kUIDismissalReasonGeneralMetric,
      password_manager::metrics_util::AUTO_SIGNIN_TOAST_TIMEOUT, 1);
}

TEST_F(AutoSignInBubbleControllerTest,
       PopupAutoSigninToastWithoutBubbleClosing) {
  Init();

  controller()->OnAutoSignInToastTimeout();

  base::HistogramTester histogram_tester;

  EXPECT_CALL(*delegate(), OnBubbleHidden());

  DestroyController();

  histogram_tester.ExpectUniqueSample(
      kUIDismissalReasonGeneralMetric,
      password_manager::metrics_util::AUTO_SIGNIN_TOAST_TIMEOUT, 1);
}
