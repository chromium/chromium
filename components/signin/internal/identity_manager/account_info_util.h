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

struct AccountInfo;
class AccountCapabilities;

namespace signin {

// Key for the `account_id` in the serialized `AccountInfo` dictionary.
inline constexpr std::string_view kAccountIdKey = "account_id";
// Key for the `last_downloaded_image_url_with_size` in the serialized
// `AccountInfo` dictionary.
inline constexpr std::string_view kLastDownloadedImageURLWithSizeKey =
    "last_downloaded_image_url_with_size";

// Builds an `AccountInfo` from the JSON data returned by the Gaia servers, if
// possible.
std::optional<AccountInfo> AccountInfoFromUserInfo(
    const base::Value::Dict& user_info);

// Builds an `AccountCapabilities` from the JSON data returned by the Gaia
// servers, if possible.
std::optional<AccountCapabilities> AccountCapabilitiesFromServerResponse(
    const base::Value::Dict& account_capabilities);

// Serializes an `AccountCapabilities` object to a `base::Value::Dict`.
base::Value::Dict SerializeAccountCapabilities(
    const AccountCapabilities& account_capabilities);

// Deserializes an `AccountCapabilities` from a `base::Value::Dict` previously
// created by `SerializeAccountCapabilities()`.
AccountCapabilities DeserializeAccountCapabilities(
    const base::Value::Dict& dict);

// Serializes an `AccountInfo` object to a `base::Value::Dict`.
base::Value::Dict SerializeAccountInfo(const AccountInfo& account_info);

// Deserializes an `AccountInfo` from a `base::Value::Dict` previously created
// by `SerializeAccountInfo()`.
// Returns `std::nullopt` if the dictionary is not valid.
std::optional<AccountInfo> DeserializeAccountInfo(
    const base::Value::Dict& dict);

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_INFO_UTIL_H_
