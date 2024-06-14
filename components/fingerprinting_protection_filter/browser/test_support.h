// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_BROWSER_TEST_SUPPORT_H_
#define COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_BROWSER_TEST_SUPPORT_H_

#include "base/memory/scoped_refptr.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"

class HostContentSettingsMap;

namespace fingerprinting_protection_filter {

// Sets up necessary dependencies of filtering classes for convenience in
// unittests.
class TestSupport {
 public:
  explicit TestSupport();
  ~TestSupport();

  TestSupport(const TestSupport&) = delete;
  TestSupport& operator=(const TestSupport&) = delete;

  privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings() {
    return &tracking_protection_settings_;
  }

  sync_preferences::TestingPrefServiceSyncable* prefs() { return &prefs_; }

 private:
  scoped_refptr<HostContentSettingsMap> InitializePrefs();

  sync_preferences::TestingPrefServiceSyncable prefs_;
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  privacy_sandbox::TrackingProtectionSettings tracking_protection_settings_;
};

}  // namespace fingerprinting_protection_filter

#endif  // COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_BROWSER_TEST_SUPPORT_H_
