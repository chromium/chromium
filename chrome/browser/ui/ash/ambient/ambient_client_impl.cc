// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ambient/ambient_client_impl.h"

#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "base/callback.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/channel_info.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/version_info/channel.h"
#include "content/public/browser/device_service.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

constexpr char kPhotosOAuthScope[] = "https://www.googleapis.com/auth/photos";
constexpr char kBackdropOAuthScope[] =
    "https://www.googleapis.com/auth/cast.backdrop";

const user_manager::User* GetActiveUser() {
  return user_manager::UserManager::Get()->GetActiveUser();
}

const user_manager::User* GetPrimaryUser() {
  return user_manager::UserManager::Get()->GetPrimaryUser();
}

Profile* GetProfileForActiveUser() {
  const user_manager::User* const active_user = GetActiveUser();
  DCHECK(active_user);
  return chromeos::ProfileHelper::Get()->GetProfileByUser(active_user);
}

bool IsPrimaryUser() {
  return GetActiveUser() == GetPrimaryUser();
}

bool HasPrimaryAccount(const Profile* profile) {
  auto* identity_manager =
      IdentityManagerFactory::GetForProfileIfExists(profile);
  if (!identity_manager)
    return false;

  return identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin);
}

bool IsEmailDomainSupported(const user_manager::User* user) {
  const std::string email = user->GetAccountId().GetUserEmail();
  DCHECK(!email.empty());

  constexpr char kGmailDomain[] = "gmail.com";
  constexpr char kGooglemailDomain[] = "googlemail.com";
  return (gaia::ExtractDomainName(email) == kGmailDomain ||
          gaia::ExtractDomainName(email) == kGooglemailDomain ||
          gaia::IsGoogleInternalAccountEmail(email));
}

}  // namespace

AmbientClientImpl::AmbientClientImpl() = default;

AmbientClientImpl::~AmbientClientImpl() = default;

bool AmbientClientImpl::IsAmbientModeAllowed() {
  DCHECK(chromeos::features::IsAmbientModeEnabled());

  if (chromeos::DemoSession::IsDeviceInDemoMode())
    return false;

  const user_manager::User* const active_user = GetActiveUser();
  if (!active_user || !active_user->HasGaiaAccount())
    return false;

  if (!IsPrimaryUser())
    return false;

  if (!IsEmailDomainSupported(active_user))
    return false;

  auto* profile = GetProfileForActiveUser();
  if (!profile)
    return false;

  // Primary account might be missing during unittests.
  if (!HasPrimaryAccount(profile))
    return false;

  if (!profile->IsRegularProfile())
    return false;

  return true;
}

void AmbientClientImpl::RequestAccessToken(GetAccessTokenCallback callback) {
  auto* profile = GetProfileForActiveUser();
  DCHECK(profile);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  DCHECK(identity_manager);

  CoreAccountInfo account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  const signin::ScopeSet scopes{kPhotosOAuthScope, kBackdropOAuthScope};
  // TODO(b/148463064): Handle retry refresh token and multiple requests.
  // Currently only one request is allowed.
  DCHECK(!access_token_fetcher_);
  access_token_fetcher_ = identity_manager->CreateAccessTokenFetcherForAccount(
      account_info.account_id, /*oauth_consumer_name=*/"ChromeOS_AmbientMode",
      scopes,
      base::BindOnce(&AmbientClientImpl::GetAccessToken,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     account_info.gaia),
      signin::AccessTokenFetcher::Mode::kImmediate);
}

scoped_refptr<network::SharedURLLoaderFactory>
AmbientClientImpl::GetURLLoaderFactory() {
  auto* profile = GetProfileForActiveUser();
  DCHECK(profile);

  return profile->GetURLLoaderFactory();
}

void AmbientClientImpl::RequestWakeLockProvider(
    mojo::PendingReceiver<device::mojom::WakeLockProvider> receiver) {
  content::GetDeviceService().BindWakeLockProvider(std::move(receiver));
}

bool AmbientClientImpl::ShouldUseProdServer() {
  if (chromeos::features::IsAmbientModeDevUseProdEnabled())
    return true;

  auto channel = chrome::GetChannel();
  return channel == version_info::Channel::STABLE ||
         channel == version_info::Channel::BETA;
}

void AmbientClientImpl::GetAccessToken(
    GetAccessTokenCallback callback,
    const std::string& gaia_id,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  // It's safe to delete AccessTokenFetcher from inside its own callback.
  access_token_fetcher_.reset();

  if (error.state() == GoogleServiceAuthError::NONE) {
    std::move(callback).Run(gaia_id, access_token_info.token,
                            access_token_info.expiration_time);
  } else {
    LOG(ERROR) << "Failed to retrieve token, error: " << error.ToString();
    std::move(callback).Run(/*gaia_id=*/std::string(),
                            /*access_token=*/std::string(),
                            /*expiration_time=*/base::Time::Now());
  }
}

