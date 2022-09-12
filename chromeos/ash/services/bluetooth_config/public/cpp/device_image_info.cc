// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/public/cpp/device_image_info.h"

#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::bluetooth_config {

namespace {

constexpr char kDefaultImageKey[] = "Default";
constexpr char kLeftBudImageKey[] = "LeftBud";
constexpr char kRightBudImageKey[] = "RightBud";
constexpr char kCaseImageKey[] = "Case";

}  // namespace

DeviceImageInfo::DeviceImageInfo(const std::string& default_image,
                                 const std::string& left_bud_image,
                                 const std::string& right_bud_image,
                                 const std::string& case_image)
    : default_image_(default_image),
      left_bud_image_(left_bud_image),
      right_bud_image_(right_bud_image),
      case_image_(case_image) {}

DeviceImageInfo::DeviceImageInfo() = default;

DeviceImageInfo::DeviceImageInfo(const DeviceImageInfo&) = default;

DeviceImageInfo& DeviceImageInfo::operator=(const DeviceImageInfo&) = default;

DeviceImageInfo::~DeviceImageInfo() = default;

// static
absl::optional<DeviceImageInfo> DeviceImageInfo::FromDictionaryValue(
    const base::Value& value) {
  if (!value.is_dict())
    return absl::nullopt;

  const std::string* default_image = value.FindStringKey(kDefaultImageKey);
  if (!default_image)
    return absl::nullopt;

  const std::string* left_bud_image = value.FindStringKey(kLeftBudImageKey);
  if (!left_bud_image)
    return absl::nullopt;

  const std::string* right_bud_image = value.FindStringKey(kRightBudImageKey);
  if (!right_bud_image)
    return absl::nullopt;

  const std::string* case_image = value.FindStringKey(kCaseImageKey);
  if (!case_image)
    return absl::nullopt;

  return DeviceImageInfo(*default_image, *left_bud_image, *right_bud_image,
                         *case_image);
}

base::Value DeviceImageInfo::ToDictionaryValue() const {
  base::Value dictionary(base::Value::Type::DICTIONARY);
  dictionary.SetKey(kDefaultImageKey, base::Value(default_image_));
  dictionary.SetKey(kLeftBudImageKey, base::Value(left_bud_image_));
  dictionary.SetKey(kRightBudImageKey, base::Value(right_bud_image_));
  dictionary.SetKey(kCaseImageKey, base::Value(case_image_));
  return dictionary;
}

}  // namespace ash::bluetooth_config
