// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/in_session_auth_dialog_client.h"

#include "ash/public/cpp/in_session_auth_dialog_client.h"
#include "ash/public/cpp/in_session_auth_dialog_controller.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chromeos/login/auth/fake_extended_authenticator.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using chromeos::FakeExtendedAuthenticator;
using chromeos::Key;
using chromeos::UserContext;

namespace {

const char kPassword[] = "password";
const char kWrongPassword[] = "wrong_password";

const AccountId kAccountId = AccountId::FromUserEmail("testemail@example.com");

// InSessionAuthDialogClient's constructor expects to find an instance of
// ash::InSessionAuthDialogController, so provide a fake that does nothing.
class FakeInSessionAuthDialogController
    : public ash::InSessionAuthDialogController {
 public:
  FakeInSessionAuthDialogController() = default;
  ~FakeInSessionAuthDialogController() override = default;

  // ash::InSessionAuthDialogController:
  void SetClient(ash::InSessionAuthDialogClient* client) override {}
  void ShowAuthenticationDialog(aura::Window* source_window,
                                const std::string& origin_name,
                                FinishCallback callback) override {}
  void DestroyAuthenticationDialog() override {}
  void AuthenticateUserWithPin(const std::string& pin,
                               OnAuthenticateCallback callback) override {}
  void AuthenticateUserWithFingerprint(
      base::OnceCallback<void(bool, ash::FingerprintState)> callback) override {
  }
  void OpenInSessionAuthHelpPage() override {}
  void Cancel() override {}
  void CheckAvailability(
      FinishCallback on_availability_checked) const override {}
};

class InSessionAuthDialogClientTest : public testing::Test {
 public:
  InSessionAuthDialogClientTest() = default;
  ~InSessionAuthDialogClientTest() override = default;

  void SetupActiveUser() {
    fake_user_manager_->AddUser(kAccountId);
    fake_user_manager_->LoginUser(kAccountId);
    auto* user = user_manager::UserManager::Get()->GetActiveUser();
    ASSERT_TRUE(user);
    // Set the profile mapping to avoid crashing in |OnPasswordAuthSuccess|.
    chromeos::ProfileHelper::Get()->SetUserToProfileMappingForTesting(user,
                                                                      nullptr);
  }

  void SetExpectedContext(const UserContext& expected_user_context) {
    client_->SetExtendedAuthenticator(
        base::MakeRefCounted<FakeExtendedAuthenticator>(client_.get(),
                                                        expected_user_context));
  }

  void AuthenticateUserWithPasswordOrPin(
      const std::string& password,
      bool authenticated_by_pin,
      base::OnceCallback<void(bool)> callback) {
    client_->AuthenticateUserWithPasswordOrPin(password, authenticated_by_pin,
                                               std::move(callback));
  }

 private:
  // The ExtendedAuthenticator::AuthenticateToCheck task is posted to main (UI)
  // thread.
  const content::BrowserTaskEnvironment task_environment_;

  ash::FakeChromeUserManager* fake_user_manager_{
      new ash::FakeChromeUserManager()};
  user_manager::ScopedUserManager scoped_user_manager_{
      base::WrapUnique(fake_user_manager_)};
  std::unique_ptr<FakeInSessionAuthDialogController> fake_controller_{
      std::make_unique<FakeInSessionAuthDialogController>()};
  std::unique_ptr<InSessionAuthDialogClient> client_{
      std::make_unique<InSessionAuthDialogClient>()};
};

TEST_F(InSessionAuthDialogClientTest, WrongPassword) {
  SetupActiveUser();
  const user_manager::User* const user =
      user_manager::UserManager::Get()->GetActiveUser();
  UserContext expected_user_context(*user);
  expected_user_context.SetKey(
      Key(chromeos::Key::KEY_TYPE_PASSWORD_PLAIN, std::string(), kPassword));

  SetExpectedContext(expected_user_context);

  base::RunLoop run_loop;

  bool result = true;
  AuthenticateUserWithPasswordOrPin(
      kWrongPassword,
      /* authenticated_by_pin = */ false,
      base::BindLambdaForTesting(
          [&result](bool success) { result = success; }));

  run_loop.RunUntilIdle();
  EXPECT_FALSE(result);
}

TEST_F(InSessionAuthDialogClientTest, PasswordAuthSuccess) {
  SetupActiveUser();
  const user_manager::User* const user =
      user_manager::UserManager::Get()->GetActiveUser();
  UserContext expected_user_context(*user);
  expected_user_context.SetKey(
      Key(chromeos::Key::KEY_TYPE_PASSWORD_PLAIN, std::string(), kPassword));

  SetExpectedContext(expected_user_context);

  base::RunLoop run_loop;

  bool result = false;
  AuthenticateUserWithPasswordOrPin(
      kPassword,
      /* authenticated_by_pin = */ false,
      base::BindLambdaForTesting(
          [&result](bool success) { result = success; }));

  run_loop.RunUntilIdle();
  EXPECT_TRUE(result);
}

}  // namespace
