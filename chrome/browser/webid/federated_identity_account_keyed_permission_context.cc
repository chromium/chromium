// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_account_keyed_permission_context.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "url/origin.h"

namespace {
const char kAccountIdsKey[] = "account-ids";

base::Value::List* ExtractAccountList(base::Value& value) {
  return value.GetDict().FindList(kAccountIdsKey);
}
}  // namespace

FederatedIdentityAccountKeyedPermissionContext::
    FederatedIdentityAccountKeyedPermissionContext(
        content::BrowserContext* browser_context,
        ContentSettingsType content_settings_type,
        const std::string& idp_origin_key)
    : ObjectPermissionContextBase(
          content_settings_type,
          HostContentSettingsMapFactory::GetForProfile(
              Profile::FromBrowserContext(browser_context))),
      idp_origin_key_(idp_origin_key) {}

bool FederatedIdentityAccountKeyedPermissionContext::HasPermission(
    const url::Origin& relying_party,
    const url::Origin& identity_provider,
    const absl::optional<std::string>& account_id) {
  // TODO(crbug.com/1334019): This is currently origin-bound, but we would like
  // this grant to apply at the 'site' (aka eTLD+1) level. We should override
  // GetGrantedObject to find a grant that matches the RP's site rather
  // than origin, and also ensure that duplicate sites cannot be added.
  const std::string key = identity_provider.Serialize();
  auto granted_object = GetGrantedObject(relying_party, key);

  if (!granted_object)
    return false;

  if (!account_id)
    return true;

  const base::Value::List* account_list =
      ExtractAccountList(granted_object->value);
  for (auto& account_id_value : *account_list) {
    if (account_id_value.GetString() == account_id)
      return true;
  }

  return false;
}

void FederatedIdentityAccountKeyedPermissionContext::GrantPermission(
    const url::Origin& relying_party,
    const url::Origin& identity_provider,
    const std::string& account_id) {
  if (HasPermission(relying_party, identity_provider, account_id))
    return;

  std::string idp_string = identity_provider.Serialize();
  const auto granted_object = GetGrantedObject(relying_party, idp_string);
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
    new_object.Set(idp_origin_key_, idp_string);
    base::Value::List account_list;
    account_list.Append(account_id);
    new_object.Set(kAccountIdsKey, base::Value(std::move(account_list)));
    GrantObjectPermission(relying_party, base::Value(std::move(new_object)));
  }
}

void FederatedIdentityAccountKeyedPermissionContext::RevokePermission(
    const url::Origin& relying_party,
    const url::Origin& identity_provider,
    const std::string& account_id) {
  std::string idp_string = identity_provider.Serialize();
  const auto object = GetGrantedObject(relying_party, idp_string);
  // TODO(crbug.com/1311268): If the provided `account_id` does not match the
  // one we used when granting the permission, early return will leave an entry
  // in the storage that cannot be removed afterwards.
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

bool FederatedIdentityAccountKeyedPermissionContext::IsValidObject(
    const base::Value& object) {
  return object.is_dict() && object.FindStringKey(idp_origin_key_);
}

std::u16string
FederatedIdentityAccountKeyedPermissionContext::GetObjectDisplayName(
    const base::Value& object) {
  DCHECK(IsValidObject(object));
  return base::UTF8ToUTF16(*object.FindStringKey(idp_origin_key_));
}

std::string FederatedIdentityAccountKeyedPermissionContext::GetKeyForObject(
    const base::Value& object) {
  DCHECK(IsValidObject(object));
  return std::string(*object.FindStringKey(idp_origin_key_));
}
