// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"

#include "components/signin/internal/identity_manager/profile_oauth2_token_service_observer.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

ProfileOAuth2TokenServiceDelegate::ScopedBatchChange::ScopedBatchChange(
    ProfileOAuth2TokenServiceDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
  delegate_->StartBatchChanges();
}

ProfileOAuth2TokenServiceDelegate::ScopedBatchChange::~ScopedBatchChange() {
  delegate_->EndBatchChanges();
}

ProfileOAuth2TokenServiceDelegate::ProfileOAuth2TokenServiceDelegate()
    : batch_change_depth_(0) {}

ProfileOAuth2TokenServiceDelegate::~ProfileOAuth2TokenServiceDelegate() =
    default;

bool ProfileOAuth2TokenServiceDelegate::ValidateAccountId(
    const CoreAccountId& account_id) const {
  bool valid = !account_id.empty();

  // If the account is given as an email, make sure its a canonical email.
  // Note that some tests don't use email strings as account id, and after
  // the gaia id migration it won't be an email.  So only check for
  // canonicalization if the account_id is suspected to be an email.
  if (account_id.ToString().find('@') != std::string::npos &&
      gaia::CanonicalizeEmail(account_id.ToString()) != account_id.ToString()) {
    valid = false;
  }

  DCHECK(valid);
  return valid;
}

void ProfileOAuth2TokenServiceDelegate::AddObserver(
    ProfileOAuth2TokenServiceObserver* observer) {
  observer_list_.AddObserver(observer);
}

void ProfileOAuth2TokenServiceDelegate::RemoveObserver(
    ProfileOAuth2TokenServiceObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void ProfileOAuth2TokenServiceDelegate::StartBatchChanges() {
  ++batch_change_depth_;
}

void ProfileOAuth2TokenServiceDelegate::EndBatchChanges() {
  --batch_change_depth_;
  DCHECK_LE(0, batch_change_depth_);
  if (batch_change_depth_ == 0) {
    FireEndBatchChanges();
  }
}

void ProfileOAuth2TokenServiceDelegate::FireEndBatchChanges() {
  for (auto& observer : observer_list_)
    observer.OnEndBatchChanges();
}

void ProfileOAuth2TokenServiceDelegate::FireRefreshTokenAvailable(
    const CoreAccountId& account_id) {
  DCHECK(!account_id.empty());
  for (auto& observer : observer_list_)
    observer.OnRefreshTokenAvailable(account_id);
}

void ProfileOAuth2TokenServiceDelegate::FireRefreshTokenRevoked(
    const CoreAccountId& account_id) {
  DCHECK(!account_id.empty());
  for (auto& observer : observer_list_)
    observer.OnRefreshTokenRevoked(account_id);
}

void ProfileOAuth2TokenServiceDelegate::FireRefreshTokensLoaded() {
  for (auto& observer : observer_list_)
    observer.OnRefreshTokensLoaded();
}

void ProfileOAuth2TokenServiceDelegate::FireAuthErrorChanged(
    const CoreAccountId& account_id,
    const GoogleServiceAuthError& error) {
  DCHECK(!account_id.empty());
  for (auto& observer : observer_list_)
    observer.OnAuthErrorChanged(account_id, error);
}

std::string ProfileOAuth2TokenServiceDelegate::GetTokenForMultilogin(
    const CoreAccountId& account_id) const {
  return std::string();
}

scoped_refptr<network::SharedURLLoaderFactory>
ProfileOAuth2TokenServiceDelegate::GetURLLoaderFactory() const {
  return nullptr;
}

GoogleServiceAuthError ProfileOAuth2TokenServiceDelegate::GetAuthError(
    const CoreAccountId& account_id) const {
  return GoogleServiceAuthError::AuthErrorNone();
}

std::vector<CoreAccountId> ProfileOAuth2TokenServiceDelegate::GetAccounts()
    const {
  return std::vector<CoreAccountId>();
}

const net::BackoffEntry* ProfileOAuth2TokenServiceDelegate::BackoffEntry()
    const {
  return nullptr;
}

void ProfileOAuth2TokenServiceDelegate::LoadCredentials(
    const CoreAccountId& primary_account_id) {
  NOTREACHED()
      << "ProfileOAuth2TokenServiceDelegate does not load credentials. "
         "Subclasses that need to load credentials must provide "
         "an implemenation of this method";
}

void ProfileOAuth2TokenServiceDelegate::ExtractCredentials(
    ProfileOAuth2TokenService* to_service,
    const CoreAccountId& account_id) {
  NOTREACHED();
}

bool ProfileOAuth2TokenServiceDelegate::FixRequestErrorIfPossible() {
  return false;
}
