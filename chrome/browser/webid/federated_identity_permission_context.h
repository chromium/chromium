// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_PERMISSION_CONTEXT_H_

#include <string>

#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/federated_identity_permission_context_delegate.h"

namespace content {
class BrowserContext;
}

class FederatedIdentityAccountKeyedPermissionContext;
class FederatedIdentityIdentityProviderSigninStatusContext;

// Context for storing permissions associated with the ability for a relying
// party site to pass an identity request to an identity provider through a
// Javascript API.
class FederatedIdentityPermissionContext
    : public content::FederatedIdentityPermissionContextDelegate,
      public KeyedService {
 public:
  explicit FederatedIdentityPermissionContext(
      content::BrowserContext* browser_context);
  ~FederatedIdentityPermissionContext() override;

  FederatedIdentityPermissionContext(
      const FederatedIdentityPermissionContext&) = delete;
  FederatedIdentityPermissionContext& operator=(
      const FederatedIdentityPermissionContext&) = delete;

  // content::FederatedIdentityPermissionContextDelegate:
  bool HasActiveSession(const url::Origin& relying_party_requester,
                        const url::Origin& identity_provider,
                        const std::string& account_identifier) override;
  void GrantActiveSession(const url::Origin& relying_party_requester,
                          const url::Origin& identity_provider,
                          const std::string& account_identifier) override;
  void RevokeActiveSession(const url::Origin& relying_party_requester,
                           const url::Origin& identity_provider,
                           const std::string& account_identifier) override;
  bool HasSharingPermission(const url::Origin& relying_party_requester,
                            const url::Origin& relying_party_embedder,
                            const url::Origin& identity_provider,
                            const std::string& account_id) override;
  void GrantSharingPermission(const url::Origin& relying_party_requester,
                              const url::Origin& relying_party_embedder,
                              const url::Origin& identity_provider,
                              const std::string& account_id) override;
  absl::optional<bool> GetIdpSigninStatus(
      const url::Origin& idp_origin) override;
  void SetIdpSigninStatus(const url::Origin& idp_origin,
                          bool idp_signin_status) override;

  void FlushScheduledSaveSettingsCalls();

 private:
  std::unique_ptr<FederatedIdentityAccountKeyedPermissionContext>
      active_session_context_;
  std::unique_ptr<FederatedIdentityAccountKeyedPermissionContext>
      sharing_context_;
  std::unique_ptr<FederatedIdentityIdentityProviderSigninStatusContext>
      idp_signin_context_;
};

#endif  // CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_PERMISSION_CONTEXT_H_
