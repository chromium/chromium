// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/content_settings.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "components/content_settings/core/common/content_settings_metadata.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"

namespace {

void FilterRulesForType(ContentSettingsForOneType& settings,
                        const GURL& primary_url) {
  std::erase_if(settings,
                [&primary_url](const ContentSettingPatternSource& source) {
                  return !source.primary_pattern.Matches(primary_url);
                });
  // We should have at least on rule remaining (the default rule).
  DCHECK_GE(settings.size(), 1u);
}

}  // namespace

ContentSetting IntToContentSetting(int content_setting) {
  return ((content_setting < 0) ||
          (content_setting >= CONTENT_SETTING_NUM_SETTINGS))
             ? CONTENT_SETTING_DEFAULT
             : static_cast<ContentSetting>(content_setting);
}

ContentSettingPatternSource::ContentSettingPatternSource(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    base::Value setting_value,
    content_settings::mojom::ProviderType source,
    bool incognito,
    content_settings::RuleMetaData metadata)
    : primary_pattern(primary_pattern),
      secondary_pattern(secondary_pattern),
      setting_value(std::move(setting_value)),
      metadata(metadata),
      source(source),
      incognito(incognito) {}

ContentSettingPatternSource::ContentSettingPatternSource() : incognito(false) {}

ContentSettingPatternSource::ContentSettingPatternSource(
    const ContentSettingPatternSource& other) {
  *this = other;
}

ContentSettingPatternSource& ContentSettingPatternSource::operator=(
    const ContentSettingPatternSource& other) {
  primary_pattern = other.primary_pattern;
  secondary_pattern = other.secondary_pattern;
  setting_value = other.setting_value.Clone();
  metadata = other.metadata;
  source = other.source;
  incognito = other.incognito;
  return *this;
}

ContentSettingPatternSource::~ContentSettingPatternSource() = default;

ContentSetting ContentSettingPatternSource::GetContentSetting() const {
  return content_settings::ValueToContentSetting(setting_value);
}

bool ContentSettingPatternSource::IsExpired() const {
  return !metadata.expiration().is_null() &&
         metadata.expiration() < base::Time::Now();
}

bool ContentSettingPatternSource::operator==(
    const ContentSettingPatternSource& other) const = default;

std::ostream& operator<<(std::ostream& os,
                         const ContentSettingPatternSource& source) {
  os << "[(";
  PrintTo(source.primary_pattern, &os);
  os << ", ";
  PrintTo(source.secondary_pattern, &os);
  os << ") source=" << static_cast<int>(source.source)
     << " value=" << source.setting_value.DebugString() << "]";
  return os;
}

// static
bool RendererContentSettingRules::IsRendererContentSetting(
    ContentSettingsType content_type) {
  return content_type == ContentSettingsType::IMAGES ||
         content_type == ContentSettingsType::JAVASCRIPT ||
         content_type == ContentSettingsType::POPUPS ||
         content_type == ContentSettingsType::MIXEDSCRIPT ||
         content_type == ContentSettingsType::AUTO_DARK_WEB_CONTENT;
}

void RendererContentSettingRules::FilterRulesByOutermostMainFrameURL(
    const GURL& outermost_main_frame_url) {
  FilterRulesForType(mixed_content_rules, outermost_main_frame_url);
}

RendererContentSettingRules::RendererContentSettingRules() = default;

RendererContentSettingRules::~RendererContentSettingRules() = default;

RendererContentSettingRules::RendererContentSettingRules(
    const RendererContentSettingRules&) = default;

RendererContentSettingRules::RendererContentSettingRules(
    RendererContentSettingRules&& rules) = default;

RendererContentSettingRules& RendererContentSettingRules::operator=(
    const RendererContentSettingRules& rules) = default;

RendererContentSettingRules& RendererContentSettingRules::operator=(
    RendererContentSettingRules&& rules) = default;

bool RendererContentSettingRules::operator==(
    const RendererContentSettingRules& other) const = default;
