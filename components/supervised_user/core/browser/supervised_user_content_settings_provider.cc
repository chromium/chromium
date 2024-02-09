// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_content_settings_provider.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_metadata.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"

namespace {

struct ContentSettingsFromSupervisedSettingsEntry {
  const char* setting_name;
  ContentSettingsType content_type;
  ContentSetting content_setting;
};

const ContentSettingsFromSupervisedSettingsEntry
    kContentSettingsFromSupervisedSettingsMap[] = {
        {
            supervised_user::kGeolocationDisabled,
            ContentSettingsType::GEOLOCATION,
            CONTENT_SETTING_BLOCK,
        },
        {
            supervised_user::kCameraMicDisabled,
            ContentSettingsType::MEDIASTREAM_CAMERA,
            CONTENT_SETTING_BLOCK,
        },
        {
            supervised_user::kCameraMicDisabled,
            ContentSettingsType::MEDIASTREAM_MIC,
            CONTENT_SETTING_BLOCK,
        },
        {
            supervised_user::kCookiesAlwaysAllowed,
            ContentSettingsType::COOKIES,
            CONTENT_SETTING_ALLOW,
        }};

}  // namespace

namespace supervised_user {

SupervisedUserContentSettingsProvider::SupervisedUserContentSettingsProvider(
    supervised_user::SupervisedUserSettingsService*
        supervised_user_settings_service) {
  // The SupervisedUserContentSettingsProvider is owned by the
  // HostContentSettingsMap which DependsOn the SupervisedUserSettingsService
  // (through their factories). This means this will get destroyed before the
  // SUSS and will be unsubscribed from it.
  user_settings_subscription_ =
      supervised_user_settings_service->SubscribeForSettingsChange(
          base::BindRepeating(&SupervisedUserContentSettingsProvider::
                                  OnSupervisedSettingsAvailable,
                              base::Unretained(this)));
}

SupervisedUserContentSettingsProvider::
    ~SupervisedUserContentSettingsProvider() = default;

std::unique_ptr<content_settings::RuleIterator>
SupervisedUserContentSettingsProvider::GetRuleIterator(
    ContentSettingsType content_type,
    bool incognito,
    const content_settings::PartitionKey& partition_key) const {
  base::AutoLock auto_lock(lock_);
  return value_map_.GetRuleIterator(content_type);
}

std::unique_ptr<content_settings::Rule>
SupervisedUserContentSettingsProvider::GetRule(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    bool off_the_record,
    const content_settings::PartitionKey& partition_key) const {
  base::AutoLock auto_lock(lock_);
  ContentSetting setting = value_map_.GetContentSetting(content_type);
  if (setting != CONTENT_SETTING_DEFAULT) {
    return std::make_unique<content_settings::Rule>(
        ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
        base::Value(setting), content_settings::RuleMetaData{});
  }
  return nullptr;
}

void SupervisedUserContentSettingsProvider::OnSupervisedSettingsAvailable(
    const base::Value::Dict& settings) {
  std::vector<ContentSettingsType> to_notify;
  // Entering locked scope to update content settings.
  {
    base::AutoLock auto_lock(lock_);
    for (const auto& entry : kContentSettingsFromSupervisedSettingsMap) {
      ContentSetting new_setting = CONTENT_SETTING_DEFAULT;
      if (settings.Find(entry.setting_name)) {
        DCHECK(settings.Find(entry.setting_name)->is_bool());
        if (settings.FindBool(entry.setting_name).value_or(false)) {
          new_setting = entry.content_setting;
        }
      }
      if (new_setting != value_map_.GetContentSetting(entry.content_type)) {
        to_notify.push_back(entry.content_type);
        value_map_.SetContentSetting(entry.content_type, new_setting);
      }
    }
  }
  for (ContentSettingsType type : to_notify) {
    NotifyObservers(ContentSettingsPattern::Wildcard(),
                    ContentSettingsPattern::Wildcard(), type,
                    /*partition_key=*/nullptr);
  }
}

// Since the SupervisedUserContentSettingsProvider is a read only content
// settings provider, all methods of the ProviderInterface that set or delete
// any settings do nothing.
bool SupervisedUserContentSettingsProvider::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    base::Value&& value,
    const content_settings::ContentSettingConstraints& constraints,
    const content_settings::PartitionKey& partition_key) {
  return false;
}

void SupervisedUserContentSettingsProvider::ClearAllContentSettingsRules(
    ContentSettingsType content_type,
    const content_settings::PartitionKey& partition_key) {}

void SupervisedUserContentSettingsProvider::ShutdownOnUIThread() {
  DCHECK(CalledOnValidThread());
  RemoveAllObservers();
  user_settings_subscription_ = {};
}

}  // namespace supervised_user
