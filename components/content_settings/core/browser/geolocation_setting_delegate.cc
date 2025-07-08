
// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/geolocation_setting_delegate.h"

#include <optional>
#include <variant>

#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/permission_settings_info.h"
#include "components/content_settings/core/common/content_settings.h"

namespace content_settings {

bool GeolocationSettingDelegate::IsValid(
    const PermissionSetting& setting) const {
  auto geo_setting = std::get<GeolocationSetting>(setting);

  if (!IsValidPermissionOption(geo_setting.approximate)) {
    return false;
  }

  if (!IsValidPermissionOption(geo_setting.precise)) {
    return false;
  }

  if (IsMorePermissive(geo_setting.precise, geo_setting.approximate)) {
    return false;
  }
  return true;
}

// Returns a setting to inherit to incognito mode. Return nullopt if the setting
// should not be inherited.
std::optional<PermissionSetting> GeolocationSettingDelegate::InheritInIncognito(
    const PermissionSetting& setting) const {
  return std::nullopt;  // TODO(crbug.com/425642101): Implement inheritance
                        // logic.
}

// Parsing and conversion methods.
base::Value GeolocationSettingDelegate::ToValue(
    const PermissionSetting& setting) const {
  const GeolocationSetting& geo_setting = std::get<GeolocationSetting>(setting);
  base::Value::Dict dict;
  dict.Set("approximate", static_cast<int>(geo_setting.approximate));
  dict.Set("precise", static_cast<int>(geo_setting.precise));
  return base::Value(std::move(dict));
}

std::optional<PermissionSetting> GeolocationSettingDelegate::FromValue(
    const base::Value& value) const {
  if (!value.is_dict()) {
    return std::nullopt;
  }
  const auto& dict = value.GetDict();

  auto approximate_optional = dict.FindInt("approximate");
  if (!approximate_optional.has_value()) {
    return std::nullopt;
  }

  auto precise_optional = dict.FindInt("precise");
  if (!precise_optional.has_value()) {
    return std::nullopt;
  }
  GeolocationSetting setting{
      static_cast<PermissionOption>(approximate_optional.value()),
      static_cast<PermissionOption>(precise_optional.value())};

  if (!IsValid(setting)) {
    return std::nullopt;
  }

  return setting;
}

bool GeolocationSettingDelegate::CanBeAutoRevoked(PermissionSetting setting,
                                                  bool is_one_time) const {
  auto* geolocation_setting = std::get_if<GeolocationSetting>(&setting);
  return !is_one_time && geolocation_setting &&
         (*geolocation_setting).approximate == PermissionOption::kAllowed;
}

}  // namespace content_settings
