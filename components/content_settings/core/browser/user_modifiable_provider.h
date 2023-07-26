// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_USER_MODIFIABLE_PROVIDER_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_USER_MODIFIABLE_PROVIDER_H_

#include "components/content_settings/core/browser/content_settings_observable_provider.h"
#include "components/content_settings/core/common/content_settings.h"

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
                                  const base::Time time) = 0;
  // Resets the last_visit time for the given setting. Returns true if the
  // setting was found and updated.
  virtual bool ResetLastVisitTime(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type) = 0;
  // Updates the last_visit time for the given setting. Returns true if the
  // setting was found and updated.
  virtual bool UpdateLastVisitTime(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type) = 0;
  // Updates the expiration time for the given setting, based on its lifetime.
  // (Only settings that have lifetimes may be renewed.) If `setting_to_match`
  // is nullopt, then the first rule with the appropriate patterns and type will
  // be updated; otherwise, a rule will only be updated if its value matches
  // `setting_to_match`. Returns true if the setting was found and updated.
  virtual bool RenewContentSetting(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      absl::optional<ContentSetting> setting_to_match) = 0;
  // Sets the providers internal clock for testing purposes.
  virtual void SetClockForTesting(base::Clock* clock) = 0;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_USER_MODIFIABLE_PROVIDER_H_
