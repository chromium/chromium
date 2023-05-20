// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_ACCOUNT_KEYED_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_ACCOUNT_KEYED_PERMISSION_CONTEXT_H_

#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/object_permission_context_base.h"

#include <string>

namespace content {
class BrowserContext;
}

namespace url {
class Origin;
}

// Context for storing permission grants that are associated with a
// (relying party, identity-provider, identity-provider account) tuple.
class FederatedIdentityAccountKeyedPermissionContext
    : public permissions::ObjectPermissionContextBase {
 public:
  FederatedIdentityAccountKeyedPermissionContext(
      content::BrowserContext* browser_context,
      ContentSettingsType content_settings_type,
      const std::string& idp_origin_key);

  FederatedIdentityAccountKeyedPermissionContext(
      const FederatedIdentityAccountKeyedPermissionContext&) = delete;
  FederatedIdentityAccountKeyedPermissionContext& operator=(
      const FederatedIdentityAccountKeyedPermissionContext&) = delete;

  // Returns whether the given relying party has any FedCM permission.
  bool HasPermission(const url::Origin& relying_party_requester);

  // Returns whether there is an existing permission for the
  // (relying_party_requester, relying_party_embedder, identity_provider,
  // account_id) tuple. `account_id` can be omitted to represent "sharing
  // permission for any account".
  bool HasPermission(const url::Origin& relying_party_requester,
                     const url::Origin& relying_party_embedder,
                     const url::Origin& identity_provider,
                     const absl::optional<std::string>& account_id);

  // Grants permission for the (relying_party_requester, relying_party_embedder,
  // identity_provider, account_id) tuple.
  void GrantPermission(const url::Origin& relying_party_requester,
                       const url::Origin& relying_party_embedder,
                       const url::Origin& identity_provider,
                       const std::string& account_id);

  // Revokes previously-granted permission for the (relying_party_requester,
  // relying_party_embedder, identity_provider, account_id) tuple.
  void RevokePermission(const url::Origin& relying_party_requester,
                        const url::Origin& relying_party_embedder,
                        const url::Origin& identity_provider,
                        const std::string& account_id);

  // permissions::ObjectPermissionContextBase:
  std::string GetKeyForObject(const base::Value::Dict& object) override;

 private:
  // permissions::ObjectPermissionContextBase:
  bool IsValidObject(const base::Value::Dict& object) override;
  std::u16string GetObjectDisplayName(const base::Value::Dict& object) override;

  const std::string idp_origin_key_;
};

#endif  // CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_ACCOUNT_KEYED_PERMISSION_CONTEXT_H_
