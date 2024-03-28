// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_ACCOUNT_KEYED_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_ACCOUNT_KEYED_PERMISSION_CONTEXT_H_

#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/object_permission_context_base.h"
#include "components/webid/federated_identity_data_model.h"

#include <string>

namespace content {
class BrowserContext;
class ContentSettingsForOneType;
}

namespace url {
class Origin;
}

class HostContentSettingsMap;

// Context for storing permission grants that are associated with a
// (relying party, identity-provider, identity-provider account) tuple.
class FederatedIdentityAccountKeyedPermissionContext
    : public permissions::ObjectPermissionContextBase {
 public:
  explicit FederatedIdentityAccountKeyedPermissionContext(
      content::BrowserContext* browser_context);
  FederatedIdentityAccountKeyedPermissionContext(
      content::BrowserContext* browser_context,
      HostContentSettingsMap* host_content_settings_map);

  FederatedIdentityAccountKeyedPermissionContext(
      const FederatedIdentityAccountKeyedPermissionContext&) = delete;
  FederatedIdentityAccountKeyedPermissionContext& operator=(
      const FederatedIdentityAccountKeyedPermissionContext&) = delete;

  // Returns whether the given relying party has any FedCM permission.
  bool HasPermission(const url::Origin& relying_party_requester);

  // Returns whether there is an existing permission for the given (relying
  // party requester site, identity provider site) tuple. This may be called on
  // any thread.
  bool HasPermission(const net::SchemefulSite& relying_party_requester,
                     const net::SchemefulSite& identity_provider);

  // Returns whether there is an existing permission for the
  // (relying_party_requester, relying_party_embedder, identity_provider,
  // account_id) tuple. `account_id` can be omitted to represent "sharing
  // permission for any account".
  bool HasPermission(const url::Origin& relying_party_requester,
                     const url::Origin& relying_party_embedder,
                     const url::Origin& identity_provider,
                     const std::optional<std::string>& account_id);

  // Grants permission for the (relying_party_requester, relying_party_embedder,
  // identity_provider, account_id) tuple.
  void GrantPermission(const url::Origin& relying_party_requester,
                       const url::Origin& relying_party_embedder,
                       const url::Origin& identity_provider,
                       const std::string& account_id);

  // Revokes previously-granted permission for the (`relying_party_requester`,
  // `relying_party_embedder`, `identity_provider`, `account_id`) tuple. If the
  // `account_id` is not found, we revoke all accounts associated with the
  // triple (`relying_party_requester`, `relying_party_embedder`,
  // `identity_provider`).
  void RevokePermission(const url::Origin& relying_party_requester,
                        const url::Origin& relying_party_embedder,
                        const url::Origin& identity_provider,
                        const std::string& account_id,
                        base::OnceClosure callback);

  // permissions::ObjectPermissionContextBase:
  std::string GetKeyForObject(const base::Value::Dict& object) override;

  void GetAllDataKeys(
      base::OnceCallback<void(
          std::vector<webid::FederatedIdentityDataModel::DataKey>)> callback);
  void RemoveFederatedIdentityDataByDataKey(
      const webid::FederatedIdentityDataModel::DataKey& data_key,
      base::OnceClosure callback);

  // Converts existing sharing permission grants into (site, site)-keyed content
  // settings.
  ContentSettingsForOneType GetSharingPermissionGrantsAsContentSettings();

 private:
  // permissions::ObjectPermissionContextBase:
  bool IsValidObject(const base::Value::Dict& object) override;
  std::u16string GetObjectDisplayName(const base::Value::Dict& object) override;

  // Sends the current FEDERATED_IDENTITY_SHARING permissions to the network
  // service, for use when choosing which cookies to include or exclude for a
  // given network request or `document.cookie` evaluation.
  void SyncSharingPermissionGrantsToNetworkService(base::OnceClosure callback);

  // The BrowserContext associated with this permission context.
  base::raw_ref<content::BrowserContext> browser_context_;
};

#endif  // CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_ACCOUNT_KEYED_PERMISSION_CONTEXT_H_
