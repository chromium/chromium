// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_active_session_permission_context.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/browser_context.h"
#include "url/gurl.h"

namespace {
const char kIdpKey[] = "identity-provider";
const char kAccountIdsKey[] = "account-ids";
}  // namespace

FederatedIdentityActiveSessionPermissionContext::
    FederatedIdentityActiveSessionPermissionContext(
        content::BrowserContext* browser_context)
    : ObjectPermissionContextBase(
          ContentSettingsType::FEDERATED_IDENTITY_ACTIVE_SESSION,
          HostContentSettingsMapFactory::GetForProfile(
              Profile::FromBrowserContext(browser_context))) {}

FederatedIdentityActiveSessionPermissionContext::
    ~FederatedIdentityActiveSessionPermissionContext() = default;

bool FederatedIdentityActiveSessionPermissionContext::HasActiveSession(
    const url::Origin& relying_party,
    const url::Origin& identity_provider,
    const std::string& account_identifier) {
  // TODO(kenrb): This is currently origin-bound, but we would like this
  // grant to apply at the 'site' (aka eTLD+1) level. We should override
  // GetGrantedObject to find a grant that matches the RP's site rather
  // than origin, and also ensure that duplicate sites cannot be added.
  // https://crbug.com/1223570.
  auto idp_string = identity_provider.Serialize();
  const auto object = GetGrantedObject(relying_party, idp_string);

  if (!object)
    return false;

  auto& account_ids = *object->value.FindListKey(kAccountIdsKey);
  for (const auto& account_id : account_ids.GetListDeprecated()) {
    if (account_id.GetString() == account_identifier)
      return true;
  }

  return false;
}

void FederatedIdentityActiveSessionPermissionContext::GrantActiveSession(
    const url::Origin& relying_party,
    const url::Origin& identity_provider,
    const std::string& account_identifier) {
  if (HasActiveSession(relying_party, identity_provider, account_identifier))
    return;

  auto idp_string = identity_provider.Serialize();
  const auto existing_object = GetGrantedObject(relying_party, idp_string);

  if (existing_object) {
    base::Value new_object = existing_object->value.Clone();
    auto& account_ids = *new_object.FindListKey(kAccountIdsKey);
    account_ids.Append(account_identifier);
    UpdateObjectPermission(relying_party, existing_object->value,
                           std::move(new_object));
    return;
  }

  base::Value new_object(base::Value::Type::DICTIONARY);
  new_object.SetStringKey(kIdpKey, idp_string);
  base::ListValue account_ids;
  account_ids.Append(account_identifier);
  new_object.SetKey(kAccountIdsKey, std::move(account_ids));
  GrantObjectPermission(relying_party, std::move(new_object));
}

void FederatedIdentityActiveSessionPermissionContext::RevokeActiveSession(
    const url::Origin& relying_party,
    const url::Origin& identity_provider,
    const std::string& account_identifier) {
  auto idp_string = identity_provider.Serialize();
  const auto object = GetGrantedObject(relying_party, idp_string);
  // TODO(cbiesinger): if the provided |account_identifier| does not match the
  // one we used when granting the permission, return early will leave an entry
  // in the storage that cannot be removed afterwards. This should be fixed as
  // part of https://crbug.com/1306852.
  if (!object)
    return;
  auto new_object = object->value.Clone();
  auto& account_ids = *new_object.FindListKey(kAccountIdsKey);
  account_ids.EraseListValue(base::Value(account_identifier));

  // Remove the permission object if there is no account left.
  if (account_ids.GetListDeprecated().size() == 0) {
    RevokeObjectPermission(relying_party, idp_string);
  } else {
    UpdateObjectPermission(relying_party, object->value, std::move(new_object));
  }
}

bool FederatedIdentityActiveSessionPermissionContext::IsValidObject(
    const base::Value& object) {
  return object.is_dict() && object.FindStringKey(kIdpKey) &&
         object.FindListKey(kAccountIdsKey);
}

std::u16string
FederatedIdentityActiveSessionPermissionContext::GetObjectDisplayName(
    const base::Value& object) {
  DCHECK(IsValidObject(object));
  return base::UTF8ToUTF16(*object.FindStringKey(kIdpKey));
}

std::string FederatedIdentityActiveSessionPermissionContext::GetKeyForObject(
    const base::Value& object) {
  DCHECK(IsValidObject(object));
  return std::string(*object.FindStringKey(kIdpKey));
}
