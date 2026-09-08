// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines utility functions to serialize and deserialize
// `AccountInfo` and `AccountCapabilities` from/to `base::Value`.
//
// These functions serve two distinct purposes:
// - Deserializing objects received from server responses.
// - Serializing / deserializing objects for permanent storage.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_INFO_UTIL_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_INFO_UTIL_H_

#include <optional>
#include <string_view>

#include "base/values.h"

class AccountCapabilities;
class AccountInfo;

namespace signin {

// Key for the `account_id` in the serialized `AccountInfo` dictionary.
inline constexpr std::string_view kAccountIdKey = "account_id";
// Key for the `last_downloaded_image_url_with_size` in the serialized
// `AccountInfo` dictionary.
inline constexpr std::string_view kLastDownloadedImageURLWithSizeKey =
    "last_downloaded_image_url_with_size";
// Key for the `accountcapability_overrides` in the serialized `AccountInfo`
// dictionary.
inline constexpr std::string_view kAccountCapabilityOverridesKey =
    "accountcapability_overrides";

// Builds an `AccountInfo` from the JSON data returned by the Gaia servers, if
// possible.
std::optional<AccountInfo> AccountInfoFromUserInfo(
    const base::DictValue& user_info);

// Builds an `AccountCapabilities` from the JSON data returned by the Gaia
// servers, if possible.
std::optional<AccountCapabilities> AccountCapabilitiesFromServerResponse(
    const base::DictValue& account_capabilities);

// Serializes an `AccountCapabilities` object to a `base::DictValue`.
base::DictValue SerializeAccountCapabilities(
    const AccountCapabilities& account_capabilities);

// Deserializes an `AccountCapabilities` from a `base::DictValue` previously
// created by `SerializeAccountCapabilities()`.
AccountCapabilities DeserializeAccountCapabilities(
    const base::DictValue& capabilities_dict,
    const base::DictValue& overrides_dict);

// Serializes an `AccountCapabilities` overrides to a `base::DictValue`.
base::DictValue SerializeAccountCapabilityOverrides(
    const AccountCapabilities& account_capabilities);

// Serializes an `AccountInfo` object to a `base::DictValue`.
base::DictValue SerializeAccountInfo(const AccountInfo& account_info);

// Deserializes an `AccountInfo` from a `base::DictValue` previously created
// by `SerializeAccountInfo()`.
// Returns `std::nullopt` if the dictionary is not valid.
std::optional<AccountInfo> DeserializeAccountInfo(const base::DictValue& dict);

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_INFO_UTIL_H_
