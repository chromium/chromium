// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/ambient/ambient_signin_controller.h"

#include <memory>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/webauthn/ambient/ambient_login_permission_controller.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "url/gurl.h"

namespace ambient_signin {

class AmbientSigninControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL("https://example.com"));

    permissions::PermissionRequestManager::CreateForWebContents(web_contents());
    AmbientLoginPermissionController::CreateForPage(
        web_contents()->GetPrimaryPage());

    prompt_factory_ =
        std::make_unique<permissions::MockPermissionPromptFactory>(
            permission_request_manager());

    AmbientSigninController::CreateForCurrentDocument(main_rfh());
  }

  void TearDown() override {
    prompt_factory_ = nullptr;
    DeleteContents();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  AmbientSigninController* controller() {
    return AmbientSigninController::GetForCurrentDocument(main_rfh());
  }

  AmbientLoginPermissionController* permission_controller() {
    return AmbientLoginPermissionController::GetForPage(
        web_contents()->GetPrimaryPage());
  }

  permissions::PermissionRequestManager* permission_request_manager() {
    return permissions::PermissionRequestManager::FromWebContents(
        web_contents());
  }

 protected:
  std::unique_ptr<permissions::MockPermissionPromptFactory> prompt_factory_;
};

TEST_F(AmbientSigninControllerTest, ShowSinglePasskey) {
  auto model =
      base::MakeRefCounted<AuthenticatorRequestDialogModel>(main_rfh());
  model->relying_party_id = "example.com";
  base::MockRepeatingClosure passkey_callback;
  model->mechanisms.emplace_back(
      AuthenticatorRequestDialogModel::Mechanism::Credential(
          {device::AuthenticatorType::kEnclave, {4, 5, 6}, std::nullopt}),
      u"username",
      ::features::IsRoundedIconsEnabled() ? vector_icons::kPasskeyIcon
                                          : vector_icons::kPasskeyOldIcon,
      passkey_callback.Get());

  controller()->Show(model.get());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return permission_request_manager()->IsRequestInProgress(); }));

  base::RunLoop run_loop;
  permission_controller()->SetFinishedNotificationForTesting(
      run_loop.QuitClosure());
  EXPECT_CALL(passkey_callback, Run());
  permission_request_manager()->Accept(std::monostate());
  run_loop.Run();
}

TEST_F(AmbientSigninControllerTest, ShowSinglePassword) {
  auto model =
      base::MakeRefCounted<AuthenticatorRequestDialogModel>(main_rfh());
  model->relying_party_id = "example.com";
  base::MockRepeatingClosure password_callback;
  model->mechanisms.emplace_back(
      AuthenticatorRequestDialogModel::Mechanism::Password(
          AuthenticatorRequestDialogModel::Mechanism::PasswordInfo(
              std::nullopt)),
      u"username",
      ::features::IsRoundedIconsEnabled() ? kPasswordIcon
                                          : kPasswordFieldOldIcon,
      password_callback.Get());

  controller()->Show(model.get());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return permission_request_manager()->IsRequestInProgress(); }));

  base::RunLoop run_loop;
  permission_controller()->SetFinishedNotificationForTesting(
      run_loop.QuitClosure());
  EXPECT_CALL(password_callback, Run()).WillOnce([&model]() {
    model->OnRequestComplete();
  });
  permission_request_manager()->Accept(std::monostate());
  run_loop.Run();
}

TEST_F(AmbientSigninControllerTest, ShowMultipleCredentials) {
  auto model =
      base::MakeRefCounted<AuthenticatorRequestDialogModel>(main_rfh());
  model->relying_party_id = "example.com";
  base::MockRepeatingClosure passkey_callback;
  base::MockRepeatingClosure password_callback;
  model->mechanisms.emplace_back(
      AuthenticatorRequestDialogModel::Mechanism::Credential(
          {device::AuthenticatorType::kEnclave, {4, 5, 6}, std::nullopt}),
      u"alice@example.com",
      ::features::IsRoundedIconsEnabled() ? vector_icons::kPasskeyIcon
                                          : vector_icons::kPasskeyOldIcon,
      passkey_callback.Get(), u"Alice Smith");
  model->mechanisms.emplace_back(
      AuthenticatorRequestDialogModel::Mechanism::Password(
          AuthenticatorRequestDialogModel::Mechanism::PasswordInfo(
              std::nullopt)),
      u"bob@example.com",
      ::features::IsRoundedIconsEnabled() ? kPasswordIcon
                                          : kPasswordFieldOldIcon,
      password_callback.Get());

  controller()->Show(model.get());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return permission_request_manager()->IsRequestInProgress(); }));

  ASSERT_FALSE(permission_request_manager()->Requests().empty());
  auto* ambient_request = static_cast<AmbientLoginPermissionRequest*>(
      permission_request_manager()->Requests().front().get());
  ASSERT_EQ(ambient_request->credentials().size(), 2u);
  EXPECT_EQ(ambient_request->credentials()[0].username, u"alice@example.com");
  EXPECT_EQ(ambient_request->credentials()[0].display_name, u"Alice Smith");
  EXPECT_EQ(ambient_request->credentials()[0].type, CredentialType::kPasskey);
  EXPECT_EQ(ambient_request->credentials()[1].username, u"bob@example.com");
  EXPECT_EQ(ambient_request->credentials()[1].type, CredentialType::kPassword);

  base::RunLoop run_loop;
  permission_controller()->SetFinishedNotificationForTesting(
      run_loop.QuitClosure());
  ambient_request->SelectCredential(1);
  EXPECT_EQ(permission_controller()->state(),
            AmbientLoginPermissionController::State::kRequestAdded);

  EXPECT_CALL(password_callback, Run()).WillOnce([&model]() {
    model->OnRequestComplete();
  });
  permission_request_manager()->Accept(std::monostate());
  run_loop.Run();
}

TEST_F(AmbientSigninControllerTest, DismissPromptClosesUIWithoutSigningIn) {
  auto model =
      base::MakeRefCounted<AuthenticatorRequestDialogModel>(main_rfh());
  model->relying_party_id = "example.com";
  base::MockRepeatingClosure passkey_callback;
  model->mechanisms.emplace_back(
      AuthenticatorRequestDialogModel::Mechanism::Credential(
          {device::AuthenticatorType::kEnclave, {4, 5, 6}, std::nullopt}),
      u"username",
      ::features::IsRoundedIconsEnabled() ? vector_icons::kPasskeyIcon
                                          : vector_icons::kPasskeyOldIcon,
      passkey_callback.Get());

  controller()->Show(model.get());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return permission_request_manager()->IsRequestInProgress(); }));

  base::RunLoop run_loop;
  permission_controller()->SetFinishedNotificationForTesting(
      run_loop.QuitClosure());
  EXPECT_CALL(passkey_callback, Run()).Times(0);
  permission_request_manager()->Dismiss(std::monostate());
  run_loop.Run();

  EXPECT_EQ(permission_controller()->state(),
            AmbientLoginPermissionController::State::kIdle);
}

TEST_F(AmbientSigninControllerTest, OnRequestCompleteClosesUI) {
  auto model =
      base::MakeRefCounted<AuthenticatorRequestDialogModel>(main_rfh());
  model->relying_party_id = "example.com";
  model->mechanisms.emplace_back(
      AuthenticatorRequestDialogModel::Mechanism::Credential(
          {device::AuthenticatorType::kEnclave, {4, 5, 6}, std::nullopt}),
      u"username",
      ::features::IsRoundedIconsEnabled() ? vector_icons::kPasskeyIcon
                                          : vector_icons::kPasskeyOldIcon,
      base::DoNothing());

  controller()->Show(model.get());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return permission_request_manager()->IsRequestInProgress(); }));

  base::RunLoop run_loop;
  permission_controller()->SetFinishedNotificationForTesting(
      run_loop.QuitClosure());
  model->OnRequestComplete();
  run_loop.Run();

  EXPECT_EQ(permission_controller()->state(),
            AmbientLoginPermissionController::State::kIdle);
}

TEST_F(AmbientSigninControllerTest,
       SubsequentRequestWithNewModelAfterRequestComplete) {
  scoped_refptr<AuthenticatorRequestDialogModel> model1 =
      base::MakeRefCounted<AuthenticatorRequestDialogModel>(main_rfh());
  model1->relying_party_id = "example.com";
  model1->mechanisms.emplace_back(
      AuthenticatorRequestDialogModel::Mechanism::Credential(
          {device::AuthenticatorType::kEnclave, {4, 5, 6}, std::nullopt}),
      u"username",
      ::features::IsRoundedIconsEnabled() ? vector_icons::kPasskeyIcon
                                          : vector_icons::kPasskeyOldIcon,
      base::DoNothing());

  controller()->Show(model1.get());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return permission_request_manager()->IsRequestInProgress(); }));

  base::RunLoop first_run_loop;
  permission_controller()->SetFinishedNotificationForTesting(
      first_run_loop.QuitClosure());
  model1->OnRequestComplete();
  first_run_loop.Run();

  // Even while `model1` is still alive, a new request with `model2` should
  // succeed and display the prompt.
  scoped_refptr<AuthenticatorRequestDialogModel> model2 =
      base::MakeRefCounted<AuthenticatorRequestDialogModel>(main_rfh());
  model2->relying_party_id = "example.com";
  model2->mechanisms.emplace_back(
      AuthenticatorRequestDialogModel::Mechanism::Credential(
          {device::AuthenticatorType::kEnclave, {4, 5, 6}, std::nullopt}),
      u"username",
      ::features::IsRoundedIconsEnabled() ? vector_icons::kPasskeyIcon
                                          : vector_icons::kPasskeyOldIcon,
      base::DoNothing());

  controller()->Show(model2.get());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return permission_request_manager()->IsRequestInProgress(); }));

  base::RunLoop second_run_loop;
  permission_controller()->SetFinishedNotificationForTesting(
      second_run_loop.QuitClosure());
  model2->OnRequestComplete();
  second_run_loop.Run();
}

TEST_F(AmbientSigninControllerTest, ControllerDestroyedBeforeModelClosesUI) {
  scoped_refptr<AuthenticatorRequestDialogModel> model =
      base::MakeRefCounted<AuthenticatorRequestDialogModel>(main_rfh());
  model->relying_party_id = "example.com";
  model->mechanisms.emplace_back(
      AuthenticatorRequestDialogModel::Mechanism::Credential(
          {device::AuthenticatorType::kEnclave, {4, 5, 6}, std::nullopt}),
      u"username",
      ::features::IsRoundedIconsEnabled() ? vector_icons::kPasskeyIcon
                                          : vector_icons::kPasskeyOldIcon,
      base::DoNothing());

  controller()->Show(model.get());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return permission_request_manager()->IsRequestInProgress(); }));

  base::RunLoop run_loop;
  permission_controller()->SetFinishedNotificationForTesting(
      run_loop.QuitClosure());
  AmbientSigninController::DeleteForCurrentDocument(main_rfh());
  run_loop.Run();

  EXPECT_FALSE(permission_request_manager()->IsRequestInProgress());
  EXPECT_EQ(permission_controller()->state(),
            AmbientLoginPermissionController::State::kIdle);
}

}  // namespace ambient_signin
