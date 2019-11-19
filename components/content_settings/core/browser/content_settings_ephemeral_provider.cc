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
    const ResourceIdentifier& resource_identifier,
    bool incognito) const {
  return content_settings_rules_.GetRuleIterator(content_type,
                                                 resource_identifier, nullptr);
}

bool EphemeralProvider::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    std::unique_ptr<base::Value>&& in_value) {
  DCHECK(CalledOnValidThread());

  if (!base::Contains(supported_types_, content_type))
    return false;

  // Default settings are set using a wildcard pattern for both
  // |primary_pattern| and |secondary_pattern|. Don't store default settings in
  // the EphemeralProvider. The EphemeralProvider handles settings for
  // specific sites/origins defined by the |primary_pattern| and the
  // |secondary_pattern|. Default settings are handled by the DefaultProvider.
  if (primary_pattern == ContentSettingsPattern::Wildcard() &&
      secondary_pattern == ContentSettingsPattern::Wildcard() &&
      resource_identifier.empty()) {
    return false;
  }

  std::unique_ptr<base::Value> value(std::move(in_value));
  if (value) {
    content_settings_rules_.SetValue(
        primary_pattern, secondary_pattern, content_type, resource_identifier,
        store_last_modified_ ? clock_->Now() : base::Time(), std::move(*value));
    NotifyObservers(primary_pattern, secondary_pattern, content_type,
                    resource_identifier);
  } else {
    // If the value exists, delete it.
    if (content_settings_rules_.GetLastModified(
            primary_pattern, secondary_pattern, content_type,
            resource_identifier) != base::Time()) {
      content_settings_rules_.DeleteValue(primary_pattern, secondary_pattern,
                                          content_type, resource_identifier);
      NotifyObservers(primary_pattern, secondary_pattern, content_type,
                      resource_identifier);
    }
  }
  return true;
}

base::Time EphemeralProvider::GetWebsiteSettingLastModified(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier) {
  DCHECK(CalledOnValidThread());

  return content_settings_rules_.GetLastModified(
      primary_pattern, secondary_pattern, content_type, resource_identifier);
}

void EphemeralProvider::ClearAllContentSettingsRules(
    ContentSettingsType content_type) {
  DCHECK(CalledOnValidThread());

  // Get all resource identifiers for this |content_type|.
  std::set<ResourceIdentifier> resource_identifiers;
  for (OriginIdentifierValueMap::EntryMap::const_iterator entry =
           content_settings_rules_.begin();
       entry != content_settings_rules_.end(); entry++) {
    if (entry->first.content_type == content_type)
      resource_identifiers.insert(entry->first.resource_identifier);
  }

  for (const ResourceIdentifier& resource_identifier : resource_identifiers)
    content_settings_rules_.DeleteValues(content_type, resource_identifier);

  if (!resource_identifiers.empty()) {
    NotifyObservers(ContentSettingsPattern(), ContentSettingsPattern(),
                    content_type, ResourceIdentifier());
  }
}

void EphemeralProvider::ShutdownOnUIThread() {
  DCHECK(CalledOnValidThread());
  RemoveAllObservers();
}

void EphemeralProvider::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

}  // namespace content_settings
