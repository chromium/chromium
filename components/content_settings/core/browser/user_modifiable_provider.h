// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_USER_MODIFIABLE_PROVIDER_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_USER_MODIFIABLE_PROVIDER_H_

#include "components/content_settings/core/browser/content_settings_observable_provider.h"
#include "components/content_settings/core/common/content_settings.h"

namespace base {
class Clock;
}

class ContentSettingsPattern;

namespace content_settings {

// Content settings provider that provides settings which may be modified by the
// user.
class UserModifiableProvider : public ObservableProvider {
 public:
  ~UserModifiableProvider() override {}
  // Returns the timestamp that a particular setting was last modified.
  virtual base::Time GetWebsiteSettingLastModified(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type) = 0;
  // Sets the providers internal clock for testing purposes.
  virtual void SetClockForTesting(base::Clock* clock) = 0;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_USER_MODIFIABLE_PROVIDER_H_
