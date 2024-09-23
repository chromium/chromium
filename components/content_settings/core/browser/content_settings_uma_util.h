// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_UMA_UTIL_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_UMA_UTIL_H_

#include "components/content_settings/core/browser/host_content_settings_map.h"

namespace content_settings_uma_util {

// Converts a given content setting to its histogram value, for use when saving
// content settings types to UKM. For UMA use RecordContentSettingsHistogram.
int ContentSettingTypeToHistogramValue(ContentSettingsType content_setting);

// Records a linear histogram for |content_setting|.
void RecordContentSettingsHistogram(const std::string& name,
                                    ContentSettingsType content_setting);

// Records an active expiry of an expired content setting in UMA.
void RecordActiveExpiryEvent(content_settings::ProviderType provider_type,
                             ContentSettingsType content_setting_type);

}  // namespace content_settings_uma_util

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_UMA_UTIL_H_
