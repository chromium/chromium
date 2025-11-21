// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/account_info_util.h"

#include <map>
#include <optional>
#include <string>

#include "base/containers/contains.h"
#include "base/values.h"
#include "components/signin/internal/identity_manager/account_capabilities_constants.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/signin_constants.h"

using signin::constants::kNoHostedDomainFound;

namespace {
// Keys used to store the different values in the JSON dictionary received
// from gaia server.
const char kGaiaIdKey[] = "id";
const char kSubKey[] = "sub";
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
  if (!gaia_id_value || gaia_id_value->empty()) {
    // The GAIA ID may be returned via a different property, based on which
    // endpoint the browser is communicating with.
    gaia_id_value = user_info.FindString(kSubKey);
    if (!gaia_id_value || gaia_id_value->empty()) {
      return std::nullopt;
    }
  }

  const std::string* email_value = user_info.FindString(kEmailKey);
  if (!email_value || email_value->empty()) {
    return std::nullopt;
  }

  AccountInfo::Builder builder(GaiaId(*gaia_id_value), *email_value);

  // The following fields remain "unknown" in `AccountInfo` if they are not set
  // or empty in the `user_info`.
  const std::string* full_name_value = user_info.FindString(kFullNameKey);
  if (full_name_value && !full_name_value->empty()) {
    builder.SetFullName(*full_name_value);
  }

  const std::string* given_name_value = user_info.FindString(kGivenNameKey);
  if (given_name_value && !given_name_value->empty()) {
    builder.SetGivenName(*given_name_value);
  }

  const std::string* locale_value = user_info.FindString(kLocaleKey);
  if (locale_value && !locale_value->empty()) {
    builder.SetLocale(*locale_value);
  }

  // The following fields will be set as "empty" in `AccountInfo` if they are
  // not set or empty in the `user_info`.
  const std::string* hosted_domain_value =
      user_info.FindString(kHostedDomainKey);
  builder.SetHostedDomain(hosted_domain_value ? *hosted_domain_value
                                              : std::string());

  const std::string* picture_url_value = user_info.FindString(kPictureUrlKey);
  builder.SetAvatarUrl(picture_url_value ? *picture_url_value : std::string());

  return builder.Build();
}

std::optional<AccountCapabilities> AccountCapabilitiesFromValue(
    const base::Value::Dict& account_capabilities) {
  const base::Value::List* list =
      account_capabilities.FindList(kAccountCapabilitiesListKey);
  if (!list) {
    return std::nullopt;
  }

  base::flat_map<std::string, bool> capabilities_map;
  for (const auto& capability_value : *list) {
    const std::string* name =
        capability_value.GetDict().FindString(kAccountCapabilityNameKey);
    if (!name) {
      return std::nullopt;  // name is a required field.
    }

    // Check whether a capability has a boolean value.
    std::optional<bool> boolean_value =
        capability_value.GetDict().FindBool(kAccountCapabilityBooleanValueKey);
    if (!boolean_value) {
      continue;
    }

    // Add the capability to the map if it's supported in Chrome.
    if (base::Contains(AccountCapabilities::GetSupportedAccountCapabilityNames(),
                       *name)) {
      capabilities_map.insert({*name, *boolean_value});
    }
  }

  return AccountCapabilities(std::move(capabilities_map));
}
