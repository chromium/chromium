// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_HOST_INDEXED_CONTENT_SETTINGS_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_HOST_INDEXED_CONTENT_SETTINGS_H_

#include <map>

#include "components/content_settings/core/common/content_settings.h"
#include "url/gurl.h"

namespace content_settings {

typedef std::map<std::string, ContentSettingsForOneType>
    HostIndexedContentSettings;

// Finds the first (in precedence order) match in the
// `indexed_content_setting`.
absl::optional<ContentSetting> FindInHostIndexedContentSettings(
    const GURL& primary_url,
    const GURL& secondary_url,
    const HostIndexedContentSettings& indexed_content_setting);

// Finds the first (in precedence order) content setting in `settings`.
absl::optional<ContentSetting> FindContentSetting(
    const GURL& primary_url,
    const GURL& secondary_url,
    const ContentSettingsForOneType& settings);

// Converts a vector representation to a primary_host-indexed map
// representation.
HostIndexedContentSettings ToHostIndexedMap(
    const ContentSettingsForOneType& settings);

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_HOST_INDEXED_CONTENT_SETTINGS_H_
