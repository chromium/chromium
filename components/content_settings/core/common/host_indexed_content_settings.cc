// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/host_indexed_content_settings.h"

#include <map>
#include <string>

#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/cookie_settings_base.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content_settings {

const ContentSettingPatternSource* FindInHostIndexedContentSettings(
    const GURL& primary_url,
    const GURL& secondary_url,
    std::reference_wrapper<const HostIndexedContentSettings>
      indexed_content_setting) {
  HostIndexedContentSettings::const_iterator it;
  // The value for a pattern without a host in the indexed settings is an
  // empty string.
  if (primary_url.has_host()) {
    const std::string& primary_host = primary_url.host();
    const ContentSettingPatternSource* content_setting = nullptr;

    if (primary_url.HostIsIPAddress()) {
      it = indexed_content_setting.get().find(primary_host);
      if (it != indexed_content_setting.get().end()) {
        content_setting =
            FindContentSetting(primary_url, secondary_url, it->second);
        if (content_setting) {
          return content_setting;
        }
      }
    } else {
      size_t cur_pos = 0;
      while (cur_pos != std::string::npos) {
        it = indexed_content_setting.get().find(
            primary_host.substr(cur_pos, std::string::npos));
        if (it != indexed_content_setting.get().end()) {
          content_setting =
              FindContentSetting(primary_url, secondary_url, it->second);
          if (content_setting) {
            return content_setting;
          }
        }
        size_t found = primary_host.find(".", cur_pos);
        cur_pos = found != std::string::npos ? found + 1 : std::string::npos;
      }
    }
  }
  it = indexed_content_setting.get().find(kAnyHost);
  if (it != indexed_content_setting.get().end()) {
    return FindContentSetting(primary_url, secondary_url, it->second);
  }
  return nullptr;
}

const ContentSettingPatternSource* FindContentSetting(
    const GURL& primary_url,
    const GURL& secondary_url,
    std::reference_wrapper<const ContentSettingsForOneType> settings) {
  const auto& entry = base::ranges::find_if(
      settings.get(), [&](const ContentSettingPatternSource& entry) {
        return !entry.IsExpired() &&
               entry.primary_pattern.Matches(primary_url) &&
               entry.secondary_pattern.Matches(secondary_url);
      });
  return entry == settings.get().end() ? nullptr : &*entry;
}

HostIndexedContentSettings ToHostIndexedMap(
    const ContentSettingsForOneType& settings) {
  HostIndexedContentSettings indexed_settings;
  for (ContentSettingPatternSource setting : settings) {
    // TODO(b/314939684): Index on secondary_pattern as well.
    std::string primary_host = setting.primary_pattern.GetHost();
    indexed_settings[primary_host].push_back(setting);
  }
  return indexed_settings;
}

bool SettingsLookupsAreConsistent(
  const GURL& primary_url,
  const GURL& secondary_url,
  const ContentSettingsForOneType& linear_settings,
  const HostIndexedContentSettings& indexed_settings) {
  const ContentSettingPatternSource* found_content_setting =
        FindContentSetting(primary_url, secondary_url,
                           linear_settings);
  const ContentSettingPatternSource* found_indexed_content_setting =
        FindInHostIndexedContentSettings(
            primary_url, secondary_url, indexed_settings);
  if (!found_content_setting || !found_indexed_content_setting) {
    return !found_content_setting && !found_indexed_content_setting;
  }
  return found_content_setting->GetContentSetting()
                == found_indexed_content_setting->GetContentSetting();

}

}  // namespace content_settings
