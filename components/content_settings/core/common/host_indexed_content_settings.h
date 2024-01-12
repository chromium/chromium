// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_HOST_INDEXED_CONTENT_SETTINGS_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_HOST_INDEXED_CONTENT_SETTINGS_H_

#include <string>

#include "components/content_settings/core/common/content_settings.h"
#include "url/gurl.h"

namespace content_settings {

inline constexpr char kAnyHost[] = "";
typedef std::map<std::string, ContentSettingsForOneType> HostToContentSettings;

// Class with maps indexed by a setting's host. If primary_pattern host is a
// wildcard, index by secondary host. Patterns with wildcard host for primary
// and secondary are handled separately.
class HostIndexedContentSettings {
 public:
  HostIndexedContentSettings();
  explicit HostIndexedContentSettings(
      const ContentSettingsForOneType& settings);

  ~HostIndexedContentSettings();
  HostIndexedContentSettings(const HostIndexedContentSettings& other) = delete;

  // Finds the ContentSettingPatternSource that matches both the primary and
  // secondary urls.
  const ContentSettingPatternSource* Find(const GURL& primary_url,
                                          const GURL& secondary_url) const;

  // Add the `pattern_source` to the HostIndexedContentSettings object.
  void Add(const ContentSettingPatternSource& pattern_source);

  // Clears the object information.
  void Clear();

  // Compares the output of the previous lookup algorithm on a flat vector with
  // the optimized indexed lookup algorithm. Only used within DCHECK calls to
  // limit use to debug builds and tests.
#if DCHECK_IS_ON()
  bool IsSameResultAsLinearLookup(
      const GURL& primary_url,
      const GURL& secondary_url,
      const ContentSettingsForOneType& linear_settings) const;
#endif  // DCHECK_IS_ON()

 private:
  HostToContentSettings primary_host_indexed_;
  HostToContentSettings secondary_host_indexed_;
  ContentSettingsForOneType wildcard_settings_;
};

// Finds the first (in precedence order) content setting in `settings`.
const ContentSettingPatternSource* FindContentSetting(
    const GURL& primary_url,
    const GURL& secondary_url,
    std::reference_wrapper<const ContentSettingsForOneType> settings);

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_HOST_INDEXED_CONTENT_SETTINGS_H_
