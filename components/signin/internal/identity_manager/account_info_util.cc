// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/account_info_util.h"

#include <map>
#include <string>

#include "base/values.h"
#include "components/signin/internal/identity_manager/account_capabilities_constants.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {
// Keys used to store the different values in the JSON dictionary received
// from gaia server.
const char kGaiaIdKey[] = "id";
const char kEmailKey[] = "email";
const char kHostedDomainKey[] = "hd";
const char kFullNameKey[] = "name";
const char kGivenNameKey[] = "given_name";
const char kLocaleKey[] = "locale";
const char kPictureUrlKey[] = "picture";
const char kAccountCapabilitiesListKey[] = "accountCapabilities";
const char kAccountCapabilityNameKey[] = "name";
const char kAccountCapabilityBooleanValueKey[] = "booleanValue";
}  // namespace

absl::optional<AccountInfo> AccountInfoFromUserInfo(
    const base::Value& user_info) {
  if (!user_info.is_dict())
    return absl::nullopt;

  // Both |gaia_id| and |email| are required value in the JSON reply, so
  // return empty result if any is missing.
  const base::Value* gaia_id_value =
      user_info.FindKeyOfType(kGaiaIdKey, base::Value::Type::STRING);
  if (!gaia_id_value)
    return absl::nullopt;

  const base::Value* email_value =
      user_info.FindKeyOfType(kEmailKey, base::Value::Type::STRING);
  if (!email_value)
    return absl::nullopt;

  AccountInfo account_info;
  account_info.email = email_value->GetString();
  account_info.gaia = gaia_id_value->GetString();

  // All other fields are optional, some with default values.
  const base::Value* hosted_domain_value =
      user_info.FindKeyOfType(kHostedDomainKey, base::Value::Type::STRING);
  if (hosted_domain_value && !hosted_domain_value->GetString().empty())
    account_info.hosted_domain = hosted_domain_value->GetString();
  else
    account_info.hosted_domain = kNoHostedDomainFound;

  const base::Value* full_name_value =
      user_info.FindKeyOfType(kFullNameKey, base::Value::Type::STRING);
  if (full_name_value)
    account_info.full_name = full_name_value->GetString();

  const base::Value* given_name_value =
      user_info.FindKeyOfType(kGivenNameKey, base::Value::Type::STRING);
  if (given_name_value)
    account_info.given_name = given_name_value->GetString();

  const base::Value* locale_value =
      user_info.FindKeyOfType(kLocaleKey, base::Value::Type::STRING);
  if (locale_value)
    account_info.locale = locale_value->GetString();

  const base::Value* picture_url_value =
      user_info.FindKeyOfType(kPictureUrlKey, base::Value::Type::STRING);
  if (picture_url_value && !picture_url_value->GetString().empty())
    account_info.picture_url = picture_url_value->GetString();
  else
    account_info.picture_url = kNoPictureURLFound;

  return account_info;
}

absl::optional<AccountCapabilities> AccountCapabilitiesFromValue(
    const base::Value& account_capabilities) {
  if (!account_capabilities.is_dict())
    return absl::nullopt;

  const base::Value* list =
      account_capabilities.FindListKey(kAccountCapabilitiesListKey);
  if (!list)
    return absl::nullopt;

  // 1. Create "capability name" -> "boolean value" mapping.
  std::map<std::string, bool> boolean_capabilities;
  for (const auto& capability_value : list->GetListDeprecated()) {
    const std::string* name =
        capability_value.FindStringKey(kAccountCapabilityNameKey);
    if (!name)
      return absl::nullopt;  // name is a required field.

    // Check whether a capability has a boolean value.
    absl::optional<bool> boolean_value =
        capability_value.FindBoolKey(kAccountCapabilityBooleanValueKey);
    if (boolean_value.has_value()) {
      boolean_capabilities[*name] = *boolean_value;
    }
  }

  // 2. Fill AccountCapabilities fields based on the mapping.
  AccountCapabilities capabilities;
  for (const std::string& name :
       AccountCapabilities::GetSupportedAccountCapabilityNames()) {
    auto it = boolean_capabilities.find(name);
    if (it != boolean_capabilities.end()) {
      capabilities.capabilities_map_[name] = it->second;
    }
  }

  return capabilities;
}
