// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/account_info_util.h"

#include <map>
#include <optional>
#include <string>

#include "base/values.h"
#include "components/signin/internal/identity_manager/account_capabilities_constants.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_info.h"

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

std::optional<AccountInfo> AccountInfoFromUserInfo(
    const base::Value::Dict& user_info) {
  // Both |gaia_id| and |email| are required value in the JSON reply, so
  // return empty result if any is missing or is empty.
  const std::string* gaia_id_value = user_info.FindString(kGaiaIdKey);
  if (!gaia_id_value || gaia_id_value->empty())
    return std::nullopt;

  const std::string* email_value = user_info.FindString(kEmailKey);
  if (!email_value || email_value->empty())
    return std::nullopt;

  AccountInfo account_info;
  account_info.email = *email_value;
  account_info.gaia = *gaia_id_value;

  // All other fields are optional, some with default values.
  const std::string* hosted_domain_value =
      user_info.FindString(kHostedDomainKey);
  if (hosted_domain_value && !hosted_domain_value->empty())
    account_info.hosted_domain = *hosted_domain_value;
  else
    account_info.hosted_domain = kNoHostedDomainFound;

  const std::string* full_name_value = user_info.FindString(kFullNameKey);
  if (full_name_value)
    account_info.full_name = *full_name_value;

  const std::string* given_name_value = user_info.FindString(kGivenNameKey);
  if (given_name_value)
    account_info.given_name = *given_name_value;

  const std::string* locale_value = user_info.FindString(kLocaleKey);
  if (locale_value)
    account_info.locale = *locale_value;

  const std::string* picture_url_value = user_info.FindString(kPictureUrlKey);
  if (picture_url_value && !picture_url_value->empty())
    account_info.picture_url = *picture_url_value;
  else
    account_info.picture_url = kNoPictureURLFound;

  return account_info;
}

std::optional<AccountCapabilities> AccountCapabilitiesFromValue(
    const base::Value::Dict& account_capabilities) {
  const base::Value::List* list =
      account_capabilities.FindList(kAccountCapabilitiesListKey);
  if (!list)
    return std::nullopt;

  // 1. Create "capability name" -> "boolean value" mapping.
  std::map<std::string, bool> boolean_capabilities;
  for (const auto& capability_value : *list) {
    const std::string* name =
        capability_value.GetDict().FindString(kAccountCapabilityNameKey);
    if (!name)
      return std::nullopt;  // name is a required field.

    // Check whether a capability has a boolean value.
    std::optional<bool> boolean_value =
        capability_value.GetDict().FindBool(kAccountCapabilityBooleanValueKey);
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
