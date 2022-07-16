// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_sharing_permission_context.h"

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/browser_context.h"

#include "base/logging.h"
#include "url/gurl.h"

namespace {
const char kRelyingPartyOriginKey[] = "rp-origin";
const char kAccountIdsKey[] = "account-ids";

std::string GetUniqueKey(const std::string& relying_party, bool has_accounts) {
  if (has_accounts) {
    return base::StringPrintf("%s|accounts", relying_party.c_str());
  }
  return relying_party;
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

bool FederatedIdentitySharingPermissionContext::HasSharingPermission(
    const url::Origin& identity_provider,
    const url::Origin& relying_party) {
  const auto key = GetUniqueKey(relying_party.Serialize(), false);
  auto granted_object = GetGrantedObject(identity_provider, key);
  // Existence of a grant object for this IDP and with the RP origin unique key
  // means we have a generic sharing permission grant for RP/IDP.
  return !!granted_object;
}

bool FederatedIdentitySharingPermissionContext::HasSharingPermissionForAccount(
    const url::Origin& identity_provider,
    const url::Origin& relying_party,
    const std::string& account_id) {
  const auto key = GetUniqueKey(relying_party.Serialize(), true);
  auto granted_object = GetGrantedObject(identity_provider, key);

  if (!granted_object)
    return false;

  auto& account_list = *granted_object->value.FindListKey(kAccountIdsKey);
  for (auto& account_id_value : account_list.GetList()) {
    if (account_id_value.GetString() == account_id)
      return true;
  }

  return false;
}

void FederatedIdentitySharingPermissionContext::GrantSharingPermission(
    const url::Origin& identity_provider,
    const url::Origin& relying_party) {
  base::Value new_object(base::Value::Type::DICTIONARY);
  new_object.SetStringKey(kRelyingPartyOriginKey, relying_party.Serialize());
  GrantObjectPermission(identity_provider, std::move(new_object));
}

void FederatedIdentitySharingPermissionContext::
    GrantSharingPermissionForAccount(const url::Origin& identity_provider,
                                     const url::Origin& relying_party,
                                     const std::string& account_id) {
  auto rp_string = relying_party.Serialize();
  const auto key = GetUniqueKey(rp_string, true);
  auto granted_object = GetGrantedObject(identity_provider, key);
  if (granted_object) {
    // There is an existing account so update its account list rather than
    // creating a new entry.
    base::Value new_object = granted_object->value.Clone();
    auto& account_list = *new_object.FindListKey(kAccountIdsKey);
    account_list.Append(account_id);
    UpdateObjectPermission(identity_provider, granted_object->value,
                           std::move(new_object));
  } else {
    base::Value new_object(base::Value::Type::DICTIONARY);
    new_object.SetStringKey(kRelyingPartyOriginKey, rp_string);
    base::ListValue account_list;
    account_list.Append(account_id);
    new_object.SetKey(kAccountIdsKey, std::move(account_list));
    GrantObjectPermission(identity_provider, std::move(new_object));
  }
}

void FederatedIdentitySharingPermissionContext::RevokeSharingPermission(
    const url::Origin& identity_provider,
    const url::Origin& relying_party) {
  const auto key = GetUniqueKey(relying_party.Serialize(), false);
  RevokeObjectPermission(identity_provider, key);
}

void FederatedIdentitySharingPermissionContext::
    RevokeSharingPermissionForAccount(const url::Origin& identity_provider,
                                      const url::Origin& relying_party,
                                      const std::string& account_id) {
  const auto key = GetUniqueKey(relying_party.Serialize(), true);
  auto granted_object = GetGrantedObject(identity_provider, key);
  auto new_object = granted_object->value.Clone();
  auto& account_list = *new_object.FindListKey(kAccountIdsKey);
  account_list.EraseListValue(base::Value(account_id));

  // Remove the permission object if there is no account left.
  if (account_list.GetList().size() == 0) {
    RevokeObjectPermission(identity_provider, key);
  } else {
    UpdateObjectPermission(identity_provider, granted_object->value,
                           std::move(new_object));
  }
}

bool FederatedIdentitySharingPermissionContext::IsValidObject(
    const base::Value& object) {
  // kAccountIds is optional as it is not needed for general sharing permission.
  return object.is_dict() && object.FindStringKey(kRelyingPartyOriginKey);
}

std::u16string FederatedIdentitySharingPermissionContext::GetObjectDisplayName(
    const base::Value& object) {
  // TODO(majidvp): Consider using a user friendly identifier for account for
  // example email address.
  DCHECK(IsValidObject(object));
  const auto rp_string = *object.FindStringKey(kRelyingPartyOriginKey);
  if (auto* account_ids = object.FindListKey(kAccountIdsKey)) {
    std::vector<std::string> ids(account_ids->GetList().size());
    for (const base::Value& account_id : account_ids->GetList()) {
      ids.push_back(account_id.GetString());
    }
    return base::UTF8ToUTF16(base::StrCat(
        {rp_string, " with accounts: ", base::JoinString(ids, ", ")}));
  } else {
    return base::UTF8ToUTF16(rp_string);
  }
}

std::string FederatedIdentitySharingPermissionContext::GetKeyForObject(
    const base::Value& object) {
  DCHECK(IsValidObject(object));
  bool has_accounts = !!object.FindListKey(kAccountIdsKey);
  const auto rp_string = *object.FindStringKey(kRelyingPartyOriginKey);
  return GetUniqueKey(rp_string, has_accounts);
}
