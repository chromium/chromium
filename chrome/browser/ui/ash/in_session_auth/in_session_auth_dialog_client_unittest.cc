// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/in_session_auth/in_session_auth_dialog_client.h"

#include <memory>

#include "ash/public/cpp/in_session_auth_dialog_client.h"
#include "ash/public/cpp/webauthn_dialog_controller.h"
#include "base/check_deref.h"
#include "base/functional/callback.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/ash/login/test/chrome_user_session_test_environment_delegate.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_cryptohome_misc_client.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "components/account_id/account_id.h"
#include "components/account_id/account_id_literal.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::ash::Key;

namespace {

const char kPassword[] = "password";
const char kWrongPassword[] = "wrong_password";

constexpr AccountId::Literal kAccountId =
    AccountId::Literal::FromUserEmailGaiaId("testemail@example.com",
                                            GaiaId::Literal("12345"));

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
  void SetUp() override {
    ash::UserDataAuthClient::InitializeFake();
    ash::FakeUserDataAuthClient::TestApi::Get()->set_enable_auth_check(true);
    ash::CryptohomeMiscClient::InitializeFake();
    ash::SystemSaltGetter::Initialize();

    auto* const browser_process = TestingBrowserProcess::GetGlobal();
    user_session_test_environment_ = std::make_unique<
        ash::test::UserSessionTestEnvironment>(
        browser_process->local_state(),
        std::make_unique<ash::test::ChromeUserSessionTestEnvironmentDelegate>(
            browser_process));
    ASSERT_TRUE(user_session_test_environment_->AddRegularUser(kAccountId));
    user_session_test_environment_->LogIn(kAccountId);

    fake_controller_ = std::make_unique<FakeInSessionAuthDialogController>();
    client_ = std::make_unique<InSessionAuthDialogClient>(
        browser_process->local_state());
  }

  void TearDown() override {
    client_.reset();
    fake_controller_.reset();
    user_session_test_environment_.reset();
    ash::SystemSaltGetter::Shutdown();
    ash::CryptohomeMiscClient::Shutdown();
    ash::UserDataAuthClient::Shutdown();
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
    base::test::TestFuture<bool> future;
    client_->StartAuthSession(kAccountId, future.GetCallback());
    EXPECT_TRUE(future.Get());
  }

 protected:
  const content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<ash::test::UserSessionTestEnvironment>
      user_session_test_environment_;
  std::unique_ptr<FakeInSessionAuthDialogController> fake_controller_;
  std::unique_ptr<InSessionAuthDialogClient> client_;
};

TEST_F(InSessionAuthDialogClientTest, WrongPassword) {
  ConfigureExistingUserWithPassword(kAccountId, kPassword);
  StartAuthSessionForActiveUser();

  base::test::TestFuture<bool> future;
  AuthenticateUserWithPasswordOrPin(kWrongPassword,
                                    /*authenticated_by_pin=*/false,
                                    future.GetCallback());

  EXPECT_FALSE(future.Get());
}

TEST_F(InSessionAuthDialogClientTest, PasswordAuthSuccess) {
  ConfigureExistingUserWithPassword(kAccountId, kPassword);
  StartAuthSessionForActiveUser();

  // Authenticating with a password is what grants strong auth.
  ash::quick_unlock::QuickUnlockStorage& quick_unlock_storage = CHECK_DEREF(
      ash::quick_unlock::QuickUnlockFactory::GetForAccountId(kAccountId));
  EXPECT_FALSE(quick_unlock_storage.HasStrongAuth());

  base::test::TestFuture<bool> future;
  AuthenticateUserWithPasswordOrPin(kPassword,
                                    /*authenticated_by_pin=*/false,
                                    future.GetCallback());

  EXPECT_TRUE(future.Get());
  EXPECT_TRUE(quick_unlock_storage.HasStrongAuth());
}

}  // namespace
