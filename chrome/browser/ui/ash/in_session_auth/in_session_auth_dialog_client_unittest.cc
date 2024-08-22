// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/in_session_auth/in_session_auth_dialog_client.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/in_session_auth_dialog_client.h"
#include "ash/public/cpp/webauthn_dialog_controller.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_cryptohome_misc_client.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::ash::Key;
using ::ash::UserContext;

namespace {

const char kPassword[] = "password";
const char kWrongPassword[] = "wrong_password";

const AccountId kAccountId = AccountId::FromUserEmail("testemail@example.com");

// InSessionAuthDialogClient's constructor expects to find an instance of
// ash::InSessionAuthDialogController, so provide a fake that does nothing.
class FakeInSessionAuthDialogController : public ash::WebAuthNDialogController {
 public:
  FakeInSessionAuthDialogController() = default;
  ~FakeInSessionAuthDialogController() override = default;

  // ash::InSessionAuthDialogController:
  void SetClient(ash::InSessionAuthDialogClient* client) override {}
  void ShowAuthenticationDialog(aura::Window* source_window,
                                const std::string& origin_name,
                                FinishCallback callback) override {}
  void DestroyAuthenticationDialog() override {}
  void AuthenticateUserWithPasswordOrPin(
      const std::string& password,
      bool authenticated_by_pin,
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
  InSessionAuthDialogClientTest() {
    ash::UserDataAuthClient::InitializeFake();
    ash::FakeUserDataAuthClient::TestApi::Get()->set_enable_auth_check(true);
    ash::CryptohomeMiscClient::InitializeFake();
    ash::SystemSaltGetter::Initialize();

    client_ = std::make_unique<InSessionAuthDialogClient>();
  }

  ~InSessionAuthDialogClientTest() override {
    ash::SystemSaltGetter::Shutdown();
    ash::CryptohomeMiscClient::Shutdown();
    ash::UserDataAuthClient::Shutdown();
  }

  void SetupActiveUser() {
    fake_user_manager_->AddUser(kAccountId);
    fake_user_manager_->LoginUser(kAccountId);
    auto* user = user_manager::UserManager::Get()->GetActiveUser();
    ASSERT_TRUE(user);
    // Set the profile mapping to avoid crashing in |OnPasswordAuthSuccess|.
    ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(user, nullptr);
  }

  void AuthenticateUserWithPasswordOrPin(
      const std::string& password,
      bool authenticated_by_pin,
      base::OnceCallback<void(bool)> callback) {
    client_->AuthenticateUserWithPasswordOrPin(password, authenticated_by_pin,
                                               std::move(callback));
  }

  void ConfigureExistingUserWithPassword(const AccountId& account_id,
                                         const std::string& password) {
    Key key(Key::KEY_TYPE_PASSWORD_PLAIN, std::string(), password);
    key.Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF,
                  ash::SystemSaltGetter::ConvertRawSaltToHexString(
                      ash::FakeCryptohomeMiscClient::GetStubSystemSalt()));

    user_data_auth::AuthFactor auth_factor;
    user_data_auth::AuthInput auth_input;

    auth_factor.set_label(ash::kCryptohomeGaiaKeyLabel);
    auth_factor.set_type(user_data_auth::AUTH_FACTOR_TYPE_PASSWORD);

    auth_input.mutable_password_input()->set_secret(key.GetSecret());

    // Add the password key to the user.
    auto* test_api = ash::FakeUserDataAuthClient::TestApi::Get();
    auto cryptohome_account_id =
        cryptohome::CreateAccountIdentifierFromAccountId(account_id);
    test_api->AddExistingUser(cryptohome_account_id);
    test_api->AddAuthFactor(cryptohome_account_id, auth_factor, auth_input);
  }

  void StartAuthSessionForActiveUser() {
    base::RunLoop run_loop;
    bool result = true;
    client_->StartAuthSession(base::BindLambdaForTesting(
        [&result](bool success) { result = success; }));
    run_loop.RunUntilIdle();
    EXPECT_TRUE(result);
  }

 protected:
  const content::BrowserTaskEnvironment task_environment_;

  ash::ScopedStubInstallAttributes install_attributes{
      ash::StubInstallAttributes::CreateConsumerOwned()};
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_{std::make_unique<ash::FakeChromeUserManager>()};
  std::unique_ptr<FakeInSessionAuthDialogController> fake_controller_{
      std::make_unique<FakeInSessionAuthDialogController>()};
  std::unique_ptr<InSessionAuthDialogClient> client_;
};

TEST_F(InSessionAuthDialogClientTest, WrongPassword) {
  SetupActiveUser();
  const user_manager::User* const user =
      user_manager::UserManager::Get()->GetActiveUser();
  UserContext expected_user_context(*user);
  Key key(Key::KEY_TYPE_PASSWORD_PLAIN, std::string(), kPassword);
  expected_user_context.SetKey(key);

  ConfigureExistingUserWithPassword(user->GetAccountId(), kPassword);
  StartAuthSessionForActiveUser();

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
      Key(Key::KEY_TYPE_PASSWORD_PLAIN, std::string(), kPassword));

  ConfigureExistingUserWithPassword(user->GetAccountId(), kPassword);
  StartAuthSessionForActiveUser();

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
