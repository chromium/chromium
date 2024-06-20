// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_ACCOUNT_KEYED_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_ACCOUNT_KEYED_PERMISSION_CONTEXT_H_

#include <string>

#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/object_permission_context_base.h"
#include "components/webid/federated_identity_data_model.h"

namespace content {
class BrowserContext;
class ContentSettingsForOneType;
}

namespace net {
class SchemefulSite;
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

  ~FederatedIdentityAccountKeyedPermissionContext() override;

  // Returns whether the given relying party has any FedCM permission.
  bool HasPermission(const url::Origin& relying_party_requester);

  // Returns whether there is an existing permission for the given (relying
  // party embedder site, identity provider site) tuple. This may be called on
  // any thread.
  //
  // Note that this query ignores the relying_party_requester portion of the
  // key. This means that if the sharing permission was granted to an embedded
  // relying party (a cross-origin iframe), `HasPermission(site, site)` may
  // return true even for non-cross-origin iframe cases.
  bool HasPermission(const net::SchemefulSite& relying_party_embedder,
                     const net::SchemefulSite& identity_provider);

  // Returns whether there is an existing permission for the
  // (relying_party_requester, relying_party_embedder, identity_provider) tuple.
  bool HasPermission(const url::Origin& relying_party_requester,
                     const url::Origin& relying_party_embedder,
                     const url::Origin& identity_provider);

  // Returns the last time when `account_id` was used via FedCM on the
  // (relying_party_requester, relying_party_embedder, identity_provider). If
  // there is no known last time, returns nullopt. If the `account_id` was known
  // to be used but a timestamp is not known, returns 0.
  std::optional<base::Time> GetLastUsedTimestamp(
      const url::Origin& relying_party_requester,
      const url::Origin& relying_party_embedder,
      const url::Origin& identity_provider,
      const std::string& account_id);

  // Grants permission for the (relying_party_requester, relying_party_embedder,
  // identity_provider, account_id) tuple.
  void GrantPermission(const url::Origin& relying_party_requester,
                       const url::Origin& relying_party_embedder,
                       const url::Origin& identity_provider,
                       const std::string& account_id);

  // Updates the timestamp of an existing permission, or does nothing if no
  // permission matching the key is found. Returns true if a permission is
  // found, false otherwise.
  bool RefreshExistingPermission(const url::Origin& relying_party_requester,
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

  // Marks the given (site, site) pair as eligible to use FedCM sharing
  // permission as a signal for the Storage Access API. This is only valid to
  // call for pairs that already have sharing permission.
  void MarkStorageAccessEligible(
      const net::SchemefulSite& relying_party_embedder,
      const net::SchemefulSite& identity_provider,
      base::OnceClosure callback);

  // Handles updates when an origin's "requires user mediation" status changes.
  void OnSetRequiresUserMediation(const url::Origin& relying_party,
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
  // settings. The returned value only includes permissions for (site, site)
  // pairs that were marked "eligible" via `MarkStorageAccessEligible`.
  ContentSettingsForOneType GetSharingPermissionGrantsAsContentSettings();

 private:
  // permissions::ObjectPermissionContextBase:
  bool IsValidObject(const base::Value::Dict& object) override;
  std::u16string GetObjectDisplayName(const base::Value::Dict& object) override;

  // Sends the current FEDERATED_IDENTITY_SHARING permissions to the network
  // service, for use when choosing which cookies to include or exclude for a
  // given network request or `document.cookie` evaluation.
  void SyncSharingPermissionGrantsToNetworkService(base::OnceClosure callback);

  void AddToAccountList(base::Value::Dict& dict, const std::string& account_id);

  // The BrowserContext associated with this permission context.
  base::raw_ref<content::BrowserContext> browser_context_;

  raw_ptr<base::Clock> clock_;

  // The set of pairs that are eligible for Storage-Access autogrants.
  // Keyed by (embedder, identity-provider) pairs.
  base::flat_set<std::pair<net::SchemefulSite, net::SchemefulSite>>
      storage_access_eligible_connections_;
};

#endif  // CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_ACCOUNT_KEYED_PERMISSION_CONTEXT_H_
