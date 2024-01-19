// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/host_indexed_content_settings.h"

#include <map>
#include <memory>
#include <string>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_metadata.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_rules.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/cookie_settings_base.h"
#include "components/content_settings/core/common/features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content_settings {
namespace {
inline constexpr char kAnyHost[] = "";

bool InsertValue(Rules& rules,
                 const ContentSettingsPattern& primary_pattern,
                 const ContentSettingsPattern& secondary_pattern,
                 base::Value value,
                 const RuleMetaData& metadata) {
  ValueEntry& entry = rules[{primary_pattern, secondary_pattern}];
  if (entry.value == value && entry.metadata == metadata) {
    return false;
  }
  entry.value = std::move(value);
  entry.metadata = metadata;
  return true;
}

bool EraseValue(Rules& rules,
                const ContentSettingsPattern& primary_pattern,
                const ContentSettingsPattern& secondary_pattern) {
  return rules.erase({primary_pattern, secondary_pattern}) > 0;
}

bool EraseValue(HostIndexedContentSettings::HostToContentSettings& index,
                const std::string& key,
                const ContentSettingsPattern& primary_pattern,
                const ContentSettingsPattern& secondary_pattern) {
  auto it = index.find(key);
  if (it == index.end()) {
    return false;
  }
  bool result = EraseValue(it->second, primary_pattern, secondary_pattern);
  if (it->second.empty()) {
    index.erase(it);
  }
  return result;
}

const RuleEntry* FindContentSetting(const GURL& primary_url,
                                    const GURL& secondary_url,
                                    const Rules& settings) {
  const auto it = base::ranges::find_if(settings, [&](const auto& entry) {
    return entry.first.primary_pattern.Matches(primary_url) &&
           entry.first.secondary_pattern.Matches(secondary_url) &&
           (base::FeatureList::IsEnabled(
                content_settings::features::kActiveContentSettingExpiry) ||
            !entry.second.metadata.IsExpired());
  });
  return it == settings.end() ? nullptr : &*it;
}

}  // namespace

const RuleEntry* FindInHostToContentSettings(
    const GURL& primary_url,
    const GURL& secondary_url,
    std::reference_wrapper<
        const HostIndexedContentSettings::HostToContentSettings>
        indexed_content_setting,
    const std::string& host) {
  // The value for a pattern without a host in the indexed settings is an
  // empty string.
  if (!host.empty()) {
    if (primary_url.HostIsIPAddress()) {
      auto it = indexed_content_setting.get().find(host);
      if (it != indexed_content_setting.get().end()) {
        auto* result =
            FindContentSetting(primary_url, secondary_url, it->second);
        if (result) {
          return result;
        }
      }
    } else {
      size_t cur_pos = 0;
      while (cur_pos != std::string::npos) {
        auto it = indexed_content_setting.get().find(
            host.substr(cur_pos, std::string::npos));
        if (it != indexed_content_setting.get().end()) {
          auto* result =
              FindContentSetting(primary_url, secondary_url, it->second);
          if (result) {
            return result;
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
        return entry.primary_pattern.Matches(primary_url) &&
               entry.secondary_pattern.Matches(secondary_url) &&
               (base::FeatureList::IsEnabled(
                    content_settings::features::kActiveContentSettingExpiry) ||
                !entry.IsExpired());
      });
  return entry == settings.get().end() ? nullptr : &*entry;
}

HostIndexedContentSettings::Iterator::Iterator(
    const HostIndexedContentSettings& index,
    bool begin)
    : index_(index) {
  index_->iterating_++;
  if (begin) {
    SetStage(Stage::kPrimaryHost);
  } else {
    SetStage(Stage::kWildcard);
    current_iterator_ = current_end_;
  }
}

HostIndexedContentSettings::Iterator::~Iterator() {
  DCHECK_GT(index_->iterating_, 0);
  index_->iterating_--;
}

HostIndexedContentSettings::Iterator&
HostIndexedContentSettings::Iterator::operator++() {
  ++current_iterator_;
  if (current_iterator_ == current_end_) {
    // If we reach the end of a host bucket, continue with the next host bucket
    // if available.
    if (next_map_iterator_ != next_map_end_) {
      current_iterator_ = next_map_iterator_->second.begin();
      current_end_ = next_map_iterator_->second.end();
      ++next_map_iterator_;
      return *this;
    }
    // Otherwise continue iterating over the next index structure.
    switch (stage_) {
      case Stage::kPrimaryHost:
        SetStage(Stage::kSecondaryHost);
        break;
      case Stage::kSecondaryHost:
        SetStage(Stage::kWildcard);
        break;
      case Stage::kWildcard:
        // We have reached the end.
        break;
      case Stage::kInvalid:
        NOTREACHED();
    }
  }
  return *this;
}

void HostIndexedContentSettings::Iterator::SetStage(Stage stage) {
  auto set_map = [this](const HostToContentSettings& map) {
    next_map_iterator_ = map.begin();
    next_map_end_ = map.end();
    current_iterator_ = next_map_iterator_->second.begin();
    current_end_ = next_map_iterator_->second.end();
    ++next_map_iterator_;
  };

  switch (stage) {
    case Stage::kPrimaryHost:
      if (!index_->primary_host_indexed_.empty()) {
        stage_ = Stage::kPrimaryHost;
        set_map(index_->primary_host_indexed_);
        break;
      }
      // Fall through to the next index structure if the requested one is empty.
      ABSL_FALLTHROUGH_INTENDED;
    case Stage::kSecondaryHost:
      if (!index_->secondary_host_indexed_.empty()) {
        stage_ = Stage::kSecondaryHost;
        set_map(index_->secondary_host_indexed_);
        break;
      }
      // Fall through to the next index structure if the requested one is empty.
      ABSL_FALLTHROUGH_INTENDED;
    case Stage::kWildcard:
      stage_ = Stage::kWildcard;
      next_map_iterator_ = {};
      next_map_end_ = {};
      current_iterator_ = index_->wildcard_settings_.begin();
      current_end_ = index_->wildcard_settings_.end();
      break;
    case Stage::kInvalid:
      NOTREACHED();
  }
}

HostIndexedContentSettings::HostIndexedContentSettings() = default;

HostIndexedContentSettings::HostIndexedContentSettings(
    HostIndexedContentSettings&& other) = default;
HostIndexedContentSettings& HostIndexedContentSettings::operator=(
    HostIndexedContentSettings&&) = default;

HostIndexedContentSettings::HostIndexedContentSettings(
    const ContentSettingsForOneType& settings) {
  for (const ContentSettingPatternSource& setting : settings) {
    SetValue(setting.primary_pattern, setting.secondary_pattern,
             setting.setting_value.Clone(), setting.metadata);
  }
}

HostIndexedContentSettings::~HostIndexedContentSettings() = default;

HostIndexedContentSettings::Iterator HostIndexedContentSettings::begin() const {
  return Iterator(*this, true);
}
HostIndexedContentSettings::Iterator HostIndexedContentSettings::end() const {
  return Iterator(*this, false);
}

const RuleEntry* HostIndexedContentSettings::Find(
    const GURL& primary_url,
    const GURL& secondary_url) const {
  const RuleEntry* found = FindInHostToContentSettings(
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

bool HostIndexedContentSettings::SetValue(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    base::Value value,
    const RuleMetaData& metadata) {
  DCHECK_EQ(iterating_, 0);
  const std::string& primary_host = primary_pattern.GetHost();
  if (!primary_host.empty()) {
    return InsertValue(primary_host_indexed_[primary_host], primary_pattern,
                       secondary_pattern, std::move(value), metadata);
  }
  const std::string& secondary_host = secondary_pattern.GetHost();
  if (!secondary_host.empty()) {
    return InsertValue(secondary_host_indexed_[secondary_host], primary_pattern,
                       secondary_pattern, std::move(value), metadata);
  }
  return InsertValue(wildcard_settings_, primary_pattern, secondary_pattern,
                     std::move(value), metadata);
}

bool HostIndexedContentSettings::DeleteValue(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern) {
  const std::string& primary_host = primary_pattern.GetHost();
  DCHECK_EQ(iterating_, 0);
  if (!primary_host.empty()) {
    return EraseValue(primary_host_indexed_, primary_host, primary_pattern,
                      secondary_pattern);
  }

  const std::string& secondary_host = secondary_pattern.GetHost();
  if (!secondary_host.empty()) {
    return EraseValue(secondary_host_indexed_, secondary_host, primary_pattern,
                      secondary_pattern);
  }

  return EraseValue(wildcard_settings_, primary_pattern, secondary_pattern);
}

void HostIndexedContentSettings::Clear() {
  DCHECK_EQ(iterating_, 0);
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
  const RuleEntry* found_indexed_content_setting =
      Find(primary_url, secondary_url);

  if (!found_content_setting || !found_indexed_content_setting) {
    return !found_content_setting && !found_indexed_content_setting;
  }
  return found_content_setting->GetContentSetting() ==
         ValueToContentSetting(found_indexed_content_setting->second.value);
}
#endif  // DCHECK_IS_ON()

}  // namespace content_settings
