// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_ACTIVE_SESSION_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_ACTIVE_SESSION_PERMISSION_CONTEXT_H_

#include <string>

#include "components/permissions/object_permission_context_base.h"
#include "content/public/browser/federated_identity_active_session_permission_context_delegate.h"

namespace base {
class Value;
}

namespace content {
class BrowserContext;
}

// Context for storing permission grants that are associated with having an
// federated sign-in session between a Relying Party and and Identity
// Provider.
class FederatedIdentityActiveSessionPermissionContext
    : public content::FederatedIdentityActiveSessionPermissionContextDelegate,
      public permissions::ObjectPermissionContextBase {
 public:
  explicit FederatedIdentityActiveSessionPermissionContext(
      content::BrowserContext* browser_context);

  ~FederatedIdentityActiveSessionPermissionContext() override;

  FederatedIdentityActiveSessionPermissionContext(
      const FederatedIdentityActiveSessionPermissionContext&) = delete;
  FederatedIdentityActiveSessionPermissionContext& operator=(
      const FederatedIdentityActiveSessionPermissionContext&) = delete;

  // content::FederatedIdentityActiveSessionPermissionContextDelegate:
  bool HasActiveSession(const url::Origin& relying_party,
                        const url::Origin& identity_provider,
                        const std::string& account_identifier) override;
  void GrantActiveSession(const url::Origin& relying_party,
                          const url::Origin& identity_provider,
                          const std::string& account_identifier) override;
  void RevokeActiveSession(const url::Origin& relying_party,
                           const url::Origin& identity_provider,
                           const std::string& account_identifier) override;

 private:
  // permissions:ObjectPermissionContextBase:
  bool IsValidObject(const base::Value& object) override;
  std::u16string GetObjectDisplayName(const base::Value& object) override;
  std::string GetKeyForObject(const base::Value& object) override;
};

#endif  // CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_ACTIVE_SESSION_PERMISSION_CONTEXT_H_
