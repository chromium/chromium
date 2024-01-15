// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_identity_provider_signin_status_context.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace {

const char kIdpKey[] = "idp-origin";
const char kIdpSigninStatusKey[] = "idp-signin-status";

}  // namespace

FederatedIdentityIdentityProviderSigninStatusContext::
    FederatedIdentityIdentityProviderSigninStatusContext(
        content::BrowserContext* browser_context)
    : ObjectPermissionContextBase(
          ContentSettingsType::
              FEDERATED_IDENTITY_IDENTITY_PROVIDER_SIGNIN_STATUS,
          HostContentSettingsMapFactory::GetForProfile(
              Profile::FromBrowserContext(browser_context))) {}

std::optional<bool>
FederatedIdentityIdentityProviderSigninStatusContext::GetSigninStatus(
    const url::Origin& identity_provider) {
  auto granted_object =
      GetGrantedObject(identity_provider, identity_provider.Serialize());
  if (!granted_object)
    return std::nullopt;

  return granted_object->value.FindBool(kIdpSigninStatusKey);
}

void FederatedIdentityIdentityProviderSigninStatusContext::SetSigninStatus(
    const url::Origin& identity_provider,
    bool signin_status) {
  auto granted_object =
      GetGrantedObject(identity_provider, identity_provider.Serialize());

  base::Value::Dict new_object;
  new_object.Set(kIdpKey, identity_provider.Serialize());
  new_object.Set(kIdpSigninStatusKey, base::Value(signin_status));
  if (granted_object) {
    UpdateObjectPermission(identity_provider, granted_object->value,
                           std::move(new_object));
  } else {
    GrantObjectPermission(identity_provider, std::move(new_object));
  }
}

std::string
FederatedIdentityIdentityProviderSigninStatusContext::GetKeyForObject(
    const base::Value::Dict& object) {
  return *object.FindString(kIdpKey);
}

bool FederatedIdentityIdentityProviderSigninStatusContext::IsValidObject(
    const base::Value::Dict& object) {
  return object.FindString(kIdpKey);
}

std::u16string
FederatedIdentityIdentityProviderSigninStatusContext::GetObjectDisplayName(
    const base::Value::Dict& object) {
  DCHECK(IsValidObject(object));
  return base::UTF8ToUTF16(*object.FindString(kIdpKey));
}
