// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/identity_manager_builder.h"

#include <string>
#include <utility>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/prefs/pref_service.h"
#include "components/signin/internal/identity_manager/account_capabilities_fetcher_factory.h"
#include "components/signin/internal/identity_manager/account_fetcher_service.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/internal/identity_manager/accounts_cookie_mutator_impl.h"
#include "components/signin/internal/identity_manager/diagnostics_provider_impl.h"
#include "components/signin/internal/identity_manager/gaia_cookie_manager_service.h"
#include "components/signin/internal/identity_manager/primary_account_manager.h"
#include "components/signin/internal/identity_manager/primary_account_mutator_impl.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_builder.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/device_accounts_synchronizer.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/signin/internal/identity_manager/account_capabilities_fetcher_factory_android.h"
#else
#include "components/signin/internal/identity_manager/account_capabilities_fetcher_factory_gaia.h"
#include "components/signin/public/webdata/token_web_data.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_IOS)
#include "components/signin/public/identity_manager/ios/device_accounts_provider.h"
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
#include "components/signin/internal/identity_manager/device_accounts_synchronizer_impl.h"
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#include "components/signin/internal/identity_manager/accounts_mutator_impl.h"
#endif

namespace signin {

namespace {

std::unique_ptr<AccountTrackerService> BuildAccountTrackerService(
    PrefService* pref_service,
    base::FilePath profile_path) {
  auto account_tracker_service = std::make_unique<AccountTrackerService>();
  account_tracker_service->Initialize(pref_service, profile_path);
  return account_tracker_service;
}

std::unique_ptr<PrimaryAccountManager> BuildPrimaryAccountManager(
    SigninClient* client,
    AccountTrackerService* account_tracker_service,
    ProfileOAuth2TokenService* token_service) {
  return std::make_unique<PrimaryAccountManager>(client, token_service,
                                                 account_tracker_service);
}

std::unique_ptr<AccountsMutator> BuildAccountsMutator(
    PrefService* prefs,
    AccountTrackerService* account_tracker_service,
    ProfileOAuth2TokenService* token_service,
    PrimaryAccountManager* primary_account_manager) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  return std::make_unique<AccountsMutatorImpl>(
      token_service, account_tracker_service, primary_account_manager, prefs);
#else
  return nullptr;
#endif
}

std::unique_ptr<AccountFetcherService> BuildAccountFetcherService(
    SigninClient* signin_client,
    ProfileOAuth2TokenService* token_service,
    AccountTrackerService* account_tracker_service,
    std::unique_ptr<image_fetcher::ImageDecoder> image_decoder,
    std::unique_ptr<AccountCapabilitiesFetcherFactory>
        account_capabilities_fetcher_factory) {
  auto account_fetcher_service = std::make_unique<AccountFetcherService>();
  account_fetcher_service->Initialize(
      signin_client, token_service, account_tracker_service,
      std::move(image_decoder),
      std::move(account_capabilities_fetcher_factory));
  return account_fetcher_service;
}

}  // anonymous namespace

IdentityManagerBuildParams::IdentityManagerBuildParams() = default;

IdentityManagerBuildParams::~IdentityManagerBuildParams() = default;

IdentityManager::InitParameters BuildIdentityManagerInitParameters(
    IdentityManagerBuildParams* params) {
  std::unique_ptr<AccountTrackerService> account_tracker_service =
      std::move(params->account_tracker_service);
  if (!account_tracker_service) {
    account_tracker_service =
        BuildAccountTrackerService(params->pref_service, params->profile_path);
  }

  std::unique_ptr<ProfileOAuth2TokenService> token_service =
      std::move(params->token_service);
  if (!token_service) {
    token_service = BuildProfileOAuth2TokenService(
        params->pref_service, account_tracker_service.get(),
        params->network_connection_tracker, params->account_consistency,
#if BUILDFLAG(IS_CHROMEOS)
        params->account_manager_facade, params->is_regular_profile,
#endif  // BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
        params->delete_signin_cookies_on_exit,
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT) ||  BUILDFLAG(IS_CHROMEOS_LACROS)
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
        params->token_web_data,
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
        params->unexportable_key_service,
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
#if BUILDFLAG(IS_IOS)
        std::move(params->device_accounts_provider),
#endif
#if BUILDFLAG(IS_WIN)
        params->reauth_callback,
#endif
        params->signin_client);
  }

  auto gaia_cookie_manager_service = std::make_unique<GaiaCookieManagerService>(
      account_tracker_service.get(), token_service.get(),
      params->signin_client);

  std::unique_ptr<PrimaryAccountManager> primary_account_manager =
      BuildPrimaryAccountManager(params->signin_client,
                                 account_tracker_service.get(),
                                 token_service.get());

  IdentityManager::InitParameters init_params;

  init_params.primary_account_mutator =
      std::make_unique<PrimaryAccountMutatorImpl>(
          account_tracker_service.get(), primary_account_manager.get(),
          params->pref_service, params->signin_client);

  init_params.accounts_mutator =
      BuildAccountsMutator(params->pref_service, account_tracker_service.get(),
                           token_service.get(), primary_account_manager.get());

  init_params.accounts_cookie_mutator =
      std::make_unique<AccountsCookieMutatorImpl>(
          params->signin_client, token_service.get(),
          gaia_cookie_manager_service.get(), account_tracker_service.get());

  init_params.diagnostics_provider = std::make_unique<DiagnosticsProviderImpl>(
      token_service.get(), gaia_cookie_manager_service.get());

  std::unique_ptr<AccountCapabilitiesFetcherFactory>
      account_capabilities_fetcher_factory;
#if BUILDFLAG(IS_ANDROID)
  account_capabilities_fetcher_factory =
      std::make_unique<AccountCapabilitiesFetcherFactoryAndroid>();
#else
  // Default to server-based lookups if platform-specific capabilities fetcher
  // is not defined.
  if (params->account_capabilities_fetcher_factory) {
    account_capabilities_fetcher_factory =
        std::move(params->account_capabilities_fetcher_factory);
  } else {
    account_capabilities_fetcher_factory =
        std::make_unique<AccountCapabilitiesFetcherFactoryGaia>(
            token_service.get(), params->signin_client);
  }
#endif  // BULIDFLAG(IS_ANDROID)

  init_params.account_fetcher_service = BuildAccountFetcherService(
      params->signin_client, token_service.get(), account_tracker_service.get(),
      std::move(params->image_decoder),
      std::move(account_capabilities_fetcher_factory));

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
  init_params.device_accounts_synchronizer =
      std::make_unique<DeviceAccountsSynchronizerImpl>(
          token_service->GetDelegate());
#endif

  init_params.account_tracker_service = std::move(account_tracker_service);
  init_params.gaia_cookie_manager_service =
      std::move(gaia_cookie_manager_service);
  init_params.primary_account_manager = std::move(primary_account_manager);
  init_params.token_service = std::move(token_service);
  init_params.account_consistency = params->account_consistency;
  init_params.signin_client = params->signin_client;
#if BUILDFLAG(IS_CHROMEOS)
  init_params.account_manager_facade = params->account_manager_facade;
#endif
  init_params.require_sync_consent_for_scope_verification =
      params->require_sync_consent_for_scope_verification;

  return init_params;
}

std::unique_ptr<IdentityManager> BuildIdentityManager(
    IdentityManagerBuildParams* params) {
  return std::make_unique<IdentityManager>(
      BuildIdentityManagerInitParameters(params));
}

}  // namespace signin
