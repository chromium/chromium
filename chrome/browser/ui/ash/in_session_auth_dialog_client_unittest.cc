// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/in_session_auth_dialog_client.h"

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
#include "chromeos/ash/components/login/auth/fake_extended_authenticator.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::ash::FakeExtendedAuthenticator;
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

  void SetExpectedContext(const UserContext& expected_user_context) {
    fake_authenticator_ = base::MakeRefCounted<FakeExtendedAuthenticator>(
        client_.get(), expected_user_context);
    client_->SetExtendedAuthenticator(fake_authenticator_);
  }

  void AuthenticateUserWithPasswordOrPin(
      const std::string& password,
      bool authenticated_by_pin,
      base::OnceCallback<void(bool)> callback) {
    client_->AuthenticateUserWithPasswordOrPin(password, authenticated_by_pin,
                                               std::move(callback));
  }

  bool GetLastUnlockWebAuthnSecret() const {
    return fake_authenticator_->last_unlock_webauthn_secret();
  }

  void ConfigureExistingUserWithPassword(const AccountId& user,
                                         const std::string& password) {
    Key key(Key::KEY_TYPE_PASSWORD_PLAIN, std::string(), password);
    key.Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF,
                  ash::SystemSaltGetter::ConvertRawSaltToHexString(
                      ash::FakeCryptohomeMiscClient::GetStubSystemSalt()));

    cryptohome::Key cryptohome_key;
    cryptohome_key.mutable_data()->set_label(ash::kCryptohomeGaiaKeyLabel);
    cryptohome_key.set_secret(key.GetSecret());

    auto* test_api = ash::FakeUserDataAuthClient::TestApi::Get();
    auto account_id = cryptohome::CreateAccountIdentifierFromAccountId(user);
    test_api->AddExistingUser(account_id);
    test_api->AddKey(account_id, cryptohome_key);
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
  // The ExtendedAuthenticator::AuthenticateToCheck task is posted to main (UI)
  // thread.
  const content::BrowserTaskEnvironment task_environment_;

  ash::FakeChromeUserManager* fake_user_manager_{
      new ash::FakeChromeUserManager()};
  user_manager::ScopedUserManager scoped_user_manager_{
      base::WrapUnique(fake_user_manager_)};
  std::unique_ptr<FakeInSessionAuthDialogController> fake_controller_{
      std::make_unique<FakeInSessionAuthDialogController>()};
  std::unique_ptr<InSessionAuthDialogClient> client_;
  scoped_refptr<ash::FakeExtendedAuthenticator> fake_authenticator_;
};

TEST_F(InSessionAuthDialogClientTest, WrongPassword) {
  SetupActiveUser();
  const user_manager::User* const user =
      user_manager::UserManager::Get()->GetActiveUser();
  UserContext expected_user_context(*user);
  Key key(Key::KEY_TYPE_PASSWORD_PLAIN, std::string(), kPassword);
  expected_user_context.SetKey(key);

  SetExpectedContext(expected_user_context);
  if (ash::features::IsUseAuthsessionForWebAuthNEnabled()) {
    ConfigureExistingUserWithPassword(user->GetAccountId(), kPassword);
    StartAuthSessionForActiveUser();
  };

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

  SetExpectedContext(expected_user_context);

  if (ash::features::IsUseAuthsessionForWebAuthNEnabled()) {
    ConfigureExistingUserWithPassword(user->GetAccountId(), kPassword);
    StartAuthSessionForActiveUser();
  };

  base::RunLoop run_loop;

  bool result = false;
  AuthenticateUserWithPasswordOrPin(
      kPassword,
      /* authenticated_by_pin = */ false,
      base::BindLambdaForTesting(
          [&result](bool success) { result = success; }));

  run_loop.RunUntilIdle();
  EXPECT_TRUE(result);
  if (!ash::features::IsUseAuthsessionForWebAuthNEnabled()) {
    EXPECT_TRUE(GetLastUnlockWebAuthnSecret());
  }
}

}  // namespace
