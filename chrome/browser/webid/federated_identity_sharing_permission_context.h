// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_SHARING_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_SHARING_PERMISSION_CONTEXT_H_

#include <string>

#include "chrome/browser/webid/federated_identity_account_keyed_permission_context.h"
#include "content/public/browser/federated_identity_sharing_permission_context_delegate.h"

namespace content {
class BrowserContext;
}

// Context for storing permissions associated with the ability for a relying
// party site to pass an identity request to an identity provider through a
// Javascript API.
class FederatedIdentitySharingPermissionContext
    : public content::FederatedIdentitySharingPermissionContextDelegate,
      public FederatedIdentityAccountKeyedPermissionContext {
 public:
  explicit FederatedIdentitySharingPermissionContext(
      content::BrowserContext* browser_context);

  ~FederatedIdentitySharingPermissionContext() override;

  FederatedIdentitySharingPermissionContext(
      const FederatedIdentitySharingPermissionContext&) = delete;
  FederatedIdentitySharingPermissionContext& operator=(
      const FederatedIdentitySharingPermissionContext&) = delete;

  // content::FederatedIdentitySharingPermissionContextDelegate:
  bool HasSharingPermissionForAnyAccount(
      const url::Origin& relying_party,
      const url::Origin& identity_provider) override;
  bool HasSharingPermission(const url::Origin& relying_party,
                            const url::Origin& identity_provider,
                            const std::string& account_id) override;
  void GrantSharingPermission(const url::Origin& relying_party,
                              const url::Origin& identity_provider,
                              const std::string& account_id) override;
  void RevokeSharingPermission(const url::Origin& relying_party,
                               const url::Origin& identity_provider,
                               const std::string& account_id) override;
};

#endif  // CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_SHARING_PERMISSION_CONTEXT_H_
