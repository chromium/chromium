// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_INFO_SERIALIZER_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_INFO_SERIALIZER_H_

#include <optional>

#include "base/values.h"

struct AccountInfo;
class AccountCapabilities;
struct CoreAccountId;

namespace signin {

// Key for the `account_id` in the serialized `AccountInfo` dictionary.
extern const char kAccountIdKey[];
// Key for the `last_downloaded_image_url_with_size` in the serialized
// `AccountInfo` dictionary.
extern const char kLastDownloadedImageURLWithSizeKey[];

// Utility class to serialize and deserialize AccountInfo from/to base::Value.
class AccountInfoSerializer {
 public:
  AccountInfoSerializer() = delete;

  // Serializes an `AccountInfo` object to a `base::Value::Dict`.
  static base::Value::Dict ToValue(const AccountInfo& account_info);

  // Deserializes an `AccountInfo` from a `base::Value::Dict`.
  // Returns `std::nullopt` if the dictionary is not valid.
  static std::optional<AccountInfo> FromValue(const base::Value::Dict& dict);

 private:
  // Deserializes `account_info` from `dict`. Does not read account ID from
  // `dict` but sets `account_id` instead.
  static std::optional<AccountInfo> DeserializeAccountInfoFromDict(
      const base::Value::Dict& dict,
      const CoreAccountId& account_id);

  // Serializes `account_info` to `dict`. Does not write `account_id`.
  static void SerializeAccountInfoToDict(base::Value::Dict& dict,
                                         const AccountInfo& account_info);

  // Deserializes capabilities from `dict`.
  static AccountCapabilities LoadAccountCapabilities(
      const base::Value::Dict& dict);
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_INFO_SERIALIZER_H_
