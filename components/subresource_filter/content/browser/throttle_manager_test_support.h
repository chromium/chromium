// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_THROTTLE_MANAGER_TEST_SUPPORT_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_THROTTLE_MANAGER_TEST_SUPPORT_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"

class HostContentSettingsMap;

namespace content {
class WebContents;
}

namespace content_settings {
class CookieSettings;
}

namespace subresource_filter {

class SubresourceFilterProfileContext;

// Sets up necessary dependencies of ContentSubresourceFilterThrottleManager for
// convenience in unittests.
class ThrottleManagerTestSupport {
 public:
  explicit ThrottleManagerTestSupport(content::WebContents* web_contents);
  ~ThrottleManagerTestSupport();

  ThrottleManagerTestSupport(const ThrottleManagerTestSupport&) = delete;
  ThrottleManagerTestSupport& operator=(const ThrottleManagerTestSupport&) =
      delete;

  SubresourceFilterProfileContext* profile_context() {
    return profile_context_.get();
  }

  // Turns on/off the smart UI feature (currently enabled in production on
  // some platforms only).
  void SetShouldUseSmartUI(bool enabled);

 private:
  sync_preferences::TestingPrefServiceSyncable prefs_;
  scoped_refptr<HostContentSettingsMap> settings_map_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  std::unique_ptr<SubresourceFilterProfileContext> profile_context_;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_THROTTLE_MANAGER_TEST_SUPPORT_H_
