// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_request_permission_context.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/browser_context.h"
#include "url/gurl.h"

namespace {
const char kIdpOriginKey[] = "idp-origin";
}  // namespace

FederatedIdentityRequestPermissionContext::
    FederatedIdentityRequestPermissionContext(
        content::BrowserContext* browser_context)
    : ObjectPermissionContextBase(
          ContentSettingsType::FEDERATED_IDENTITY_REQUEST,
          HostContentSettingsMapFactory::GetForProfile(
              Profile::FromBrowserContext(browser_context))) {}

FederatedIdentityRequestPermissionContext::
    ~FederatedIdentityRequestPermissionContext() = default;

bool FederatedIdentityRequestPermissionContext::HasRequestPermission(
    const url::Origin& relying_party,
    const url::Origin& identity_provider) {
  const auto objects = GetGrantedObjects(relying_party);

  for (const auto& object : objects) {
    if (GetKeyForObject(object->value) == identity_provider.Serialize())
      return true;
  }

  return false;
}

void FederatedIdentityRequestPermissionContext::GrantRequestPermission(
    const url::Origin& relying_party,
    const url::Origin& identity_provider) {
  const auto objects = GetGrantedObjects(relying_party);

  auto idp_string = identity_provider.Serialize();

  base::Value new_object(base::Value::Type::DICTIONARY);
  new_object.SetStringKey(kIdpOriginKey, idp_string);

  for (const auto& object : objects) {
    if (GetKeyForObject(object->value) == idp_string) {
      UpdateObjectPermission(relying_party, object->value,
                             std::move(new_object));
      return;
    }
  }

  GrantObjectPermission(relying_party, std::move(new_object));
}

void FederatedIdentityRequestPermissionContext::RevokeRequestPermission(
    const url::Origin& relying_party,
    const url::Origin& identity_provider) {
  RevokeObjectPermission(relying_party, identity_provider.Serialize());
}

bool FederatedIdentityRequestPermissionContext::IsValidObject(
    const base::Value& object) {
  return object.is_dict() && object.FindStringKey(kIdpOriginKey);
}

std::u16string FederatedIdentityRequestPermissionContext::GetObjectDisplayName(
    const base::Value& object) {
  DCHECK(IsValidObject(object));
  return base::UTF8ToUTF16(*object.FindStringKey(kIdpOriginKey));
}

std::string FederatedIdentityRequestPermissionContext::GetKeyForObject(
    const base::Value& object) {
  DCHECK(IsValidObject(object));
  return std::string(*object.FindStringKey(kIdpOriginKey));
}
