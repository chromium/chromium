// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/content_settings_utils.h"

#include <memory>
#include <optional>
#include <variant>

#include "base/feature_list.h"
#include "base/values.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/features.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content_settings {

namespace {

// Converts a |Value| to a |ContentSetting|. Returns a result if |value| encodes
// a valid content setting, nullopt otherwise. Note that
// |CONTENT_SETTING_DEFAULT| is encoded as a NULL value, so it is not allowed as
// an integer value.
std::optional<ContentSetting> ParseContentSettingValue(
    const base::Value& value) {
  if (value.is_none()) {
    return CONTENT_SETTING_DEFAULT;
  }
  if (!value.is_int()) {
    return std::nullopt;
  }
  ContentSetting setting = IntToContentSetting(value.GetInt());
  return setting == CONTENT_SETTING_DEFAULT ? std::nullopt
                                            : std::make_optional(setting);
}

std::optional<PermissionSetting> ValueToGeolocationSetting(
    const base::Value& value) {
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

}  // namespace

ContentSetting ValueToContentSetting(const base::Value& value) {
  auto setting = ParseContentSettingValue(value);
  DCHECK(setting.has_value()) << value.DebugString();
  return setting.value_or(CONTENT_SETTING_DEFAULT);
}

base::Value ContentSettingToValue(ContentSetting setting) {
  if (setting <= CONTENT_SETTING_DEFAULT ||
      setting >= CONTENT_SETTING_NUM_SETTINGS) {
    return base::Value();
  }
  return base::Value(setting);
}

std::optional<PermissionSetting> ValueToPermissionSetting(
    ContentSettingsType content_type,
    const base::Value& value) {
  switch (content_type) {
    case ContentSettingsType::GEOLOCATION:
      if (base::FeatureList::IsEnabled(
              content_settings::features::kApproximateGeolocationPermission)) {
        return ValueToGeolocationSetting(value);
      } else {
        return ParseContentSettingValue(value);
      }
    default:
      return ParseContentSettingValue(value);
  }
}

base::Value PermissionSettingToValue(const PermissionSetting& setting) {
  return std::visit(
      absl::Overload{
          [](ContentSetting setting) { return ContentSettingToValue(setting); },
          [](GeolocationSetting setting) {
            base::Value::Dict dict;
            dict.Set("approximate", setting.approximate);
            dict.Set("precise", setting.precise);
            return base::Value(std::move(dict));
          }},
      setting);
}

std::unique_ptr<base::Value> ToNullableUniquePtrValue(base::Value value) {
  if (value.is_none()) {
    return nullptr;
  }
  return base::Value::ToUniquePtrValue(std::move(value));
}

base::Value FromNullableUniquePtrValue(std::unique_ptr<base::Value> value) {
  if (!value) {
    return base::Value();
  }
  return base::Value::FromUniquePtrValue(std::move(value));
}

bool PatternAppliesToSingleOrigin(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern) {
  // Default settings and other patterns apply to multiple origins.
  if (!primary_pattern.MatchesSingleOrigin()) {
    return false;
  }
  // Embedded content settings only match when |url| is embedded in another
  // origin, so ignore non-wildcard secondary patterns.
  if (secondary_pattern != ContentSettingsPattern::Wildcard()) {
    return false;
  }
  return true;
}

}  // namespace content_settings
