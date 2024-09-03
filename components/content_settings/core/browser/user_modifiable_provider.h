// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_USER_MODIFIABLE_PROVIDER_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_USER_MODIFIABLE_PROVIDER_H_

#include "components/content_settings/core/browser/content_settings_observable_provider.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_partition_key.h"

namespace base {
class Clock;
}

namespace content_settings {

// Content settings provider that provides settings which may be modified by the
// user.
class UserModifiableProvider : public ObservableProvider {
 public:
  ~UserModifiableProvider() override = default;
  // Updates the `last_used` time for the given setting. Returns true if the
  // setting was found and updated.
  virtual bool UpdateLastUsedTime(const GURL& primary_url,
                                  const GURL& secondary_url,
                                  ContentSettingsType content_type,
                                  const base::Time time,
                                  const PartitionKey& partition_key) = 0;
  // Resets the last_visit time for the given setting. Returns true if the
  // setting was found and updated.
  virtual bool ResetLastVisitTime(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      const PartitionKey& partition_key) = 0;
  // Updates the last_visit time for the given setting. Returns true if the
  // setting was found and updated.
  virtual bool UpdateLastVisitTime(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      const PartitionKey& partition_key) = 0;
  // Updates the expiration time for the given setting, based on its lifetime.
  // (Only settings that have lifetimes may be renewed.) If `setting_to_match`
  // is nullopt, then the first rule with the appropriate patterns and type will
  // be updated; otherwise, a rule will only be updated if its value matches
  // `setting_to_match`. Returns the TimeDelta between now and the setting's old
  // expiration if any setting was updated; nullopt otherwise.
  virtual std::optional<base::TimeDelta> RenewContentSetting(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      std::optional<ContentSetting> setting_to_match,
      const PartitionKey& partition_key) = 0;
  // Sets the providers internal clock for testing purposes.
  virtual void SetClockForTesting(const base::Clock* clock) = 0;

  // Asks provider to expire the website setting for a particular
  // |primary_pattern|, |secondary_pattern|, |content_type| tuple.
  //
  // This should only be called on the UI thread, and not after
  // ShutdownOnUIThread has been called.
  virtual void ExpireWebsiteSetting(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_settings_type,
      const PartitionKey& partition_key);
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_USER_MODIFIABLE_PROVIDER_H_
