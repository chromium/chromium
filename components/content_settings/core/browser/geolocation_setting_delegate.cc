
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
  DCHECK(std::holds_alternative<GeolocationSetting>(setting)) << setting;
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

bool GeolocationSettingDelegate::IsDefaultSettingValid(
    const PermissionSetting& setting) const {
  DCHECK(std::holds_alternative<GeolocationSetting>(setting)) << setting;
  auto permission_setting = std::get<GeolocationSetting>(setting);
  if (permission_setting.approximate != permission_setting.precise) {
    // The UI only supports default settings with approximate == precise.
    return false;
  }
  return IsValid(permission_setting);
}

// Returns a setting to inherit to incognito mode. Return nullopt if the setting
// should not be inherited.
PermissionSetting GeolocationSettingDelegate::InheritInIncognito(
    const PermissionSetting& setting) const {
  GeolocationSetting geo_setting = std::get<GeolocationSetting>(setting);

  // Only BLOCK should be inherited to incognito
  return GeolocationSetting{geo_setting.approximate == PermissionOption::kDenied
                                ? PermissionOption::kDenied
                                : PermissionOption::kAsk,
                            geo_setting.precise == PermissionOption::kDenied
                                ? PermissionOption::kDenied
                                : PermissionOption::kAsk};
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

bool GeolocationSettingDelegate::IsAnyPermissionAllowed(
    const PermissionSetting& setting) const {
  DCHECK(std::holds_alternative<GeolocationSetting>(setting)) << setting;
  // When precise is allowed, then approximate must be allowed too so we only
  // need to check approximate here.
  return std::get<GeolocationSetting>(setting).approximate ==
         PermissionOption::kAllowed;
}

bool GeolocationSettingDelegate::IsUndecided(
    const PermissionSetting& setting) const {
  DCHECK(std::holds_alternative<GeolocationSetting>(setting)) << setting;
  const auto& geo_setting = std::get<GeolocationSetting>(setting);
  return geo_setting.approximate == PermissionOption::kAsk &&
         geo_setting.precise == PermissionOption::kAsk;
}

bool GeolocationSettingDelegate::CanTrackLastVisit() const {
  return true;
}

bool GeolocationSettingDelegate::ShouldCoalesceEphemeralState() const {
  return true;
}

PermissionSetting GeolocationSettingDelegate::CoalesceEphemeralState(
    const PermissionSetting& persistent_permission_setting,
    const PermissionSetting& ephemeral_permission_setting) const {
  GeolocationSetting persistent_geo_setting =
      std::get<GeolocationSetting>(persistent_permission_setting);
  GeolocationSetting ephemeral_geo_setting =
      std::get<GeolocationSetting>(ephemeral_permission_setting);

  PermissionOption approximate =
      ephemeral_geo_setting.approximate == PermissionOption::kAsk
          ? persistent_geo_setting.approximate
          : ephemeral_geo_setting.approximate;
  PermissionOption precise =
      ephemeral_geo_setting.precise == PermissionOption::kAsk
          ? persistent_geo_setting.precise
          : ephemeral_geo_setting.precise;

  return GeolocationSetting{approximate, precise};
}

PermissionSetting GeolocationSettingDelegate::ApplyPermissionEmbargo(
    const PermissionSetting& setting) const {
  GeolocationSetting geo_setting = std::get<GeolocationSetting>(setting);
  if (geo_setting.approximate == PermissionOption::kAsk) {
    geo_setting.approximate = PermissionOption::kDenied;
  }
  if (geo_setting.precise == PermissionOption::kAsk) {
    geo_setting.precise = PermissionOption::kDenied;
  }
  return geo_setting;
}

}  // namespace content_settings
