// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webapps/installable/installed_webapp_provider.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/feature_list.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "chrome/browser/webapps/installable/installed_webapp_bridge.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/permission_settings_info.h"
#include "components/content_settings/core/browser/permission_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "url/gurl.h"

using content_settings::RuleIterator;

namespace {

bool IsSupportedContentType(ContentSettingsType content_type) {
  switch (content_type) {
    case ContentSettingsType::NOTIFICATIONS:
      return true;
    case ContentSettingsType::GEOLOCATION:
      return !base::FeatureList::IsEnabled(
          content_settings::features::kApproximateGeolocationPermission);
    case ContentSettingsType::GEOLOCATION_WITH_OPTIONS:
      return base::FeatureList::IsEnabled(
          content_settings::features::kApproximateGeolocationPermission);
    default:
      return false;
  }
}

}  // namespace

InstalledWebappProvider::InstalledWebappProvider() {
  InstalledWebappBridge::SetProviderInstance(this);
  RefreshRulesForType(ContentSettingsType::NOTIFICATIONS);
  RefreshRulesForType(ContentSettingsType::GEOLOCATION);
  RefreshRulesForType(ContentSettingsType::GEOLOCATION_WITH_OPTIONS);
}
InstalledWebappProvider::~InstalledWebappProvider() {
  InstalledWebappBridge::SetProviderInstance(nullptr);
}

std::unique_ptr<RuleIterator> InstalledWebappProvider::GetRuleIterator(
    ContentSettingsType content_type,
    bool incognito) const {
  if (incognito || !IsSupportedContentType(content_type)) {
    return nullptr;
  }
  return value_map_.GetRuleIterator(content_type);
}

std::unique_ptr<content_settings::Rule> InstalledWebappProvider::GetRule(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    bool off_the_record) const {
  if (off_the_record || !IsSupportedContentType(content_type)) {
    return nullptr;
  }
  base::AutoLock auto_lock(value_map_.GetLock());
  return value_map_.GetRule(primary_url, secondary_url, content_type);
}

bool InstalledWebappProvider::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const base::Value& value,
    const content_settings::ContentSettingConstraints& constraints) {
  // You can't set settings through this provider.
  return false;
}

void InstalledWebappProvider::ClearAllContentSettingsRules(
    ContentSettingsType content_type) {
  // You can't set settings through this provider.
}

void InstalledWebappProvider::ShutdownOnUIThread() {
  CHECK(CalledOnValidThread());
  RemoveAllObservers();
}

void InstalledWebappProvider::Notify(ContentSettingsType content_type) {
  CHECK(CalledOnValidThread());
  if (is_refreshing_) {
    return;
  }
  RefreshRulesForType(content_type);
  NotifyObservers(ContentSettingsPattern::Wildcard(),
                  ContentSettingsPattern::Wildcard(), content_type);
}

void InstalledWebappProvider::RefreshRulesForType(
    ContentSettingsType content_type) {
  CHECK(CalledOnValidThread());
  if (!IsSupportedContentType(content_type) || is_refreshing_) {
    return;
  }

  base::AutoReset<bool> reset_refreshing(&is_refreshing_, true);
  RuleList rules =
      InstalledWebappBridge::GetInstalledWebappPermissions(content_type);
  const auto* info =
      content_settings::PermissionSettingsRegistry::GetInstance()->Get(
          content_type);
  CHECK(info);

  base::AutoLock auto_lock(value_map_.GetLock());
  value_map_.DeleteValues(content_type);
  for (const auto& [origin, setting] : rules) {
    DCHECK(info->delegate().IsValid(setting)) << setting;
    value_map_.SetValue(ContentSettingsPattern::FromURLNoWildcard(origin),
                        ContentSettingsPattern::Wildcard(), content_type,
                        info->delegate().ToValue(setting),
                        content_settings::RuleMetaData{});
  }
}
