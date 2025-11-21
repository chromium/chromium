// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/account_info_serializer.h"

#include <string_view>

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_id.h"

namespace signin {

const char kAccountIdKey[] = "account_id";
const char kLastDownloadedImageURLWithSizeKey[] =
    "last_downloaded_image_url_with_size";

}  // namespace signin

namespace {

const char kAccountEmailKey[] = "email";
const char kAccountGaiaKey[] = "gaia";
const char kAccountHostedDomainKey[] = "hd";
const char kAccountFullNameKey[] = "full_name";
const char kAccountGivenNameKey[] = "given_name";
const char kAccountLocaleKey[] = "locale";
const char kAccountPictureURLKey[] = "picture_url";
const char kAccountChildAttributeKey[] = "is_supervised_child";
const char kAdvancedProtectionAccountStatusKey[] =
    "is_under_advanced_protection";
const char kAccountAccessPoint[] = "access_point";

// Converts the capability service name into a nested Chrome pref path.
std::string GetCapabilityPrefPath(std::string_view capability_name) {
  return base::StrCat({"accountcapabilities.", capability_name});
}

void SetAccountCapabilityState(base::Value::Dict& value,
                               std::string_view capability_name,
                               signin::Tribool state) {
  value.SetByDottedPath(GetCapabilityPrefPath(capability_name),
                        static_cast<int>(state));
}

signin::Tribool ParseTribool(std::optional<int> int_value) {
  if (!int_value.has_value()) {
    return signin::Tribool::kUnknown;
  }
  switch (int_value.value()) {
    case static_cast<int>(signin::Tribool::kTrue):
      return signin::Tribool::kTrue;
    case static_cast<int>(signin::Tribool::kFalse):
      return signin::Tribool::kFalse;
    case static_cast<int>(signin::Tribool::kUnknown):
      return signin::Tribool::kUnknown;
    default:
      LOG(ERROR) << "Unexpected tribool value (" << int_value.value() << ")";
      return signin::Tribool::kUnknown;
  }
}

signin::Tribool FindAccountCapabilityState(const base::Value::Dict& dict,
                                           std::string_view name) {
  std::optional<int> capability =
      dict.FindIntByDottedPath(GetCapabilityPrefPath(name));
  return ParseTribool(capability);
}

// Returns a non-empty string found in `dict` by `key` or nullptr if a string is
// not found or `key` contains an empty string.
const std::string* FindStringIfNonEmpty(const base::Value::Dict& dict,
                                        std::string_view key) {
  const std::string* value = dict.FindString(key);
  if (!value) {
    return value;
  }
  return value->empty() ? nullptr : value;
}

}  // namespace

// static
AccountCapabilities signin::AccountInfoSerializer::LoadAccountCapabilities(
    const base::Value::Dict& dict) {
  base::flat_map<std::string, bool> capabilities_map;
  for (std::string_view name :
       AccountCapabilities::GetSupportedAccountCapabilityNames()) {
    signin::Tribool state = FindAccountCapabilityState(dict, name);
    if (state != signin::Tribool::kUnknown) {
      capabilities_map.emplace(name, state == signin::Tribool::kTrue);
    }
  }
  return AccountCapabilities(std::move(capabilities_map));
}

// static
std::optional<AccountInfo>
signin::AccountInfoSerializer::DeserializeAccountInfoFromDict(
    const base::Value::Dict& dict,
    const CoreAccountId& account_id) {
  const std::string* gaia_id = FindStringIfNonEmpty(dict, kAccountGaiaKey);
  const std::string* email = FindStringIfNonEmpty(dict, kAccountEmailKey);

  if (!email) {
    // Cannot build `AccountInfo` without an email.
    return std::nullopt;
  }

#if BUILDFLAG(IS_CHROMEOS)
  AccountInfo::Builder builder =
      AccountInfo::Builder::CreateWithPossiblyEmptyGaiaId(
          GaiaId(gaia_id ? *gaia_id : std::string()), *email);
#else
  if (!gaia_id) {
    // Gaia ID is required on all platforms except ChromeOS.
    // TODO(crbug.com/40268200): Remove this exception after the migration is
    // done.
    return std::nullopt;
  }

  AccountInfo::Builder builder(GaiaId(*gaia_id), *email);
#endif  // BUILDFLAG(IS_CHROMEOS)

  builder.SetAccountId(account_id);

  if (const std::string* hosted_domain =
          FindStringIfNonEmpty(dict, kAccountHostedDomainKey)) {
    builder.SetHostedDomain(*hosted_domain);
  }
  if (const std::string* full_name =
          FindStringIfNonEmpty(dict, kAccountFullNameKey)) {
    builder.SetFullName(*full_name);
  }
  if (const std::string* given_name =
          FindStringIfNonEmpty(dict, kAccountGivenNameKey)) {
    builder.SetGivenName(*given_name);
  }
  if (const std::string* locale =
          FindStringIfNonEmpty(dict, kAccountLocaleKey)) {
    builder.SetLocale(*locale);
  }
  if (const std::string* picture_url =
          FindStringIfNonEmpty(dict, kAccountPictureURLKey)) {
    builder.SetAvatarUrl(*picture_url);
  }
  if (const std::string* last_downloaded_image_url_with_size =
          FindStringIfNonEmpty(dict,
                               signin::kLastDownloadedImageURLWithSizeKey)) {
    builder.SetLastDownloadedAvatarUrlWithSize(
        *last_downloaded_image_url_with_size);
  }
  if (std::optional<bool> is_under_advanced_protection =
          dict.FindBool(kAdvancedProtectionAccountStatusKey)) {
    builder.SetIsUnderAdvancedProtection(*is_under_advanced_protection);
  }
  if (std::optional<int> access_point = dict.FindInt(kAccountAccessPoint)) {
    builder.SetLastAuthenticationAccessPoint(
        static_cast<signin_metrics::AccessPoint>(*access_point));
  }
  builder.SetIsChildAccount(
      ParseTribool(dict.FindInt(kAccountChildAttributeKey)));
  builder.UpdateAccountCapabilitiesWith(LoadAccountCapabilities(dict));
  return builder.Build();
}

// static
void signin::AccountInfoSerializer::SerializeAccountInfoToDict(
    base::Value::Dict& dict,
    const AccountInfo& account_info) {
  dict.Set(kAccountEmailKey, account_info.email);
  dict.Set(kAccountGaiaKey, account_info.gaia.ToString());
  dict.Set(kAccountHostedDomainKey, account_info.hosted_domain);
  dict.Set(kAccountFullNameKey, account_info.full_name);
  dict.Set(kAccountGivenNameKey, account_info.given_name);
  dict.Set(kAccountLocaleKey, account_info.locale);
  dict.Set(kAccountPictureURLKey, account_info.picture_url);
  dict.Set(kAccountChildAttributeKey,
           static_cast<int>(account_info.is_child_account));
  dict.Set(kAdvancedProtectionAccountStatusKey,
           account_info.is_under_advanced_protection);
  dict.Set(kAccountAccessPoint, static_cast<int>(account_info.access_point));
  for (std::string_view name :
       AccountCapabilities::GetSupportedAccountCapabilityNames()) {
    signin::Tribool capability_state =
        account_info.capabilities.GetCapabilityByName(name);
    SetAccountCapabilityState(dict, name, capability_state);
  }
  dict.Set(signin::kLastDownloadedImageURLWithSizeKey,
           account_info.last_downloaded_image_url_with_size);
}

// static
base::Value::Dict signin::AccountInfoSerializer::ToValue(
    const AccountInfo& account_info) {
  base::Value::Dict dict;
  dict.Set(signin::kAccountIdKey, account_info.account_id.ToString());
  SerializeAccountInfoToDict(dict, account_info);
  return dict;
}

// static
std::optional<AccountInfo> signin::AccountInfoSerializer::FromValue(
    const base::Value::Dict& dict) {
  const std::string* account_key =
      FindStringIfNonEmpty(dict, signin::kAccountIdKey);
  if (!account_key) {
    return std::nullopt;
  }

  return DeserializeAccountInfoFromDict(
      dict, CoreAccountId::FromString(*account_key));
}
