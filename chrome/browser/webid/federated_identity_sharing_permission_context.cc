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
const char kIdpOriginKey[] = "idp-origin";
const char kAccountIdsKey[] = "account-ids";

base::Value::List* ExtractAccountList(base::Value& value) {
  return value.GetDict().FindList(kAccountIdsKey);
}

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

bool FederatedIdentitySharingPermissionContext::
    HasSharingPermissionForAnyAccount(const url::Origin& relying_party,
                                      const url::Origin& identity_provider) {
  const std::string key = identity_provider.Serialize();
  return GetGrantedObject(relying_party, key).get();
}

bool FederatedIdentitySharingPermissionContext::HasSharingPermission(
    const url::Origin& relying_party,
    const url::Origin& identity_provider,
    const std::string& account_id) {
  const std::string key = identity_provider.Serialize();
  auto granted_object = GetGrantedObject(relying_party, key);

  if (!granted_object)
    return false;

  const base::Value::List* account_list =
      ExtractAccountList(granted_object->value);
  for (auto& account_id_value : *account_list) {
    if (account_id_value.GetString() == account_id)
      return true;
  }

  return false;
}

void FederatedIdentitySharingPermissionContext::GrantSharingPermission(
    const url::Origin& relying_party,
    const url::Origin& identity_provider,
    const std::string& account_id) {
  if (HasSharingPermission(relying_party, identity_provider, account_id))
    return;

  std::string idp_string = identity_provider.Serialize();
  auto granted_object = GetGrantedObject(relying_party, idp_string);
  if (granted_object) {
    // There is an existing account so update its account list rather than
    // creating a new entry.
    base::Value new_object = granted_object->value.Clone();
    base::Value::List* new_object_list = ExtractAccountList(new_object);
    new_object_list->Append(account_id);
    UpdateObjectPermission(relying_party, granted_object->value,
                           std::move(new_object));
  } else {
    base::Value::Dict new_object;
    new_object.Set(kIdpOriginKey, idp_string);
    base::Value::List account_list;
    account_list.Append(account_id);
    new_object.Set(kAccountIdsKey, base::Value(std::move(account_list)));
    GrantObjectPermission(relying_party, base::Value(std::move(new_object)));
  }
}

void FederatedIdentitySharingPermissionContext::RevokeSharingPermission(
    const url::Origin& relying_party,
    const url::Origin& identity_provider,
    const std::string& account_id) {
  std::string idp_string = identity_provider.Serialize();
  const auto object = GetGrantedObject(relying_party, idp_string);
  if (!object)
    return;

  base::Value new_object = object->value.Clone();
  base::Value::List& account_ids =
      new_object.FindListKey(kAccountIdsKey)->GetList();
  account_ids.EraseValue(base::Value(account_id));

  // Remove the permission object if there is no account left.
  if (account_ids.size() == 0) {
    RevokeObjectPermission(relying_party, idp_string);
  } else {
    UpdateObjectPermission(relying_party, object->value, std::move(new_object));
  }
}

bool FederatedIdentitySharingPermissionContext::IsValidObject(
    const base::Value& object) {
  return object.is_dict() && object.FindStringKey(kIdpOriginKey);
}

std::u16string FederatedIdentitySharingPermissionContext::GetObjectDisplayName(
    const base::Value& object) {
  DCHECK(IsValidObject(object));
  return base::UTF8ToUTF16(*object.FindStringKey(kIdpOriginKey));
}

std::string FederatedIdentitySharingPermissionContext::GetKeyForObject(
    const base::Value& object) {
  DCHECK(IsValidObject(object));
  return std::string(*object.FindStringKey(kIdpOriginKey));
}
