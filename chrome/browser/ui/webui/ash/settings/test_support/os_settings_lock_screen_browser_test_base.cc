// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/test_support/os_settings_lock_screen_browser_test_base.h"

#include <string>

#include "ash/auth/active_session_auth_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/in_session_auth_dialog_controller.h"
#include "ash/shell.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/data/webui/chromeos/settings/test_api.test-mojom-test-utils.h"
#include "chromeos/ash/components/osauth/impl/auth_surface_registry.h"
#include "chromeos/ash/components/osauth/public/auth_engine_api.h"
#include "chromeos/ash/components/osauth/public/auth_parts.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash::settings {

OSSettingsLockScreenBrowserTestBase::OSSettingsLockScreenBrowserTestBase(
    ash::AshAuthFactor auth_factor_type)
    : auth_factor_type_(auth_factor_type) {
  // We configure FakeUserDataAuthClient (via `cryptohome_`) here and not
  // later because the global PinBackend object reads whether or not
  // cryptohome PINs are supported on startup. If we set up
  // FakeUserDataAuthClient in SetUpOnMainThread, then PinBackend would
  // determine whether PINs are supported before we have configured
  // FakeUserDataAuthClient.
  test::UserAuthConfig config;
  if (auth_factor_type_ == ash::AshAuthFactor::kGaiaPassword) {
    config.WithOnlinePassword(kPassword);
  } else if (auth_factor_type_ == ash::AshAuthFactor::kLocalPassword) {
    CHECK_EQ(auth_factor_type_, ash::AshAuthFactor::kLocalPassword);
    config.WithLocalPassword(kPassword);
  } else if (auth_factor_type_ == ash::AshAuthFactor::kCryptohomePin) {
    CHECK_EQ(auth_factor_type_, ash::AshAuthFactor::kCryptohomePin);
    config.WithCryptohomePin(kPin, kPinStubSalt);
  } else {
    NOTREACHED();
  }

  logged_in_user_mixin_ = std::make_unique<LoggedInUserMixin>(
      &mixin_host_, /*test_base=*/this, embedded_test_server(),
      LoggedInUserMixin::LogInType::kConsumer,
      /*include_initial_user=*/true,
      /*account_id=*/std::nullopt, config);
  cryptohome_ = &logged_in_user_mixin_->GetCryptohomeMixin();
  cryptohome_->set_enable_auth_check(true);
  cryptohome_->set_supports_low_entropy_credentials(true);
  cryptohome_->MarkUserAsExisting(GetAccountId());
}

OSSettingsLockScreenBrowserTestBase::~OSSettingsLockScreenBrowserTestBase() =
    default;

void OSSettingsLockScreenBrowserTestBase::SetUpOnMainThread() {
  MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  logged_in_user_mixin_->LogInUser();
}

mojom::LockScreenSettingsAsyncWaiter
OSSettingsLockScreenBrowserTestBase::OpenLockScreenSettings() {
  if (ash::features::IsUseAuthPanelInSessionEnabled()) {
    base::test::TestFuture<AuthSurfaceRegistry::AuthSurface> future;
    auto subscription =
        ash::AuthParts::Get()->GetAuthSurfaceRegistry()->RegisterShownCallback(
            future.GetCallback());

    auto os_settings_driver = OpenOSSettings();

    lock_screen_settings_remote_ =
        mojo::Remote(os_settings_driver.GoToLockScreenSettings());

    auto surface = future.Get();
    CHECK_EQ(surface, AuthSurfaceRegistry::AuthSurface::kInSession);

    base::RunLoop().RunUntilIdle();
  } else {
    auto os_settings_driver = OpenOSSettings();
    lock_screen_settings_remote_ =
        mojo::Remote(os_settings_driver.GoToLockScreenSettings());
  }

  return mojom::LockScreenSettingsAsyncWaiter(
      lock_screen_settings_remote_.get());
}

void OSSettingsLockScreenBrowserTestBase::Authenticate() {
  switch (auth_factor_type_) {
    case ash::AshAuthFactor::kCryptohomePin:
      AuthenticateUsingPin();
      break;
    case ash::AshAuthFactor::kGaiaPassword:
    case ash::AshAuthFactor::kLocalPassword:
      AuthenticateUsingPassword();
      break;
    default:
      NOTREACHED();
  }
}

void OSSettingsLockScreenBrowserTestBase::AuthenticateUsingPassword() {
  auto* controller = static_cast<ActiveSessionAuthControllerImpl*>(
      Shell::Get()->active_session_auth_controller());

  ActiveSessionAuthControllerImpl::TestApi(controller)
      .SubmitPassword(kPassword);

  base::RunLoop().RunUntilIdle();
}

void OSSettingsLockScreenBrowserTestBase::AuthenticateUsingPin() {
  auto* controller = static_cast<ActiveSessionAuthControllerImpl*>(
      Shell::Get()->active_session_auth_controller());

  ActiveSessionAuthControllerImpl::TestApi(controller).SubmitPin(kPin);

  base::RunLoop().RunUntilIdle();
}

mojom::LockScreenSettingsAsyncWaiter
OSSettingsLockScreenBrowserTestBase::OpenLockScreenSettingsAndAuthenticate() {
  if (ash::features::IsUseAuthPanelInSessionEnabled()) {
    OpenLockScreenSettings();
    Authenticate();
  } else {
    OpenLockScreenSettings().Authenticate(kPassword);
  }

  // The mojom AsyncWaiter classes have deleted copy constructors even though
  // they only hold a non-owning pointer to a mojo remote. This restriction
  // should probably be dropped, so that we can just return the async waiter
  // created by the call to `OpenLockScreenSettings` here. As a workaround, we
  // simply create a new waiter.
  return mojom::LockScreenSettingsAsyncWaiter{
      lock_screen_settings_remote_.get()};
}

mojom::LockScreenSettingsAsyncWaiter OSSettingsLockScreenBrowserTestBase::
    OpenLockScreenSettingsDeepLinkAndAuthenticate(
        const std::string& setting_id) {
  std::string relative_url = "/osPrivacy/lockScreen?settingId=";
  relative_url += setting_id;
  if (ash::features::IsUseAuthPanelInSessionEnabled()) {
    base::test::TestFuture<AuthSurfaceRegistry::AuthSurface> future;
    auto subscription =
        ash::AuthParts::Get()->GetAuthSurfaceRegistry()->RegisterShownCallback(
            future.GetCallback());

    auto os_settings_driver = OpenOSSettings(relative_url);

    auto surface = future.Get();
    CHECK_EQ(surface, AuthSurfaceRegistry::AuthSurface::kInSession);

    base::RunLoop().RunUntilIdle();

    Authenticate();

    lock_screen_settings_remote_ =
        mojo::Remote(os_settings_driver.AssertOnLockScreenSettings());
  } else {
    auto os_settings_driver = OpenOSSettings(relative_url);
    lock_screen_settings_remote_ =
        mojo::Remote(os_settings_driver.AssertOnLockScreenSettings());
    mojom::LockScreenSettingsAsyncWaiter(lock_screen_settings_remote_.get())
        .Authenticate(kPassword);
  }

  return mojom::LockScreenSettingsAsyncWaiter(
      lock_screen_settings_remote_.get());
}

const AccountId& OSSettingsLockScreenBrowserTestBase::GetAccountId() {
  return logged_in_user_mixin_->GetAccountId();
}

mojom::OSSettingsDriverAsyncWaiter
OSSettingsLockScreenBrowserTestBase::OpenOSSettings(
    const std::string& relative_url) {
  os_settings_driver_remote_ =
      mojo::Remote(os_settings_mixin_.OpenOSSettings(relative_url));
  return mojom::OSSettingsDriverAsyncWaiter(os_settings_driver_remote_.get());
}

}  // namespace ash::settings
