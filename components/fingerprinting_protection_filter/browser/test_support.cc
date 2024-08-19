// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/browser/test_support.h"

#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"

namespace fingerprinting_protection_filter {

scoped_refptr<HostContentSettingsMap> TestSupport::InitializePrefs() {
  HostContentSettingsMap::RegisterProfilePrefs(prefs()->registry());
  privacy_sandbox::tracking_protection::RegisterProfilePrefs(
      prefs()->registry());
  content_settings::CookieSettings::RegisterProfilePrefs(prefs()->registry());

  // Always set this pref to true as the ThrottleManager unit tests
  // are not testing this functionality.
  prefs()->SetBoolean(prefs::kFingerprintingProtectionEnabled, true);
  prefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kBlockThirdParty));

  return base::MakeRefCounted<HostContentSettingsMap>(
      prefs(), /*is_off_the_record=*/false, /*store_last_modified=*/false,
      /*restore_session=*/false, /*should_record_metrics=*/false);
}

TestSupport::TestSupport()
    : host_content_settings_map_(InitializePrefs()),
      tracking_protection_settings_(prefs(),
                                    host_content_settings_map_.get(),
                                    /*is_incognito=*/false) {}

TestSupport::~TestSupport() {
  host_content_settings_map_->ShutdownOnUIThread();
  tracking_protection_settings_.Shutdown();
}

}  // namespace fingerprinting_protection_filter
