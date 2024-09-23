// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/profile_oauth2_token_service_builder.h"

#include <string>
#include <utility>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/device_id_helper.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/base/signin_switches.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate_android.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "components/signin/internal/identity_manager/mutable_profile_oauth2_token_service_delegate.h"
#include "components/signin/public/webdata/token_web_data.h"
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#include "components/signin/internal/identity_manager/token_binding_helper.h"
#include "components/unexportable_keys/unexportable_key_service.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate_chromeos.h"
#endif

#if BUILDFLAG(IS_IOS)
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate_ios.h"
#include "components/signin/public/identity_manager/ios/device_accounts_provider.h"
#endif

namespace {

#if BUILDFLAG(IS_ANDROID)
// TODO(crbug.com/40637107) Provide AccountManagerFacade as a parameter once
// IdentityServicesProvider owns its instance management.
std::unique_ptr<ProfileOAuth2TokenServiceDelegateAndroid>
CreateAndroidOAuthDelegate(AccountTrackerService* account_tracker_service) {
  return std::make_unique<ProfileOAuth2TokenServiceDelegateAndroid>(
      account_tracker_service);
}
#elif BUILDFLAG(IS_IOS)
std::unique_ptr<ProfileOAuth2TokenServiceIOSDelegate> CreateIOSOAuthDelegate(
    SigninClient* signin_client,
    std::unique_ptr<DeviceAccountsProvider> device_accounts_provider,
    AccountTrackerService* account_tracker_service) {
  return std::make_unique<ProfileOAuth2TokenServiceIOSDelegate>(
      signin_client, std::move(device_accounts_provider),
      account_tracker_service);
}
#elif BUILDFLAG(IS_CHROMEOS)
std::unique_ptr<ProfileOAuth2TokenServiceDelegate> CreateCrOsOAuthDelegate(
    SigninClient* signin_client,
    AccountTrackerService* account_tracker_service,
    network::NetworkConnectionTracker* network_connection_tracker,
    account_manager::AccountManagerFacade* account_manager_facade,
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    bool delete_signin_cookies_on_exit,
#endif
    bool is_regular_profile) {
  return std::make_unique<signin::ProfileOAuth2TokenServiceDelegateChromeOS>(
      signin_client, account_tracker_service, network_connection_tracker,
      account_manager_facade,
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      delete_signin_cookies_on_exit,
#endif
      is_regular_profile);
}
#elif BUILDFLAG(ENABLE_DICE_SUPPORT)

std::unique_ptr<MutableProfileOAuth2TokenServiceDelegate>
CreateMutableProfileOAuthDelegate(
    AccountTrackerService* account_tracker_service,
    signin::AccountConsistencyMethod account_consistency,
    bool delete_signin_cookies_on_exit,
    scoped_refptr<TokenWebData> token_web_data,
    SigninClient* signin_client,
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    unexportable_keys::UnexportableKeyService* unexportable_key_service,
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#if BUILDFLAG(IS_WIN)
    MutableProfileOAuth2TokenServiceDelegate::FixRequestErrorCallback
        reauth_callback,
#endif
    network::NetworkConnectionTracker* network_connection_tracker) {
  // When signin cookies are cleared on exit and Dice is enabled, all tokens
  // should also be cleared.
  RevokeAllTokensOnLoad revoke_all_tokens_on_load =
      (account_consistency == signin::AccountConsistencyMethod::kDice) &&
              delete_signin_cookies_on_exit
          ? RevokeAllTokensOnLoad::kDeleteSiteDataOnExit
          : RevokeAllTokensOnLoad::kNo;

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  std::unique_ptr<TokenBindingHelper> token_binding_helper;
  if (unexportable_key_service) {
    token_binding_helper =
        std::make_unique<TokenBindingHelper>(*unexportable_key_service);
  }
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

  return std::make_unique<MutableProfileOAuth2TokenServiceDelegate>(
      signin_client, account_tracker_service, network_connection_tracker,
      token_web_data, account_consistency, revoke_all_tokens_on_load,
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
      std::move(token_binding_helper),
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#if BUILDFLAG(IS_WIN)
      reauth_callback
#else
      MutableProfileOAuth2TokenServiceDelegate::FixRequestErrorCallback()
#endif  // BUILDFLAG(IS_WIN)
  );
}
#endif  // BUILDFLAG(IS_ANDROID)

std::unique_ptr<ProfileOAuth2TokenServiceDelegate>
CreateOAuth2TokenServiceDelegate(
    AccountTrackerService* account_tracker_service,
    signin::AccountConsistencyMethod account_consistency,
    SigninClient* signin_client,
#if BUILDFLAG(IS_CHROMEOS)
    account_manager::AccountManagerFacade* account_manager_facade,
    bool is_regular_profile,
#endif  // BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
    bool delete_signin_cookies_on_exit,
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    scoped_refptr<TokenWebData> token_web_data,
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    unexportable_keys::UnexportableKeyService* unexportable_key_service,
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#endif
#if BUILDFLAG(IS_IOS)
    std::unique_ptr<DeviceAccountsProvider> device_accounts_provider,
#endif
#if BUILDFLAG(IS_WIN)
    MutableProfileOAuth2TokenServiceDelegate::FixRequestErrorCallback
        reauth_callback,
#endif
    network::NetworkConnectionTracker* network_connection_tracker) {
#if BUILDFLAG(IS_ANDROID)
  return CreateAndroidOAuthDelegate(account_tracker_service);
#elif BUILDFLAG(IS_IOS)
  return CreateIOSOAuthDelegate(signin_client,
                                std::move(device_accounts_provider),
                                account_tracker_service);
#elif BUILDFLAG(IS_CHROMEOS)
  return CreateCrOsOAuthDelegate(signin_client, account_tracker_service,
                                 network_connection_tracker,
                                 account_manager_facade,
#if BUILDFLAG(IS_CHROMEOS_LACROS)
                                 delete_signin_cookies_on_exit,
#endif
                                 is_regular_profile);
#elif BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Fall back to |MutableProfileOAuth2TokenServiceDelegate| on all platforms
  // other than Android, iOS, and Chrome OS (Ash and Lacros).
  return CreateMutableProfileOAuthDelegate(
      account_tracker_service, account_consistency,
      delete_signin_cookies_on_exit, token_web_data, signin_client,
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
      unexportable_key_service,
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#if BUILDFLAG(IS_WIN)
      reauth_callback,
#endif  // BUILDFLAG(IS_WIN)
      network_connection_tracker);
#else
  NOTREACHED_IN_MIGRATION();
  return nullptr;
#endif  // BUILDFLAG(IS_ANDROID)
}

}  // namespace

std::unique_ptr<ProfileOAuth2TokenService> BuildProfileOAuth2TokenService(
    PrefService* pref_service,
    AccountTrackerService* account_tracker_service,
    network::NetworkConnectionTracker* network_connection_tracker,
    signin::AccountConsistencyMethod account_consistency,
#if BUILDFLAG(IS_CHROMEOS)
    account_manager::AccountManagerFacade* account_manager_facade,
    bool is_regular_profile,
#endif  // BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
    bool delete_signin_cookies_on_exit,
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    scoped_refptr<TokenWebData> token_web_data,
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    unexportable_keys::UnexportableKeyService* unexportable_key_service,
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
#if BUILDFLAG(IS_IOS)
    std::unique_ptr<DeviceAccountsProvider> device_accounts_provider,
#endif
#if BUILDFLAG(IS_WIN)
    MutableProfileOAuth2TokenServiceDelegate::FixRequestErrorCallback
        reauth_callback,
#endif
    SigninClient* signin_client) {
// On ChromeOS the device ID is not managed by the token service.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Ensure the device ID is not empty. This is important for Dice, because the
  // device ID is needed on the network thread, but can only be generated on the
  // main thread.
  std::string device_id = signin::GetSigninScopedDeviceId(pref_service);
  DCHECK(!device_id.empty());
#endif

  return std::make_unique<ProfileOAuth2TokenService>(
      pref_service,
      CreateOAuth2TokenServiceDelegate(
          account_tracker_service, account_consistency, signin_client,
#if BUILDFLAG(IS_CHROMEOS)
          account_manager_facade, is_regular_profile,
#endif  // BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
          delete_signin_cookies_on_exit,
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
          token_web_data,
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
          unexportable_key_service,
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#endif
#if BUILDFLAG(IS_IOS)
          std::move(device_accounts_provider),
#endif
#if BUILDFLAG(IS_WIN)
          reauth_callback,
#endif
          network_connection_tracker));
}
