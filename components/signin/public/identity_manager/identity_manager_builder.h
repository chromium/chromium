// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_MANAGER_BUILDER_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_MANAGER_BUILDER_H_

#include <memory>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/signin/public/identity_manager/identity_manager.h"

#if !defined(OS_ANDROID)
#include "base/memory/scoped_refptr.h"
#endif

#if defined(OS_WIN)
#include "base/callback.h"
#endif

class AccountTrackerService;
class PrefService;
class ProfileOAuth2TokenService;
class SigninClient;

#if !defined(OS_ANDROID)
class TokenWebData;
#endif

#if defined(OS_IOS)
class DeviceAccountsProvider;
#endif

namespace image_fetcher {
class ImageDecoder;
}

namespace network {
class NetworkConnectionTracker;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace ash {
class AccountManager;
}

namespace account_manager {
class AccountManagerFacade;
}
#endif

namespace signin {
enum class AccountConsistencyMethod;

struct IdentityManagerBuildParams {
  IdentityManagerBuildParams();
  ~IdentityManagerBuildParams();

  AccountConsistencyMethod account_consistency;
  std::unique_ptr<AccountTrackerService> account_tracker_service;
  std::unique_ptr<image_fetcher::ImageDecoder> image_decoder;
  PrefService* local_state;
  network::NetworkConnectionTracker* network_connection_tracker;
  PrefService* pref_service;
  base::FilePath profile_path;
  SigninClient* signin_client;
  std::unique_ptr<ProfileOAuth2TokenService> token_service;

#if !defined(OS_ANDROID)
  bool delete_signin_cookies_on_exit;
  scoped_refptr<TokenWebData> token_web_data;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::AccountManager* account_manager;
  account_manager::AccountManagerFacade* account_manager_facade;
  bool is_regular_profile;
#endif

#if defined(OS_IOS)
  std::unique_ptr<DeviceAccountsProvider> device_accounts_provider;
#endif

#if defined(OS_WIN)
  base::RepeatingCallback<bool()> reauth_callback;
#endif

  bool allow_access_token_fetch = true;
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
