// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_PERMISSION_CONTEXT_H_

#include <string>
#include <vector>

#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/permissions/object_permission_context_base.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/webid/federated_identity_data_model.h"
#include "content/public/browser/federated_identity_permission_context_delegate.h"
#include "net/base/schemeful_site.h"

namespace content {
class BrowserContext;
}

class FederatedIdentityAccountKeyedPermissionContext;
class FederatedIdentityIdentityProviderRegistrationContext;
class FederatedIdentityIdentityProviderSigninStatusContext;

// Context for storing permissions associated with the ability for a relying
// party site to pass an identity request to an identity provider through a
// Javascript API.
class FederatedIdentityPermissionContext
    : public content::FederatedIdentityPermissionContextDelegate,
      public signin::IdentityManager::Observer,
      public KeyedService,
      public webid::FederatedIdentityDataModel {
 public:
  explicit FederatedIdentityPermissionContext(
      content::BrowserContext* browser_context);
  ~FederatedIdentityPermissionContext() override;

  FederatedIdentityPermissionContext(
      const FederatedIdentityPermissionContext&) = delete;
  FederatedIdentityPermissionContext& operator=(
      const FederatedIdentityPermissionContext&) = delete;

  // KeyedService:
  void Shutdown() override;

  // content::FederatedIdentityPermissionContextDelegate:
  void AddIdpSigninStatusObserver(IdpSigninStatusObserver* observer) override;
  void RemoveIdpSigninStatusObserver(
      IdpSigninStatusObserver* observer) override;
  bool HasSharingPermission(const url::Origin& relying_party_requester,
                            const url::Origin& relying_party_embedder,
                            const url::Origin& identity_provider) override;
  std::optional<base::Time> GetLastUsedTimestamp(
      const url::Origin& relying_party_requester,
      const url::Origin& relying_party_embedder,
      const url::Origin& identity_provider,
      const std::string& account_id) override;
  bool HasSharingPermission(
      const url::Origin& relying_party_requester) override;
  void GrantSharingPermission(const url::Origin& relying_party_requester,
                              const url::Origin& relying_party_embedder,
                              const url::Origin& identity_provider,
                              const std::string& account_id) override;
  void RevokeSharingPermission(const url::Origin& relying_party_requester,
                               const url::Origin& relying_party_embedder,
                               const url::Origin& identity_provider,
                               const std::string& account_id) override;
  void RefreshExistingSharingPermission(
      const url::Origin& relying_party_requester,
      const url::Origin& relying_party_embedder,
      const url::Origin& identity_provider,
      const std::string& account_id) override;
  std::optional<bool> GetIdpSigninStatus(
      const url::Origin& idp_origin) override;
  void SetIdpSigninStatus(const url::Origin& idp_origin,
                          bool idp_signin_status) override;
  std::vector<GURL> GetRegisteredIdPs() override;
  void RegisterIdP(const GURL& url) override;
  void UnregisterIdP(const GURL& url) override;
  void OnSetRequiresUserMediation(const url::Origin& relying_party,
                                  base::OnceClosure callback) override;

  // signin::IdentityManager::Observer:
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;

  // FederatedIdentityDataModel:
  void GetAllDataKeys(
      base::OnceCallback<void(std::vector<DataKey>)> callback) override;
  void RemoveFederatedIdentityDataByDataKey(
      const DataKey& data_key,
      base::OnceClosure callback) override;

  void FlushScheduledSaveSettingsCalls();

  // Returns whether there is an existing sharing permission for the given
  // (relying party embedder site, identity provider site) pair.
  bool HasSharingPermission(const net::SchemefulSite& relying_party_embedder,
                            const net::SchemefulSite& identity_provider);

  // Marks the given (site, site) pair as eligible to use FedCM sharing
  // permission as a signal for the Storage Access API. This is only valid to
  // call for pairs that already have sharing permission.
  void MarkStorageAccessEligible(
      const net::SchemefulSite& relying_party_embedder,
      const net::SchemefulSite& identity_provider,
      base::OnceClosure callback);

  // Converts existing sharing permission grants into (site, site)-keyed content
  // settings.
  ContentSettingsForOneType GetSharingPermissionGrantsAsContentSettings();

 private:
  std::unique_ptr<FederatedIdentityAccountKeyedPermissionContext>
      sharing_context_;
  std::unique_ptr<FederatedIdentityIdentityProviderSigninStatusContext>
      idp_signin_context_;
  std::unique_ptr<FederatedIdentityIdentityProviderRegistrationContext>
      idp_registration_context_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      obs_{this};

  base::ObserverList<IdpSigninStatusObserver> idp_signin_status_observer_list_;
};

#endif  // CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_PERMISSION_CONTEXT_H_
