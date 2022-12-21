// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_account_keyed_permission_context.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace {
const char kAccountIdsKey[] = "account-ids";
const char kRpRequesterKey[] = "rp-requester";
const char kRpEmbedderKey[] = "rp-embedder";

void AddToAccountList(base::Value::Dict& dict, const std::string& account_id) {
  base::Value::List* account_list = dict.FindList(kAccountIdsKey);
  if (account_list) {
    account_list->Append(account_id);
    return;
  }

  base::Value::List new_list;
  new_list.Append(account_id);
  dict.Set(kAccountIdsKey, base::Value(std::move(new_list)));
}

std::string BuildKey(const absl::optional<std::string>& relying_party_requester,
                     const absl::optional<std::string>& relying_party_embedder,
                     const std::string& identity_provider) {
  if (relying_party_requester && relying_party_embedder &&
      *relying_party_requester != *relying_party_embedder) {
    return base::StringPrintf("%s<%s", identity_provider.c_str(),
                              relying_party_embedder->c_str());
  }
  // Use `identity_provider` as the key when
  // `relying_party_requester` == `relying_party_embedder` for the sake of
  // backwards compatibility with permissions stored prior to addition of
  // `relying_party_embedder` key.
  return identity_provider;
}

std::string BuildKey(const url::Origin& relying_party_requester,
                     const url::Origin& relying_party_embedder,
                     const url::Origin& identity_provider) {
  return BuildKey(relying_party_requester.Serialize(),
                  relying_party_embedder.Serialize(),
                  identity_provider.Serialize());
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
    const url::Origin& relying_party_requester,
    const url::Origin& relying_party_embedder,
    const url::Origin& identity_provider,
    const std::string& account_id) {
  // TODO(crbug.com/1334019): This is currently origin-bound, but we would like
  // this grant to apply at the 'site' (aka eTLD+1) level. We should override
  // GetGrantedObject to find a grant that matches the RP's site rather
  // than origin, and also ensure that duplicate sites cannot be added.
  std::string key = BuildKey(relying_party_requester, relying_party_embedder,
                             identity_provider);
  auto granted_object = GetGrantedObject(relying_party_requester, key);

  if (!granted_object)
    return false;

  const base::Value::List* account_list =
      granted_object->value.GetDict().FindList(kAccountIdsKey);
  if (!account_list)
    return false;

  for (auto& account_id_value : *account_list) {
    if (account_id_value.GetString() == account_id)
      return true;
  }

  return false;
}

void FederatedIdentityAccountKeyedPermissionContext::GrantPermission(
    const url::Origin& relying_party_requester,
    const url::Origin& relying_party_embedder,
    const url::Origin& identity_provider,
    const std::string& account_id) {
  if (HasPermission(relying_party_requester, relying_party_embedder,
                    identity_provider, account_id))
    return;

  std::string key = BuildKey(relying_party_requester, relying_party_embedder,
                             identity_provider);
  const auto granted_object = GetGrantedObject(relying_party_requester, key);
  if (granted_object) {
    base::Value new_object = granted_object->value.Clone();
    AddToAccountList(new_object.GetDict(), account_id);
    UpdateObjectPermission(relying_party_requester, granted_object->value,
                           std::move(new_object));
  } else {
    base::Value::Dict new_object;
    new_object.Set(kRpRequesterKey, relying_party_requester.Serialize());
    new_object.Set(kRpEmbedderKey, relying_party_embedder.Serialize());
    new_object.Set(idp_origin_key_, identity_provider.Serialize());
    AddToAccountList(new_object, account_id);
    GrantObjectPermission(relying_party_requester,
                          base::Value(std::move(new_object)));
  }
}

void FederatedIdentityAccountKeyedPermissionContext::RevokePermission(
    const url::Origin& relying_party_requester,
    const url::Origin& relying_party_embedder,
    const url::Origin& identity_provider,
    const std::string& account_id) {
  std::string key = BuildKey(relying_party_requester, relying_party_embedder,
                             identity_provider);
  const auto object = GetGrantedObject(relying_party_requester, key);
  // TODO(crbug.com/1311268): If the provided `account_id` does not match the
  // one we used when granting the permission, early return will leave an entry
  // in the storage that cannot be removed afterwards.
  if (!object)
    return;

  base::Value new_object = object->value.Clone();
  base::Value::List* account_ids =
      new_object.GetDict().FindList(kAccountIdsKey);
  if (account_ids)
    account_ids->EraseValue(base::Value(account_id));

  // Remove the permission object if there is no account left.
  if (!account_ids || account_ids->size() == 0) {
    RevokeObjectPermission(relying_party_requester, key);
  } else {
    UpdateObjectPermission(relying_party_requester, object->value,
                           std::move(new_object));
  }
}

std::string FederatedIdentityAccountKeyedPermissionContext::GetKeyForObject(
    const base::Value& object) {
  DCHECK(IsValidObject(object));
  const std::string* rp_requester_origin =
      object.GetDict().FindString(kRpRequesterKey);
  const std::string* rp_embedder_origin =
      object.GetDict().FindString(kRpEmbedderKey);
  const std::string* idp_origin = object.GetDict().FindString(idp_origin_key_);
  return BuildKey(
      rp_requester_origin ? absl::optional<std::string>(*rp_requester_origin)
                          : absl::nullopt,
      rp_embedder_origin ? absl::optional<std::string>(*rp_embedder_origin)
                         : absl::nullopt,
      *idp_origin);
}

bool FederatedIdentityAccountKeyedPermissionContext::IsValidObject(
    const base::Value& object) {
  return object.is_dict() && object.GetDict().FindString(idp_origin_key_);
}

std::u16string
FederatedIdentityAccountKeyedPermissionContext::GetObjectDisplayName(
    const base::Value& object) {
  DCHECK(IsValidObject(object));
  return base::UTF8ToUTF16(*object.GetDict().FindString(idp_origin_key_));
}
