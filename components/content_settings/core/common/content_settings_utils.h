// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_UTILS_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_UTILS_H_

#include <memory>

#include "components/content_settings/core/common/content_settings.h"

namespace base {
class Value;
}

namespace content_settings {

// Converts |value| to |ContentSetting|.
ContentSetting ValueToContentSetting(const base::Value& value);

// Returns a base::Value representation of |setting| if |setting| is
// a valid content setting. Otherwise, returns an empty value.
base::Value ContentSettingToValue(ContentSetting setting);

// Adaptor for converting from the new way of base::Value to the old one.
// Like base::Value::ToUniquePtrValue but converts NONE-type values to nullptr.
std::unique_ptr<base::Value> ToNullableUniquePtrValue(base::Value value);
// Adaptor for converting from the old way of base::Value to the new one.
// Like base::Value::FromUniquePtrValue but converts nullptr to NONE-type value.
base::Value FromNullableUniquePtrValue(std::unique_ptr<base::Value> value);

// Whether |primary_pattern| and |secondary_pattern| pair applies to a single
// origin.
bool PatternAppliesToSingleOrigin(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern);

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_UTILS_H_
