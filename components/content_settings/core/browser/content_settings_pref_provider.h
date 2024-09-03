// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_PREF_PROVIDER_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_PREF_PROVIDER_H_

// A content settings provider that takes its settings out of the pref service.

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/user_modifiable_provider.h"
#include "components/content_settings/core/common/content_settings_partition_key.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace base {
class Clock;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace content_settings {

class ContentSettingsPref;

// Content settings provider that provides content settings from the user
// preference.
class PrefProvider : public UserModifiableProvider {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  PrefProvider(PrefService* prefs,
               bool off_the_record,
               bool store_last_modified,
               bool restore_session);

  PrefProvider(const PrefProvider&) = delete;
  PrefProvider& operator=(const PrefProvider&) = delete;

  ~PrefProvider() override;

  // UserModifiableProvider implementations.
  std::unique_ptr<RuleIterator> GetRuleIterator(
      ContentSettingsType content_type,
      bool off_the_record,
      const PartitionKey& partition_key) const override;

  std::unique_ptr<Rule> GetRule(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      bool off_the_record,
      const PartitionKey& partition_key) const override;

  bool SetWebsiteSetting(const ContentSettingsPattern& primary_pattern,
                         const ContentSettingsPattern& secondary_pattern,
                         ContentSettingsType content_type,
                         base::Value&& value,
                         const ContentSettingConstraints& constraints,
                         const PartitionKey& partition_key) override;
  void ClearAllContentSettingsRules(ContentSettingsType content_type,
                                    const PartitionKey& partition_key) override;
  void ShutdownOnUIThread() override;
  bool UpdateLastUsedTime(const GURL& primary_url,
                          const GURL& secondary_url,
                          ContentSettingsType content_type,
                          const base::Time time,
                          const PartitionKey& partition_key) override;
  bool ResetLastVisitTime(const ContentSettingsPattern& primary_pattern,
                          const ContentSettingsPattern& secondary_pattern,
                          ContentSettingsType content_type,
                          const PartitionKey& partition_key) override;
  bool UpdateLastVisitTime(const ContentSettingsPattern& primary_pattern,
                           const ContentSettingsPattern& secondary_pattern,
                           ContentSettingsType content_type,
                           const PartitionKey& partition_key) override;
  std::optional<base::TimeDelta> RenewContentSetting(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      std::optional<ContentSetting> setting_to_match,
      const PartitionKey& partition_key) override;
  void SetClockForTesting(const base::Clock* clock) override;

  ContentSettingsPref* GetPref(ContentSettingsType type) const;

 private:
  friend class DeadlockCheckerObserver;  // For testing.

  void Notify(const ContentSettingsPattern& primary_pattern,
              const ContentSettingsPattern& secondary_pattern,
              ContentSettingsType content_type,
              const PartitionKey* partition_key);

  bool SetLastVisitTime(const ContentSettingsPattern& primary_pattern,
                        const ContentSettingsPattern& secondary_pattern,
                        ContentSettingsType content_type,
                        const base::Time time,
                        const PartitionKey& partition_key);

  // Finds the first setting whose Rule satisfies `is_match`, and performs some
  // update. `perform_update` may modify the Rule in-place, and should return
  // true if any modifications were made.  Returns whether or not any setting
  // was updated.
  bool UpdateSetting(ContentSettingsType content_type,
                     base::FunctionRef<bool(const Rule&)> is_match,
                     base::FunctionRef<bool(Rule&)> perform_update,
                     const PartitionKey& partition_key);

  // Clean up the obsolete preferences from the user's profile.
  void DiscardOrMigrateObsoletePreferences();

  // Returns true if this provider supports the given |content_type|.
  bool supports_type(ContentSettingsType content_type) const {
    return content_settings_prefs_.find(content_type) !=
           content_settings_prefs_.end();
  }

  // Weak; owned by the Profile and reset in ShutdownOnUIThread.
  raw_ptr<PrefService> prefs_;

  const bool off_the_record_;

  bool store_last_modified_;

  PrefChangeRegistrar pref_change_registrar_;

  std::map<ContentSettingsType, std::unique_ptr<ContentSettingsPref>>
      content_settings_prefs_;

  base::ThreadChecker thread_checker_;

  raw_ptr<const base::Clock> clock_;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_PREF_PROVIDER_H_
