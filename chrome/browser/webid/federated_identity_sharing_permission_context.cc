// Copyright 2021 The Chromium Authors. All rights reserved.
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
const char kRelyingPartyOriginKey[] = "rp-origin";
}  // namespace

FederatedIdentitySharingPermissionContext::
    FederatedIdentitySharingPermissionContext(
        content::BrowserContext* browser_context)
    : ObjectPermissionContextBase(
          ContentSettingsType::FEDERATED_IDENTITY_SHARING,
          HostContentSettingsMapFactory::GetForProfile(
              Profile::FromBrowserContext(browser_context))) {}

FederatedIdentitySharingPermissionContext::
    ~FederatedIdentitySharingPermissionContext() = default;

bool FederatedIdentitySharingPermissionContext::HasSharingPermission(
    const url::Origin& identity_provider,
    const url::Origin& relying_party) {
  const auto objects = GetGrantedObjects(identity_provider);

  for (const auto& object : objects) {
    if (GetKeyForObject(object->value) == relying_party.Serialize())
      return true;
  }

  return false;
}

void FederatedIdentitySharingPermissionContext::GrantSharingPermission(
    const url::Origin& identity_provider,
    const url::Origin& relying_party) {
  const auto objects = GetGrantedObjects(identity_provider);

  auto rp_string = relying_party.Serialize();

  base::Value new_object(base::Value::Type::DICTIONARY);
  new_object.SetStringKey(kRelyingPartyOriginKey, rp_string);

  for (const auto& object : objects) {
    if (GetKeyForObject(object->value) == rp_string) {
      UpdateObjectPermission(identity_provider, object->value,
                             std::move(new_object));
      return;
    }
  }

  GrantObjectPermission(identity_provider, std::move(new_object));
}

void FederatedIdentitySharingPermissionContext::RevokeSharingPermission(
    const url::Origin& identity_provider,
    const url::Origin& relying_party) {
  RevokeObjectPermission(identity_provider, relying_party.Serialize());
}

bool FederatedIdentitySharingPermissionContext::IsValidObject(
    const base::Value& object) {
  return object.is_dict() && object.FindStringKey(kRelyingPartyOriginKey);
}

std::u16string FederatedIdentitySharingPermissionContext::GetObjectDisplayName(
    const base::Value& object) {
  DCHECK(IsValidObject(object));
  return base::UTF8ToUTF16(*object.FindStringKey(kRelyingPartyOriginKey));
}

std::string FederatedIdentitySharingPermissionContext::GetKeyForObject(
    const base::Value& object) {
  DCHECK(IsValidObject(object));
  return std::string(*object.FindStringKey(kRelyingPartyOriginKey));
}
