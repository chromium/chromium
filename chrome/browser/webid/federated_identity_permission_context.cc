// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_permission_context.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/webid/federated_identity_account_keyed_permission_context.h"
#include "chrome/browser/webid/federated_identity_identity_provider_registration_context.h"
#include "chrome/browser/webid/federated_identity_identity_provider_signin_status_context.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "content/public/browser/browser_context.h"
#include "google_apis/gaia/gaia_urls.h"
#include "url/origin.h"

FederatedIdentityPermissionContext::FederatedIdentityPermissionContext(
    content::BrowserContext* browser_context)
    : sharing_context_(
          new FederatedIdentityAccountKeyedPermissionContext(browser_context)),
      idp_signin_context_(
          new FederatedIdentityIdentityProviderSigninStatusContext(
              browser_context)),
      idp_registration_context_(
          new FederatedIdentityIdentityProviderRegistrationContext(
              browser_context)) {
  if (!browser_context->IsOffTheRecord()) {
    Profile* profile = Profile::FromBrowserContext(browser_context);
    signin::IdentityManager* mgr =
        IdentityManagerFactory::GetForProfile(profile);
    if (mgr) {
      obs_.Observe(mgr);
    }
  }
}

FederatedIdentityPermissionContext::~FederatedIdentityPermissionContext() =
    default;

void FederatedIdentityPermissionContext::Shutdown() {
  obs_.Reset();
  FlushScheduledSaveSettingsCalls();
  KeyedService::Shutdown();
}

void FederatedIdentityPermissionContext::AddIdpSigninStatusObserver(
    IdpSigninStatusObserver* observer) {
  if (idp_signin_status_observer_list_.HasObserver(observer)) {
    return;
  }

  idp_signin_status_observer_list_.AddObserver(observer);
}

void FederatedIdentityPermissionContext::RemoveIdpSigninStatusObserver(
    IdpSigninStatusObserver* observer) {
  idp_signin_status_observer_list_.RemoveObserver(observer);
}

bool FederatedIdentityPermissionContext::HasSharingPermission(
    const url::Origin& relying_party_requester,
    const url::Origin& relying_party_embedder,
    const url::Origin& identity_provider) {
  return sharing_context_->HasPermission(
      relying_party_requester, relying_party_embedder, identity_provider);
}

std::optional<base::Time>
FederatedIdentityPermissionContext::GetLastUsedTimestamp(
    const url::Origin& relying_party_requester,
    const url::Origin& relying_party_embedder,
    const url::Origin& identity_provider,
    const std::string& account_id) {
  return sharing_context_->GetLastUsedTimestamp(relying_party_requester,
                                                relying_party_embedder,
                                                identity_provider, account_id);
}

bool FederatedIdentityPermissionContext::HasSharingPermission(
    const url::Origin& relying_party_requester) {
  return sharing_context_->HasPermission(relying_party_requester);
}

bool FederatedIdentityPermissionContext::HasSharingPermission(
    const net::SchemefulSite& relying_party_embedder,
    const net::SchemefulSite& identity_provider) {
  return sharing_context_->HasPermission(relying_party_embedder,
                                         identity_provider);
}

void FederatedIdentityPermissionContext::MarkStorageAccessEligible(
    const net::SchemefulSite& relying_party_embedder,
    const net::SchemefulSite& identity_provider,
    base::OnceClosure callback) {
  sharing_context_->MarkStorageAccessEligible(
      relying_party_embedder, identity_provider, std::move(callback));
}

void FederatedIdentityPermissionContext::OnSetRequiresUserMediation(
    const url::Origin& relying_party,
    base::OnceClosure callback) {
  sharing_context_->OnSetRequiresUserMediation(relying_party,
                                               std::move(callback));
}

void FederatedIdentityPermissionContext::GrantSharingPermission(
    const url::Origin& relying_party_requester,
    const url::Origin& relying_party_embedder,
    const url::Origin& identity_provider,
    const std::string& account_id) {
  sharing_context_->GrantPermission(relying_party_requester,
                                    relying_party_embedder, identity_provider,
                                    account_id);
}

void FederatedIdentityPermissionContext::RevokeSharingPermission(
    const url::Origin& relying_party_requester,
    const url::Origin& relying_party_embedder,
    const url::Origin& identity_provider,
    const std::string& account_id) {
  sharing_context_->RevokePermission(relying_party_requester,
                                     relying_party_embedder, identity_provider,
                                     account_id, base::DoNothing());
}

void FederatedIdentityPermissionContext::RefreshExistingSharingPermission(
    const url::Origin& relying_party_requester,
    const url::Origin& relying_party_embedder,
    const url::Origin& identity_provider,
    const std::string& account_id) {
  sharing_context_->RefreshExistingPermission(relying_party_requester,
                                              relying_party_embedder,
                                              identity_provider, account_id);
}

ContentSettingsForOneType FederatedIdentityPermissionContext::
    GetSharingPermissionGrantsAsContentSettings() {
  return sharing_context_->GetSharingPermissionGrantsAsContentSettings();
}

void FederatedIdentityPermissionContext::GetAllDataKeys(
    base::OnceCallback<void(std::vector<DataKey>)> callback) {
  sharing_context_->GetAllDataKeys(std::move(callback));
}

void FederatedIdentityPermissionContext::RemoveFederatedIdentityDataByDataKey(
    const DataKey& data_key,
    base::OnceClosure callback) {
  sharing_context_->RemoveFederatedIdentityDataByDataKey(data_key,
                                                         std::move(callback));
}

std::optional<bool> FederatedIdentityPermissionContext::GetIdpSigninStatus(
    const url::Origin& idp_origin) {
  return idp_signin_context_->GetSigninStatus(idp_origin);
}

void FederatedIdentityPermissionContext::SetIdpSigninStatus(
    const url::Origin& idp_origin,
    bool idp_signin_status) {
  std::optional<bool> old_idp_signin_status = GetIdpSigninStatus(idp_origin);
  // We always notify if idp_signin_status is true because the list of logged
  // in accounts may have changed.
  if (!idp_signin_status && (idp_signin_status == old_idp_signin_status)) {
    return;
  }

  idp_signin_context_->SetSigninStatus(idp_origin, idp_signin_status);
  for (IdpSigninStatusObserver& observer : idp_signin_status_observer_list_) {
    observer.OnIdpSigninStatusReceived(idp_origin, idp_signin_status);
  }
}

std::vector<GURL> FederatedIdentityPermissionContext::GetRegisteredIdPs() {
  return idp_registration_context_->GetRegisteredIdPs();
}

void FederatedIdentityPermissionContext::RegisterIdP(const GURL& origin) {
  idp_registration_context_->RegisterIdP(origin);
}

void FederatedIdentityPermissionContext::UnregisterIdP(const GURL& origin) {
  idp_registration_context_->UnregisterIdP(origin);
}

void FederatedIdentityPermissionContext::FlushScheduledSaveSettingsCalls() {
  sharing_context_->FlushScheduledSaveSettingsCalls();
  idp_signin_context_->FlushScheduledSaveSettingsCalls();
  idp_registration_context_->FlushScheduledSaveSettingsCalls();
}

void FederatedIdentityPermissionContext::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  bool logged_in =
      !accounts_in_cookie_jar_info.GetPotentiallyInvalidSignedInAccounts()
           .empty();
  GURL gaia_url = GaiaUrls::GetInstance()->gaia_url();
  url::Origin origin = url::Origin::Create(gaia_url);
  SetIdpSigninStatus(origin, logged_in);
}
