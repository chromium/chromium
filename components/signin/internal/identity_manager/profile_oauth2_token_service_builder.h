// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PROFILE_OAUTH2_TOKEN_SERVICE_BUILDER_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PROFILE_OAUTH2_TOKEN_SERVICE_BUILDER_H_

#include <memory>

#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "components/signin/public/base/signin_buildflags.h"

#if !BUILDFLAG(IS_ANDROID)
#include "base/memory/scoped_refptr.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "components/signin/internal/identity_manager/mutable_profile_oauth2_token_service_delegate.h"
#endif

class AccountTrackerService;
class PrefService;
class ProfileOAuth2TokenService;
class SigninClient;

#if BUILDFLAG(IS_IOS)
class DeviceAccountsProvider;
#endif

namespace signin {
enum class AccountConsistencyMethod;
}

namespace network {
class NetworkConnectionTracker;
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
class TokenWebData;
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
namespace unexportable_keys {
class UnexportableKeyService;
}
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#endif

#if BUILDFLAG(IS_CHROMEOS)
namespace account_manager {
class AccountManagerFacade;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

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
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
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
    SigninClient* signin_client);
#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PROFILE_OAUTH2_TOKEN_SERVICE_BUILDER_H_
