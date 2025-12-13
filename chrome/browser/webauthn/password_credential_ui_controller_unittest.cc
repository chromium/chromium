// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/password_credential_ui_controller.h"

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/shared_types.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/password_manager/core/browser/credential_manager_utils.h"
#include "components/password_manager/core/browser/mock_password_feature_manager.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  MockPasswordManagerClient() = default;
  ~MockPasswordManagerClient() override = default;

  MOCK_METHOD(const password_manager::MockPasswordFeatureManager*,
              GetPasswordFeatureManager,
              (),
              (const override));
};

class MockManagePasswordsUIController : public ManagePasswordsUIController {
 public:
  explicit MockManagePasswordsUIController(content::WebContents* web_contents)
      : ManagePasswordsUIController(web_contents) {}
  ~MockManagePasswordsUIController() override = default;

  MOCK_METHOD(void,
              AuthenticateUserWithMessage,
              (const std::u16string&, base::OnceCallback<void(bool)>),
              (override));
};

}  // namespace

class PasswordCredentialUIControllerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ON_CALL(client_, GetPasswordFeatureManager)
        .WillByDefault(testing::Return(&feature_manager_));
    model_ = base::MakeRefCounted<AuthenticatorRequestDialogModel>(
        web_contents()->GetPrimaryMainFrame());
    controller_ = std::make_unique<PasswordCredentialUIController>(
        web_contents()->GetPrimaryMainFrame()->GetGlobalId(), model_.get());
    controller_->SetPasswordManagerClientForTesting(&client_);
    web_contents()->SetUserData(
        ManagePasswordsUIController::UserDataKey(),
        std::make_unique<::testing::NiceMock<MockManagePasswordsUIController>>(
            web_contents()));
  }

 protected:
  const testing::NiceMock<MockManagePasswordsUIController>&
  mock_ui_controller() {
    return static_cast<testing::NiceMock<MockManagePasswordsUIController>&>(
        *web_contents()->GetUserData(
            ManagePasswordsUIController::UserDataKey()));
  }
  testing::NiceMock<password_manager::MockPasswordFeatureManager>
      feature_manager_;
  scoped_refptr<AuthenticatorRequestDialogModel> model_;
  std::unique_ptr<PasswordCredentialUIController> controller_;
  testing::NiceMock<MockPasswordManagerClient> client_;
};

TEST_F(PasswordCredentialUIControllerTest,
       OnPasswordCredentialSelected_NoAuth) {
  EXPECT_CALL(feature_manager_, IsBiometricAuthenticationBeforeFillingEnabled())
      .WillOnce(testing::Return(false));

  base::MockCallback<
      content::AuthenticatorRequestClientDelegate::PasswordSelectedCallback>
      callback;
  controller_->SetPasswordSelectedCallback(callback.Get());

  PasswordCredentialPair password = {u"user", u"pass"};
  EXPECT_CALL(callback, Run).Times(1);
  controller_->OnPasswordCredentialSelected(password);
}

TEST_F(PasswordCredentialUIControllerTest,
       OnPasswordCredentialSelected_WithAuth) {
  EXPECT_CALL(feature_manager_, IsBiometricAuthenticationBeforeFillingEnabled())
      .WillOnce(testing::Return(true));

  base::MockCallback<
      content::AuthenticatorRequestClientDelegate::PasswordSelectedCallback>
      callback;
  controller_->SetPasswordSelectedCallback(callback.Get());

  PasswordCredentialPair password = {u"user", u"pass"};
  EXPECT_CALL(callback, Run).Times(0);
  controller_->OnPasswordCredentialSelected(password);
  EXPECT_EQ(model_->step(),
            AuthenticatorRequestDialogModel::Step::kPasswordOsAuth);
}

TEST_F(PasswordCredentialUIControllerTest, OnStepTransition_OsAuth) {
  EXPECT_CALL(feature_manager_, IsBiometricAuthenticationBeforeFillingEnabled())
      .WillOnce(testing::Return(true));
  PasswordCredentialPair password = {u"user", u"pass"};
  controller_->OnPasswordCredentialSelected(password);
  ASSERT_EQ(model_->step(),
            AuthenticatorRequestDialogModel::Step::kPasswordOsAuth);

  EXPECT_CALL(mock_ui_controller(), AuthenticateUserWithMessage).Times(1);
  controller_->OnStepTransition();
}

TEST_F(PasswordCredentialUIControllerTest, OnAuthenticationCompleted_Success) {
  ON_CALL(client_, GetPasswordFeatureManager)
      .WillByDefault(testing::Return(&feature_manager_));
  EXPECT_CALL(feature_manager_, IsBiometricAuthenticationBeforeFillingEnabled())
      .WillOnce(testing::Return(true));
  PasswordCredentialPair password = {u"user", u"pass"};
  controller_->OnPasswordCredentialSelected(password);
  ASSERT_EQ(model_->step(),
            AuthenticatorRequestDialogModel::Step::kPasswordOsAuth);

  base::MockCallback<
      content::AuthenticatorRequestClientDelegate::PasswordSelectedCallback>
      callback;
  controller_->SetPasswordSelectedCallback(callback.Get());

  base::OnceCallback<void(bool)> auth_callback;
  EXPECT_CALL(mock_ui_controller(), AuthenticateUserWithMessage)
      .WillOnce([&](const std::u16string&, base::OnceCallback<void(bool)> cb) {
        auth_callback = std::move(cb);
      });
  controller_->OnStepTransition();

  EXPECT_CALL(callback, Run).Times(1);
  std::move(auth_callback).Run(true);
}

TEST_F(PasswordCredentialUIControllerTest, OnAuthenticationCompleted_Failure) {
  ON_CALL(client_, GetPasswordFeatureManager)
      .WillByDefault(testing::Return(&feature_manager_));
  EXPECT_CALL(feature_manager_, IsBiometricAuthenticationBeforeFillingEnabled())
      .WillOnce(testing::Return(true));
  PasswordCredentialPair password = {u"user", u"pass"};
  controller_->OnPasswordCredentialSelected(password);
  ASSERT_EQ(model_->step(),
            AuthenticatorRequestDialogModel::Step::kPasswordOsAuth);

  base::MockCallback<
      content::AuthenticatorRequestClientDelegate::PasswordSelectedCallback>
      callback;
  controller_->SetPasswordSelectedCallback(callback.Get());

  base::OnceCallback<void(bool)> auth_callback;
  EXPECT_CALL(mock_ui_controller(), AuthenticateUserWithMessage)
      .WillOnce([&](const std::u16string&, base::OnceCallback<void(bool)> cb) {
        auth_callback = std::move(cb);
      });
  controller_->OnStepTransition();

  EXPECT_CALL(callback, Run).Times(0);
  std::move(auth_callback).Run(false);
}
