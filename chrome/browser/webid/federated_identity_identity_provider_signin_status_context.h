// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_IDENTITY_PROVIDER_SIGNIN_STATUS_CONTEXT_H_
#define CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_IDENTITY_PROVIDER_SIGNIN_STATUS_CONTEXT_H_

#include "components/permissions/object_permission_context_base.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

#include <string>

namespace content {
class BrowserContext;
}

namespace url {
class Origin;
}

// Context for storing whether the browser has observed the user signing into
// an identity provider by observing the IdP-SignIn-Status HTTP header.
class FederatedIdentityIdentityProviderSigninStatusContext
    : public permissions::ObjectPermissionContextBase {
 public:
  explicit FederatedIdentityIdentityProviderSigninStatusContext(
      content::BrowserContext* browser_context);

  FederatedIdentityIdentityProviderSigninStatusContext(
      const FederatedIdentityIdentityProviderSigninStatusContext&) = delete;
  FederatedIdentityIdentityProviderSigninStatusContext& operator=(
      const FederatedIdentityIdentityProviderSigninStatusContext&) = delete;

  // Returns the sign-in status for the passed-in `identity_provider`. Returns
  // absl::nullopt if there isn't a stored sign-in status.
  absl::optional<bool> GetSigninStatus(const url::Origin& identity_provider);

  // Stores the sign-in status for the passed-in `identity_provider`.
  void SetSigninStatus(const url::Origin& identity_provider,
                       bool signin_status);

  // permissions::ObjectPermissionContextBase:
  std::string GetKeyForObject(const base::Value& object) override;

 private:
  // permissions::ObjectPermissionContextBase:
  bool IsValidObject(const base::Value& object) override;
  std::u16string GetObjectDisplayName(const base::Value& object) override;
};

#endif  // CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_IDENTITY_PROVIDER_SIGNIN_STATUS_CONTEXT_H_
