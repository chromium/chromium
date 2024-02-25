// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/public/cpp/device_image_info.h"

#include <optional>

#include "base/values.h"

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
std::optional<DeviceImageInfo> DeviceImageInfo::FromDictionaryValue(
    const base::Value::Dict& value) {
  const std::string* default_image = value.FindString(kDefaultImageKey);
  if (!default_image) {
    return std::nullopt;
  }

  const std::string* left_bud_image = value.FindString(kLeftBudImageKey);
  if (!left_bud_image) {
    return std::nullopt;
  }

  const std::string* right_bud_image = value.FindString(kRightBudImageKey);
  if (!right_bud_image) {
    return std::nullopt;
  }

  const std::string* case_image = value.FindString(kCaseImageKey);
  if (!case_image) {
    return std::nullopt;
  }

  return DeviceImageInfo(*default_image, *left_bud_image, *right_bud_image,
                         *case_image);
}

base::Value::Dict DeviceImageInfo::ToDictionaryValue() const {
  base::Value::Dict dictionary;
  dictionary.Set(kDefaultImageKey, default_image_);
  dictionary.Set(kLeftBudImageKey, left_bud_image_);
  dictionary.Set(kRightBudImageKey, right_bud_image_);
  dictionary.Set(kCaseImageKey, case_image_);
  return dictionary;
}

}  // namespace ash::bluetooth_config
