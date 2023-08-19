// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_PERMISSION_CONTEXT_H_

#include <string>
#include <vector>

#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/federated_identity_permission_context_delegate.h"

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
      public KeyedService {
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
  bool HasActiveSession(const url::Origin& relying_party_requester,
                        const url::Origin& identity_provider,
                        const std::string& account_identifier) override;
  void GrantActiveSession(const url::Origin& relying_party_requester,
                          const url::Origin& identity_provider,
                          const std::string& account_identifier) override;
  void RevokeActiveSession(const url::Origin& relying_party_requester,
                           const url::Origin& identity_provider,
                           const std::string& account_identifier) override;
  bool HasSharingPermission(
      const url::Origin& relying_party_requester,
      const url::Origin& relying_party_embedder,
      const url::Origin& identity_provider,
      const absl::optional<std::string>& account_id) override;
  bool HasSharingPermission(
      const url::Origin& relying_party_requester) override;
  void GrantSharingPermission(const url::Origin& relying_party_requester,
                              const url::Origin& relying_party_embedder,
                              const url::Origin& identity_provider,
                              const std::string& account_id) override;
  absl::optional<bool> GetIdpSigninStatus(
      const url::Origin& idp_origin) override;
  void SetIdpSigninStatus(const url::Origin& idp_origin,
                          bool idp_signin_status) override;
  std::vector<GURL> GetRegisteredIdPs() override;
  void RegisterIdP(const GURL& url) override;
  void UnregisterIdP(const GURL& url) override;

  // signin::IdentityManager::Observer:
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;

  void FlushScheduledSaveSettingsCalls();

 private:
  std::unique_ptr<FederatedIdentityAccountKeyedPermissionContext>
      active_session_context_;
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
