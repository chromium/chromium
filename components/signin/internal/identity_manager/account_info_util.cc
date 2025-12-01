// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/account_info_util.h"

#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/signin/internal/identity_manager/account_capabilities_constants.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/signin_constants.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_id.h"

namespace signin {

namespace {

// Keys used to store the different values in the JSON dictionary received
// from gaia server.
namespace server {
constexpr std::string_view kGaiaIdKey = "id";
constexpr std::string_view kSubKey = "sub";
constexpr std::string_view kEmailKey = "email";
constexpr std::string_view kHostedDomainKey = "hd";
constexpr std::string_view kFullNameKey = "name";
constexpr std::string_view kGivenNameKey = "given_name";
constexpr std::string_view kLocaleKey = "locale";
constexpr std::string_view kPictureUrlKey = "picture";
constexpr std::string_view kAccountCapabilitiesListKey = "accountCapabilities";
constexpr std::string_view kAccountCapabilityNameKey = "name";
constexpr std::string_view kAccountCapabilityBooleanValueKey = "booleanValue";
}  // namespace server

// Keys used to store the different values in the JSON dictionary in a
// serialized form on disk.
namespace local {
using ::signin::kAccountIdKey;
using ::signin::kLastDownloadedImageURLWithSizeKey;
using ::signin::constants::kNoHostedDomainFound;
constexpr std::string_view kAccountEmailKey = "email";
constexpr std::string_view kAccountGaiaKey = "gaia";
constexpr std::string_view kAccountHostedDomainKey = "hd";
constexpr std::string_view kAccountFullNameKey = "full_name";
constexpr std::string_view kAccountGivenNameKey = "given_name";
constexpr std::string_view kAccountLocaleKey = "locale";
constexpr std::string_view kAccountPictureURLKey = "picture_url";
constexpr std::string_view kAccountChildAttributeKey = "is_supervised_child";
constexpr std::string_view kAdvancedProtectionAccountStatusKey =
    "is_under_advanced_protection";
constexpr std::string_view kAccountAccessPoint = "access_point";
constexpr std::string_view kAccountCapabilitiesKey = "accountcapabilities";
}  // namespace local

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
      DVLOG(1) << "Unexpected tribool value (" << int_value.value() << ")";
      return signin::Tribool::kUnknown;
  }
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

std::optional<AccountInfo> AccountInfoFromUserInfo(
    const base::Value::Dict& user_info) {
  // Both |gaia_id| and |email| are required value in the JSON reply, so
  // return empty result if any is missing or is empty.
  const std::string* gaia_id_value =
      FindStringIfNonEmpty(user_info, server::kGaiaIdKey);
  if (!gaia_id_value) {
    // The GAIA ID may be returned via a different property, based on which
    // endpoint the browser is communicating with.
    gaia_id_value = FindStringIfNonEmpty(user_info, server::kSubKey);
    if (!gaia_id_value) {
      return std::nullopt;
    }
  }

  const std::string* email_value =
      FindStringIfNonEmpty(user_info, server::kEmailKey);
  if (!email_value) {
    return std::nullopt;
  }

  AccountInfo::Builder builder(GaiaId(*gaia_id_value), *email_value);

  // The following fields remain "unknown" in `AccountInfo` if they are not set
  // or empty in the `user_info`.
  if (const std::string* full_name_value =
          FindStringIfNonEmpty(user_info, server::kFullNameKey)) {
    builder.SetFullName(*full_name_value);
  }

  if (const std::string* given_name_value =
          FindStringIfNonEmpty(user_info, server::kGivenNameKey)) {
    builder.SetGivenName(*given_name_value);
  }

  if (const std::string* locale_value =
          FindStringIfNonEmpty(user_info, server::kLocaleKey)) {
    builder.SetLocale(*locale_value);
  }

  // The following fields will be set as "empty" in `AccountInfo` if they are
  // not set or empty in the `user_info`.
  const std::string* hosted_domain_value =
      user_info.FindString(server::kHostedDomainKey);
  builder.SetHostedDomain(hosted_domain_value ? *hosted_domain_value
                                              : std::string());

  const std::string* picture_url_value =
      user_info.FindString(server::kPictureUrlKey);
  builder.SetAvatarUrl(picture_url_value ? *picture_url_value : std::string());

  return builder.Build();
}

std::optional<AccountCapabilities> AccountCapabilitiesFromServerResponse(
    const base::Value::Dict& account_capabilities) {
  const base::Value::List* list =
      account_capabilities.FindList(server::kAccountCapabilitiesListKey);
  if (!list) {
    return std::nullopt;
  }

  base::flat_map<std::string, bool> capabilities_map;
  for (const auto& capability_value : *list) {
    const std::string* name = capability_value.GetDict().FindString(
        server::kAccountCapabilityNameKey);
    if (!name) {
      return std::nullopt;  // name is a required field.
    }

    // Check whether a capability has a boolean value.
    std::optional<bool> boolean_value = capability_value.GetDict().FindBool(
        server::kAccountCapabilityBooleanValueKey);
    if (!boolean_value) {
      continue;
    }

    // Add the capability to the map if it's supported in Chrome.
    if (base::Contains(
            AccountCapabilities::GetSupportedAccountCapabilityNames(), *name)) {
      capabilities_map.insert({*name, *boolean_value});
    }
  }

  return AccountCapabilities(std::move(capabilities_map));
}

base::Value::Dict SerializeAccountCapabilities(
    const AccountCapabilities& account_capabilities) {
  base::Value::Dict dict;
  for (std::string_view name :
       AccountCapabilities::GetSupportedAccountCapabilityNames()) {
    signin::Tribool capability_state =
        account_capabilities.GetCapabilityByName(name);
    dict.Set(name, static_cast<int>(capability_state));
  }
  return dict;
}

AccountCapabilities DeserializeAccountCapabilities(
    const base::Value::Dict& dict) {
  base::flat_map<std::string, bool> capabilities_map;
  for (std::string_view name :
       AccountCapabilities::GetSupportedAccountCapabilityNames()) {
    signin::Tribool state = ParseTribool(dict.FindInt(name));
    if (state != signin::Tribool::kUnknown) {
      capabilities_map.emplace(name, state == signin::Tribool::kTrue);
    }
  }
  return AccountCapabilities(std::move(capabilities_map));
}

base::Value::Dict SerializeAccountInfo(const AccountInfo& account_info) {
  std::string hosted_domain_to_set;
  if (std::optional<std::string_view> hosted_domain =
          account_info.GetHostedDomain()) {
    hosted_domain_to_set = hosted_domain->empty() ? local::kNoHostedDomainFound
                                                  : std::string(*hosted_domain);
  }
  return base::Value::Dict()
      .Set(local::kAccountIdKey, account_info.account_id.ToString())
      .Set(local::kAccountEmailKey, account_info.email)
      .Set(local::kAccountGaiaKey, account_info.gaia.ToString())
      .Set(local::kAccountHostedDomainKey, hosted_domain_to_set)
      .Set(local::kAccountFullNameKey, account_info.full_name)
      .Set(local::kAccountGivenNameKey, account_info.given_name)
      .Set(local::kAccountLocaleKey, account_info.locale)
      .Set(local::kAccountPictureURLKey, account_info.picture_url)
      .Set(local::kAccountChildAttributeKey,
           static_cast<int>(account_info.is_child_account))
      .Set(local::kAdvancedProtectionAccountStatusKey,
           account_info.is_under_advanced_protection)
      .Set(local::kAccountAccessPoint,
           static_cast<int>(account_info.access_point))
      .Set(local::kLastDownloadedImageURLWithSizeKey,
           account_info.last_downloaded_image_url_with_size)
      .Set(local::kAccountCapabilitiesKey,
           SerializeAccountCapabilities(account_info.capabilities));
}

std::optional<AccountInfo> DeserializeAccountInfo(
    const base::Value::Dict& dict) {
  const std::string* account_id =
      FindStringIfNonEmpty(dict, local::kAccountIdKey);
  const std::string* email =
      FindStringIfNonEmpty(dict, local::kAccountEmailKey);

  if (!email || !account_id) {
    // Cannot build `AccountInfo` without an email or account_id.
    return std::nullopt;
  }

  const std::string* gaia_id =
      FindStringIfNonEmpty(dict, local::kAccountGaiaKey);
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

  builder.SetAccountId(CoreAccountId::FromString(*account_id));

  if (const std::string* hosted_domain =
          FindStringIfNonEmpty(dict, local::kAccountHostedDomainKey)) {
    builder.SetHostedDomain(*hosted_domain);
  }
  if (const std::string* full_name =
          FindStringIfNonEmpty(dict, local::kAccountFullNameKey)) {
    builder.SetFullName(*full_name);
  }
  if (const std::string* given_name =
          FindStringIfNonEmpty(dict, local::kAccountGivenNameKey)) {
    builder.SetGivenName(*given_name);
  }
  if (const std::string* locale =
          FindStringIfNonEmpty(dict, local::kAccountLocaleKey)) {
    builder.SetLocale(*locale);
  }
  if (const std::string* picture_url =
          FindStringIfNonEmpty(dict, local::kAccountPictureURLKey)) {
    builder.SetAvatarUrl(*picture_url);
  }
  if (const std::string* last_downloaded_image_url_with_size =
          FindStringIfNonEmpty(dict,
                               local::kLastDownloadedImageURLWithSizeKey)) {
    builder.SetLastDownloadedAvatarUrlWithSize(
        *last_downloaded_image_url_with_size);
  }
  if (std::optional<bool> is_under_advanced_protection =
          dict.FindBool(local::kAdvancedProtectionAccountStatusKey)) {
    builder.SetIsUnderAdvancedProtection(*is_under_advanced_protection);
  }
  if (std::optional<int> access_point =
          dict.FindInt(local::kAccountAccessPoint)) {
    builder.SetLastAuthenticationAccessPoint(
        static_cast<signin_metrics::AccessPoint>(*access_point));
  }
  builder.SetIsChildAccount(
      ParseTribool(dict.FindInt(local::kAccountChildAttributeKey)));
  if (const base::Value::Dict* capabilities =
          dict.FindDict(local::kAccountCapabilitiesKey)) {
    builder.UpdateAccountCapabilitiesWith(
        DeserializeAccountCapabilities(*capabilities));
  }
  return builder.Build();
}

}  // namespace signin
