// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_CONTENT_SETTINGS_PROVIDER_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_CONTENT_SETTINGS_PROVIDER_H_

// A content setting provider that is set by the custodian of a supervised user.

#include "base/callback_list.h"
#include "base/synchronization/lock.h"
#include "components/content_settings/core/browser/content_settings_global_value_map.h"
#include "components/content_settings/core/browser/content_settings_observable_provider.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/common/content_settings_partition_key.h"

namespace supervised_user {
class SupervisedUserSettingsService;

// SupervisedUserContentSettingsProvider that provides content-settings managed
// by the custodian of a supervised user.
//
// PartitionKey is ignored by this provider because the content settings should
// apply across partitions.
class SupervisedUserContentSettingsProvider
    : public content_settings::ObservableProvider {
 public:
  explicit SupervisedUserContentSettingsProvider(
      supervised_user::SupervisedUserSettingsService*
          supervised_user_settings_service);

  SupervisedUserContentSettingsProvider(
      const SupervisedUserContentSettingsProvider&) = delete;
  SupervisedUserContentSettingsProvider& operator=(
      const SupervisedUserContentSettingsProvider&) = delete;

  ~SupervisedUserContentSettingsProvider() override;

  // ProviderInterface implementations.
  std::unique_ptr<content_settings::RuleIterator> GetRuleIterator(
      ContentSettingsType content_type,
      bool incognito,
      const content_settings::PartitionKey& partition_key) const override;
  std::unique_ptr<content_settings::Rule> GetRule(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      bool off_the_record,
      const content_settings::PartitionKey& partition_key) const override;

  bool SetWebsiteSetting(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      base::Value&& value,
      const content_settings::ContentSettingConstraints& constraints,
      const content_settings::PartitionKey& partition_key) override;

  void ClearAllContentSettingsRules(
      ContentSettingsType content_type,
      const content_settings::PartitionKey& partition_key) override;

  void ShutdownOnUIThread() override;

 private:
  // Callback on receiving settings from the supervised user settings service.
  void OnSupervisedSettingsAvailable(const base::Value::Dict& settings);

  content_settings::GlobalValueMap value_map_;

  // Used around accesses to the |value_map_| object to guarantee
  // thread safety.
  mutable base::Lock lock_;

  base::CallbackListSubscription user_settings_subscription_;
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_CONTENT_SETTINGS_PROVIDER_H_
