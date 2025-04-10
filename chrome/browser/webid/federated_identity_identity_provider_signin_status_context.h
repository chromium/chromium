// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_IDENTITY_PROVIDER_SIGNIN_STATUS_CONTEXT_H_
#define CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_IDENTITY_PROVIDER_SIGNIN_STATUS_CONTEXT_H_

#include <optional>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/types/optional_ref.h"
#include "base/values.h"
#include "components/permissions/object_permission_context_base.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-forward.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
}

namespace url {
class Origin;
}

namespace blink::common::webid {
struct LoginStatusOptions;
}  // namespace blink::common::webid

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
  // std::nullopt if there isn't a stored sign-in status.
  std::optional<bool> GetSigninStatus(const url::Origin& identity_provider);

  // Returns the stored profile information for the passed-in
  // `identity_provider`. If the signin status is false, no profile
  // information was stored, or the stored accounts have expired, this returns
  // an empty List. The consumer must validate that the accounts contain all
  // required fields.
  base::Value::List GetAccounts(const url::Origin& identity_provider);

  void SetSigninStatus(
      const url::Origin& identity_provider,
      bool signin_status,
      base::optional_ref<const blink::common::webid::LoginStatusOptions>
          options);

  // permissions::ObjectPermissionContextBase:
  std::string GetKeyForObject(const base::Value::Dict& object) override;

  // permissions::ObjectPermissionContextBase:
  void FlushScheduledSaveSettingsCalls();

 private:
  // permissions::ObjectPermissionContextBase:
  bool IsValidObject(const base::Value::Dict& object) override;
  std::u16string GetObjectDisplayName(const base::Value::Dict& object) override;
};

#endif  // CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_IDENTITY_PROVIDER_SIGNIN_STATUS_CONTEXT_H_
