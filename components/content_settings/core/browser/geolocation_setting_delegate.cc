
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
namespace {
bool IsValidOption(ContentSetting setting) {
  return setting == CONTENT_SETTING_ASK || setting == CONTENT_SETTING_ALLOW ||
         setting == CONTENT_SETTING_BLOCK;
}
}  // namespace

bool GeolocationSettingDelegate::IsValid(
    const PermissionSetting& setting) const {
  auto* geo_setting = std::get_if<GeolocationSetting>(&setting);
  if (!geo_setting) {
    return false;
  }
  if (!IsValidOption(geo_setting->approximate)) {
    return false;
  }
  if (!IsValidOption(geo_setting->precise)) {
    return false;
  }
  if (IsMorePermissive(geo_setting->precise, geo_setting->approximate)) {
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
  dict.Set("approximate", geo_setting.approximate);
  dict.Set("precise", geo_setting.precise);
  return base::Value(std::move(dict));
}

std::optional<PermissionSetting> GeolocationSettingDelegate::FromValue(
    const base::Value& value) const {
  if (!value.is_dict()) {
    return std::nullopt;
  }
  const auto& dict = value.GetDict();
  ContentSetting approximate = IntToContentSetting(
      dict.FindInt("approximate").value_or(CONTENT_SETTING_DEFAULT));
  if (approximate == CONTENT_SETTING_DEFAULT) {
    return std::nullopt;
  }
  auto precise = IntToContentSetting(
      dict.FindInt("precise").value_or(CONTENT_SETTING_DEFAULT));
  if (precise == CONTENT_SETTING_DEFAULT) {
    return std::nullopt;
  }
  return GeolocationSetting{approximate, precise};
}

}  // namespace content_settings
