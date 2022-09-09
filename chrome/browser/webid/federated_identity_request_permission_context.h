// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_REQUEST_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_REQUEST_PERMISSION_CONTEXT_H_

#include <string>

#include "components/permissions/object_permission_context_base.h"
#include "content/public/browser/federated_identity_request_permission_context_delegate.h"

namespace base {
class Value;
}

namespace content {
class BrowserContext;
}

// Context for storing permissions associated with the ability for a relying
// party site to pass an identity request to an identity provider through a
// Javascript API.
class FederatedIdentityRequestPermissionContext
    : public content::FederatedIdentityRequestPermissionContextDelegate,
      public permissions::ObjectPermissionContextBase {
 public:
  explicit FederatedIdentityRequestPermissionContext(
      content::BrowserContext* browser_context);

  ~FederatedIdentityRequestPermissionContext() override;

  FederatedIdentityRequestPermissionContext(
      const FederatedIdentityRequestPermissionContext&) = delete;
  FederatedIdentityRequestPermissionContext& operator=(
      const FederatedIdentityRequestPermissionContext&) = delete;

  // content::FederatedIdentityRequestPermissionContextDelegate:
  bool HasRequestPermission(const url::Origin& relying_party,
                            const url::Origin& identity_provider) override;
  void GrantRequestPermission(const url::Origin& relying_party,
                              const url::Origin& identity_provider) override;
  void RevokeRequestPermission(const url::Origin& relying_party,
                               const url::Origin& identity_provider) override;

 private:
  // permissions:ObjectPermissionContextBase:
  bool IsValidObject(const base::Value& object) override;
  std::u16string GetObjectDisplayName(const base::Value& object) override;
  std::string GetKeyForObject(const base::Value& object) override;
};

#endif  // CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_REQUEST_PERMISSION_CONTEXT_H_
