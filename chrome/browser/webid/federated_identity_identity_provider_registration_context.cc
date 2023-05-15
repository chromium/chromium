// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_identity_provider_registration_context.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace {

const char kIdPRegistrationKey[] = "idp-registration";
const char kIdPRegistrationUrlKey[] = "idp-registration-url";

}  // namespace

FederatedIdentityIdentityProviderRegistrationContext::
    FederatedIdentityIdentityProviderRegistrationContext(
        content::BrowserContext* browser_context)
    : ObjectPermissionContextBase(
          ContentSettingsType::
              FEDERATED_IDENTITY_IDENTITY_PROVIDER_REGISTRATION,
          HostContentSettingsMapFactory::GetForProfile(
              Profile::FromBrowserContext(browser_context))) {}

std::vector<GURL>
FederatedIdentityIdentityProviderRegistrationContext::GetRegisteredIdPs() {
  std::vector<GURL> result;
  for (const auto& object : GetAllGrantedObjects()) {
    if (!IsValidObject(object->value)) {
      continue;
    }
    auto* configURLs = object->value.FindList(kIdPRegistrationUrlKey);
    for (const auto& configURL : *configURLs) {
      result.emplace_back(GURL(configURL.GetString()));
    }
  }
  return result;
}

void FederatedIdentityIdentityProviderRegistrationContext::RegisterIdP(
    const GURL& url) {
  url::Origin origin = url::Origin::Create(url);
  auto granted_object = GetGrantedObject(origin, origin.Serialize());

  if (granted_object) {
    auto old = granted_object->value.Clone();
    auto* configURLs = granted_object->value.FindList(kIdPRegistrationUrlKey);
    // Remove any duplicates.
    configURLs->EraseValue(base::Value(url.spec()));
    configURLs->Append(base::Value(url.spec()));
    UpdateObjectPermission(origin, old, std::move(granted_object->value));
  } else {
    base::Value::Dict new_object;
    new_object.Set(kIdPRegistrationKey, origin.Serialize());
    base::Value::List configURLs;
    configURLs.Append(base::Value(url.spec()));
    new_object.Set(kIdPRegistrationUrlKey, std::move(configURLs));
    GrantObjectPermission(origin, std::move(new_object));
  }
}

void FederatedIdentityIdentityProviderRegistrationContext::UnregisterIdP(
    const GURL& url) {
  url::Origin origin = url::Origin::Create(url);
  auto granted_object = GetGrantedObject(origin, origin.Serialize());

  if (!granted_object) {
    return;
  }

  auto old = granted_object->value.Clone();
  auto* configURLs = granted_object->value.FindList(kIdPRegistrationUrlKey);
  configURLs->EraseValue(base::Value(url.spec()));
  UpdateObjectPermission(origin, old, std::move(granted_object->value));
}

std::string
FederatedIdentityIdentityProviderRegistrationContext::GetKeyForObject(
    const base::Value::Dict& object) {
  DCHECK(IsValidObject(object));
  return *object.FindString(kIdPRegistrationKey);
}

bool FederatedIdentityIdentityProviderRegistrationContext::IsValidObject(
    const base::Value::Dict& object) {
  return object.FindString(kIdPRegistrationKey);
}

std::u16string
FederatedIdentityIdentityProviderRegistrationContext::GetObjectDisplayName(
    const base::Value::Dict& object) {
  DCHECK(IsValidObject(object));
  return base::UTF8ToUTF16(*object.FindString(kIdPRegistrationKey));
}
