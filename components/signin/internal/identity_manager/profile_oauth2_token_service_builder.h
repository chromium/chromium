// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PROFILE_OAUTH2_TOKEN_SERVICE_BUILDER_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PROFILE_OAUTH2_TOKEN_SERVICE_BUILDER_H_

#include <memory>

#include "build/build_config.h"

#if !defined(OS_ANDROID)
#include "base/memory/scoped_refptr.h"
#endif

#if defined(OS_WIN)
#include "components/signin/internal/identity_manager/mutable_profile_oauth2_token_service_delegate.h"
#endif

class AccountTrackerService;
class PrefService;
class ProfileOAuth2TokenService;
class SigninClient;

#if defined(OS_IOS)
class DeviceAccountsProvider;
#endif

namespace signin {
enum class AccountConsistencyMethod;
}

namespace network {
class NetworkConnectionTracker;
}

#if !defined(OS_ANDROID)
class TokenWebData;
#endif

#if defined(OS_CHROMEOS)
namespace chromeos {
class AccountManager;
}
#endif

std::unique_ptr<ProfileOAuth2TokenService> BuildProfileOAuth2TokenService(
    PrefService* pref_service,
    AccountTrackerService* account_tracker_service,
    network::NetworkConnectionTracker* network_connection_tracker,
    signin::AccountConsistencyMethod account_consistency,
#if defined(OS_CHROMEOS)
    chromeos::AccountManager* account_manager,
    bool is_regular_profile,
#endif
#if !defined(OS_ANDROID)
    bool delete_signin_cookies_on_exit,
    scoped_refptr<TokenWebData> token_web_data,
#endif
#if defined(OS_IOS)
    std::unique_ptr<DeviceAccountsProvider> device_accounts_provider,
#endif
#if defined(OS_WIN)
    MutableProfileOAuth2TokenServiceDelegate::FixRequestErrorCallback
        reauth_callback,
#endif
    SigninClient* signin_client);
#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PROFILE_OAUTH2_TOKEN_SERVICE_BUILDER_H_
