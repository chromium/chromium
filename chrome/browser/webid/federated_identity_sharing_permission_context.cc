// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_sharing_permission_context.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/browser_context.h"
#include "url/gurl.h"

namespace {
const char kIdpOriginKey[] = "idp-origin";
const char kIdpSigninStatusKey[] = "idp-signin-status";
}  // namespace

FederatedIdentitySharingPermissionContext::
    FederatedIdentitySharingPermissionContext(
        content::BrowserContext* browser_context)
    : FederatedIdentityAccountKeyedPermissionContext(
          browser_context,
          ContentSettingsType::FEDERATED_IDENTITY_SHARING,
          kIdpOriginKey) {}

FederatedIdentitySharingPermissionContext::
    ~FederatedIdentitySharingPermissionContext() = default;

bool FederatedIdentitySharingPermissionContext::HasSharingPermission(
    const url::Origin& relying_party_requester,
    const url::Origin& relying_party_embedder,
    const url::Origin& identity_provider,
    const std::string& account_id) {
  return HasPermission(relying_party_requester, relying_party_embedder,
                       identity_provider, account_id);
}

void FederatedIdentitySharingPermissionContext::GrantSharingPermission(
    const url::Origin& relying_party_requester,
    const url::Origin& relying_party_embedder,
    const url::Origin& identity_provider,
    const std::string& account_id) {
  GrantPermission(relying_party_requester, relying_party_embedder,
                  identity_provider, account_id);
}

absl::optional<bool>
FederatedIdentitySharingPermissionContext::GetIdpSigninStatus(
    const url::Origin& idp_origin) {
  auto granted_object = GetGrantedObject(idp_origin, idp_origin.Serialize());

  if (!granted_object)
    return absl::nullopt;

  return granted_object->value.GetDict().FindBool(kIdpSigninStatusKey);
}

void FederatedIdentitySharingPermissionContext::SetIdpSigninStatus(
    const url::Origin& idp_origin,
    bool idp_signin_status) {
  auto granted_object = GetGrantedObject(idp_origin, idp_origin.Serialize());
  if (granted_object) {
    base::Value new_object = granted_object->value.Clone();
    new_object.GetDict().Set(kIdpSigninStatusKey, idp_signin_status);
    UpdateObjectPermission(idp_origin, granted_object->value,
                           std::move(new_object));
  } else {
    base::Value::Dict new_object;
    new_object.Set(kIdpOriginKey, idp_origin.Serialize());
    new_object.Set(kIdpSigninStatusKey, base::Value(idp_signin_status));
    GrantObjectPermission(idp_origin, base::Value(std::move(new_object)));
  }
}
