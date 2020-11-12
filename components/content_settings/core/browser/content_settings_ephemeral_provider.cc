// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_ephemeral_provider.h"

#include "base/stl_util.h"
#include "base/time/default_clock.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"

namespace content_settings {

// ////////////////////////////////////////////////////////////////////////////
// EphemeralProvider:
//

EphemeralProvider::EphemeralProvider(bool store_last_modified)
    : store_last_modified_(store_last_modified),
      clock_(base::DefaultClock::GetInstance()) {
  ContentSettingsRegistry* content_settings =
      ContentSettingsRegistry::GetInstance();
  WebsiteSettingsRegistry* website_settings =
      WebsiteSettingsRegistry::GetInstance();
  for (const WebsiteSettingsInfo* info : *website_settings) {
    const ContentSettingsInfo* content_type_info =
        content_settings->Get(info->type());
    // If this is an ephemeral content setting, handle it in this class.
    if (content_type_info && content_type_info->storage_behavior() ==
                                 ContentSettingsInfo::EPHEMERAL) {
      supported_types_.insert(info->type());
    }
  }
}

EphemeralProvider::~EphemeralProvider() {}

std::unique_ptr<RuleIterator> EphemeralProvider::GetRuleIterator(
    ContentSettingsType content_type,
    bool incognito) const {
  return content_settings_rules_.GetRuleIterator(content_type, nullptr);
}

bool EphemeralProvider::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    std::unique_ptr<base::Value>&& in_value,
    const ContentSettingConstraints& /*constraint*/) {
  DCHECK(CalledOnValidThread());

  if (!base::Contains(supported_types_, content_type))
    return false;

  // Default settings are set using a wildcard pattern for both
  // |primary_pattern| and |secondary_pattern|. Don't store default settings in
  // the EphemeralProvider. The EphemeralProvider handles settings for
  // specific sites/origins defined by the |primary_pattern| and the
  // |secondary_pattern|. Default settings are handled by the DefaultProvider.
  if (primary_pattern == ContentSettingsPattern::Wildcard() &&
      secondary_pattern == ContentSettingsPattern::Wildcard()) {
    return false;
  }

  std::unique_ptr<base::Value> value(std::move(in_value));
  if (value) {
    content_settings_rules_.SetValue(
        primary_pattern, secondary_pattern, content_type,
        store_last_modified_ ? clock_->Now() : base::Time(), std::move(*value),
        {});
    NotifyObservers(primary_pattern, secondary_pattern, content_type);
  } else {
    // If the value exists, delete it.
    if (content_settings_rules_.GetLastModified(
            primary_pattern, secondary_pattern, content_type) != base::Time()) {
      content_settings_rules_.DeleteValue(primary_pattern, secondary_pattern,
                                          content_type);
      NotifyObservers(primary_pattern, secondary_pattern, content_type);
    }
  }
  return true;
}

base::Time EphemeralProvider::GetWebsiteSettingLastModified(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type) {
  DCHECK(CalledOnValidThread());

  return content_settings_rules_.GetLastModified(
      primary_pattern, secondary_pattern, content_type);
}

void EphemeralProvider::ClearAllContentSettingsRules(
    ContentSettingsType content_type) {
  DCHECK(CalledOnValidThread());

  if (content_settings_rules_.find(content_type) ==
      content_settings_rules_.end())
    return;

  content_settings_rules_.DeleteValues(content_type);

  NotifyObservers(ContentSettingsPattern(), ContentSettingsPattern(),
                  content_type);
}

void EphemeralProvider::ShutdownOnUIThread() {
  DCHECK(CalledOnValidThread());
  RemoveAllObservers();
}

void EphemeralProvider::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

}  // namespace content_settings
