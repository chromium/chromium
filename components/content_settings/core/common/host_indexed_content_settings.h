// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_HOST_INDEXED_CONTENT_SETTINGS_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_HOST_INDEXED_CONTENT_SETTINGS_H_

#include "components/content_settings/core/common/content_settings.h"
#include "url/gurl.h"

namespace content_settings {

inline constexpr char kAnyHost[] = "";

// Finds the first (in precedence order) match in the
// `indexed_content_setting`. The returned pointer is valid as long as the
// `indexed_content_setting` exists.
const ContentSettingPatternSource* FindInHostIndexedContentSettings(
    const GURL& primary_url,
    const GURL& secondary_url,
    std::reference_wrapper<const HostIndexedContentSettings>
        indexed_content_setting);

// Finds the first (in precedence order) content setting in `settings`.
const ContentSettingPatternSource* FindContentSetting(
    const GURL& primary_url,
    const GURL& secondary_url,
    std::reference_wrapper<const ContentSettingsForOneType> settings);

// Converts a vector representation to a primary_host-indexed map
// representation.
HostIndexedContentSettings ToHostIndexedMap(
    const ContentSettingsForOneType& settings);

// Compares the output of the previous lookup algorithm on a flat vector with
// the optimized indexed lookup algorithm. Only used within DCHECK calls to
// limit use to debug builds and tests.
bool SettingsLookupsAreConsistent(
    const GURL& primary_url,
    const GURL& secondary_url,
    const ContentSettingsForOneType& linear_settings,
    const HostIndexedContentSettings& indexed_settings);

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_HOST_INDEXED_CONTENT_SETTINGS_H_
