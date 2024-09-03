// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/host_indexed_content_settings.h"

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_metadata.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_rules.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/cookie_settings_base.h"
#include "components/content_settings/core/common/features.h"

namespace content_settings {
namespace {
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
                                    const Rules& settings,
                                    const base::Clock* clock) {
  const auto it = base::ranges::find_if(settings, [&](const auto& entry) {
    return entry.first.primary_pattern.Matches(primary_url) &&
           entry.first.secondary_pattern.Matches(secondary_url) &&
           (base::FeatureList::IsEnabled(
                content_settings::features::kActiveContentSettingExpiry) ||
            !entry.second.metadata.IsExpired(clock));
  });
  return it == settings.end() ? nullptr : &*it;
}

const RuleEntry* FindInHostToContentSettings(
    const GURL& primary_url,
    const GURL& secondary_url,
    const HostIndexedContentSettings::HostToContentSettings&
        indexed_content_setting,
    std::string_view host,
    const base::Clock* clock) {
  if (host.empty() || indexed_content_setting.empty()) {
    return nullptr;
  }

  // Trim ending dot in host.
  if (host.back() == '.') {
    host.remove_suffix(1);
  }
  if (primary_url.HostIsIPAddress()) {
    auto it = indexed_content_setting.find(host);
    if (it != indexed_content_setting.end()) {
      auto* result =
          FindContentSetting(primary_url, secondary_url, it->second, clock);
      if (result) {
        return result;
      }
    }
  } else {
    std::string_view subdomain(host);
    while (!subdomain.empty()) {
      auto it = indexed_content_setting.find(subdomain);
      if (it != indexed_content_setting.end()) {
        auto* result =
            FindContentSetting(primary_url, secondary_url, it->second, clock);
        if (result) {
          return result;
        }
      }
      size_t found = subdomain.find(".");
      subdomain = found != std::string::npos ? subdomain.substr(found + 1)
                                             : std::string_view();
    }
  }
  return nullptr;
}

}  // namespace

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

HostIndexedContentSettings::Iterator::Iterator(const Iterator& other)
    : index_(other.index_),
      stage_(other.stage_),
      next_map_iterator_(other.next_map_iterator_),
      next_map_end_(other.next_map_end_),
      current_iterator_(other.current_iterator_),
      current_end_(other.current_end_) {
  index_->iterating_++;
}

HostIndexedContentSettings::Iterator::Iterator(Iterator&& other)
    : index_(other.index_),
      stage_(other.stage_),
      next_map_iterator_(other.next_map_iterator_),
      next_map_end_(other.next_map_end_),
      current_iterator_(other.current_iterator_),
      current_end_(other.current_end_) {
  index_->iterating_++;
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
        NOTREACHED_IN_MIGRATION();
    }
  }
  return *this;
}

HostIndexedContentSettings::Iterator
HostIndexedContentSettings::Iterator::operator++(int) {
  Iterator ret = *this;
  operator++();
  return ret;
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
      NOTREACHED_IN_MIGRATION();
  }
}

HostIndexedContentSettings::HostIndexedContentSettings()
    : HostIndexedContentSettings(base::DefaultClock::GetInstance()) {}

HostIndexedContentSettings::HostIndexedContentSettings(const base::Clock* clock)
    : clock_(clock) {
  DCHECK(clock);
}

HostIndexedContentSettings::HostIndexedContentSettings(ProviderType source,
                                                       bool off_the_record)
    : source_(source),
      off_the_record_(off_the_record),
      clock_(base::DefaultClock::GetInstance()) {}

HostIndexedContentSettings::HostIndexedContentSettings(
    HostIndexedContentSettings&& other) = default;
HostIndexedContentSettings& HostIndexedContentSettings::operator=(
    HostIndexedContentSettings&&) = default;

// static
std::vector<HostIndexedContentSettings> HostIndexedContentSettings::Create(
    const ContentSettingsForOneType& settings) {
  std::vector<HostIndexedContentSettings> indices;
  if (settings.empty()) {
    return indices;
  }
  for (const auto& entry : settings) {
    // Indices need to be split by content settings provider to ensure
    // accurate precedence of settings.
    if (indices.empty() || entry.source != indices.back().source_ ||
        entry.incognito != indices.back().off_the_record()) {
      indices.emplace_back(entry.source, entry.incognito);
    }
    indices.back().SetValue(entry.primary_pattern, entry.secondary_pattern,
                            entry.setting_value.Clone(), entry.metadata);
  }
  return indices;
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
      primary_url, secondary_url, primary_host_indexed_,
      primary_url.host_piece(), clock_);
  if (found) {
    return found;
  }
  found = FindInHostToContentSettings(primary_url, secondary_url,
                                      secondary_host_indexed_,
                                      secondary_url.host_piece(), clock_);
  if (found) {
    return found;
  }
  return FindContentSetting(primary_url, secondary_url, wildcard_settings_,
                            clock_);
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
  DCHECK_EQ(iterating_, 0);
  const std::string& primary_host = primary_pattern.GetHost();
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

size_t HostIndexedContentSettings::size() const {
  size_t size = 0;
  for (const auto& it : primary_host_indexed_) {
    size += it.second.size();
  }
  for (const auto& it : secondary_host_indexed_) {
    size += it.second.size();
  }
  size += wildcard_settings_.size();
  return size;
}

bool HostIndexedContentSettings::empty() const {
  return primary_host_indexed_.empty() && secondary_host_indexed_.empty() &&
         wildcard_settings_.empty();
}

void HostIndexedContentSettings::SetClockForTesting(const base::Clock* clock) {
  clock_ = clock;
}

}  // namespace content_settings
