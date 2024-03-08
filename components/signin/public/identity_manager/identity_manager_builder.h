// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_MANAGER_BUILDER_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_MANAGER_BUILDER_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/identity_manager.h"

#if !BUILDFLAG(IS_ANDROID)
#include "base/memory/scoped_refptr.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/functional/callback.h"
#endif

class AccountCapabilitiesFetcherFactory;
class PrefService;
class SigninClient;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
class TokenWebData;
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
namespace unexportable_keys {
class UnexportableKeyService;
}
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#endif

#if BUILDFLAG(IS_IOS)
class DeviceAccountsProvider;
#endif

namespace image_fetcher {
class ImageDecoder;
}

namespace network {
class NetworkConnectionTracker;
}

#if BUILDFLAG(IS_CHROMEOS)
namespace account_manager {
class AccountManagerFacade;
}
#endif

namespace signin {
enum class AccountConsistencyMethod;

struct IdentityManagerBuildParams {
  IdentityManagerBuildParams();
  ~IdentityManagerBuildParams();

  AccountConsistencyMethod account_consistency =
      AccountConsistencyMethod::kDisabled;
  std::unique_ptr<image_fetcher::ImageDecoder> image_decoder;
  raw_ptr<PrefService> local_state = nullptr;
  raw_ptr<network::NetworkConnectionTracker> network_connection_tracker;
  raw_ptr<PrefService> pref_service = nullptr;
  base::FilePath profile_path;
  raw_ptr<SigninClient> signin_client = nullptr;
  std::unique_ptr<AccountCapabilitiesFetcherFactory>
      account_capabilities_fetcher_factory;
  std::unique_ptr<ProfileOAuth2TokenService> token_service;
  std::unique_ptr<AccountTrackerService> account_tracker_service;

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
  bool delete_signin_cookies_on_exit = false;
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  scoped_refptr<TokenWebData> token_web_data;
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  raw_ptr<unexportable_keys::UnexportableKeyService> unexportable_key_service =
      nullptr;
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#endif

#if BUILDFLAG(IS_CHROMEOS)
  raw_ptr<account_manager::AccountManagerFacade, DanglingUntriaged>
      account_manager_facade = nullptr;
  bool is_regular_profile = false;
#endif

#if BUILDFLAG(IS_IOS)
  std::unique_ptr<DeviceAccountsProvider> device_accounts_provider;
#endif

  bool require_sync_consent_for_scope_verification = true;

#if BUILDFLAG(IS_WIN)
  base::RepeatingCallback<bool()> reauth_callback;
#endif
};

// Builds all required dependencies to initialize the IdentityManager instance.
IdentityManager::InitParameters BuildIdentityManagerInitParameters(
    IdentityManagerBuildParams* params);

// Builds an IdentityManager instance from the supplied embedder-level
// dependencies.
std::unique_ptr<IdentityManager> BuildIdentityManager(
    IdentityManagerBuildParams* params);

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_MANAGER_BUILDER_H_
