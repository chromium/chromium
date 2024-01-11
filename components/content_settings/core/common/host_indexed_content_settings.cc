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

const ContentSettingPatternSource* FindInHostToContentSettings(
    const GURL& primary_url,
    const GURL& secondary_url,
    std::reference_wrapper<const HostToContentSettings> indexed_content_setting,
    const std::string& host) {
  // The value for a pattern without a host in the indexed settings is an
  // empty string.
  if (!host.empty()) {
    const ContentSettingPatternSource* content_setting = nullptr;

    if (primary_url.HostIsIPAddress()) {
      auto it = indexed_content_setting.get().find(host);
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
        auto it = indexed_content_setting.get().find(
            host.substr(cur_pos, std::string::npos));
        if (it != indexed_content_setting.get().end()) {
          content_setting =
              FindContentSetting(primary_url, secondary_url, it->second);
          if (content_setting) {
            return content_setting;
          }
        }
        size_t found = host.find(".", cur_pos);
        cur_pos = found != std::string::npos ? found + 1 : std::string::npos;
      }
    }
  }
  auto it = indexed_content_setting.get().find(kAnyHost);
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

HostIndexedContentSettings::HostIndexedContentSettings() {
  HostIndexedContentSettings(ContentSettingsForOneType{});
}

HostIndexedContentSettings::HostIndexedContentSettings(
    const ContentSettingsForOneType& settings) {
  for (const ContentSettingPatternSource& setting : settings) {
    Add(setting);
  }
}

HostIndexedContentSettings::~HostIndexedContentSettings() = default;

const ContentSettingPatternSource* HostIndexedContentSettings::Find(
    const GURL& primary_url,
    const GURL& secondary_url) const {
  const ContentSettingPatternSource* found = FindInHostToContentSettings(
      primary_url, secondary_url, primary_host_indexed_, primary_url.host());
  if (found) {
    return found;
  }
  found = FindInHostToContentSettings(primary_url, secondary_url,
                                      secondary_host_indexed_,
                                      secondary_url.host());
  if (found) {
    return found;
  }
  return FindContentSetting(primary_url, secondary_url, wildcard_settings_);
}

void HostIndexedContentSettings::Add(
    const ContentSettingPatternSource& pattern_source) {
  const std::string& primary_host = pattern_source.primary_pattern.GetHost();
  if (!primary_host.empty()) {
    primary_host_indexed_[primary_host].push_back(std::move(pattern_source));
    return;
  }
  const std::string& secondary_host =
      pattern_source.secondary_pattern.GetHost();
  if (!secondary_host.empty()) {
    secondary_host_indexed_[secondary_host].push_back(
        std::move(pattern_source));
    return;
  }
  wildcard_settings_.push_back(std::move(pattern_source));
}

void HostIndexedContentSettings::Clear() {
  primary_host_indexed_.clear();
  secondary_host_indexed_.clear();
  wildcard_settings_.clear();
}

#if DCHECK_IS_ON()
bool HostIndexedContentSettings::IsSameResultAsLinearLookup(
    const GURL& primary_url,
    const GURL& secondary_url,
    const ContentSettingsForOneType& linear_settings) const {
  const ContentSettingPatternSource* found_content_setting =
      FindContentSetting(primary_url, secondary_url, linear_settings);
  const ContentSettingPatternSource* found_indexed_content_setting =
      Find(primary_url, secondary_url);

  if (!found_content_setting || !found_indexed_content_setting) {
    return !found_content_setting && !found_indexed_content_setting;
  }
  return found_content_setting->GetContentSetting() ==
         found_indexed_content_setting->GetContentSetting();
}
#endif  // DCHECK_IS_ON()

}  // namespace content_settings
