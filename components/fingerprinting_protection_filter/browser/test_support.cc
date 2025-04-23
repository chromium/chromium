// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/browser/test_support.h"

#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/fingerprinting_protection_filter/common/prefs.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"

namespace fingerprinting_protection_filter {

void TestSupport::InitializePrefsAndContentSettings() {
  HostContentSettingsMap::RegisterProfilePrefs(prefs()->registry());
  privacy_sandbox::tracking_protection::RegisterProfilePrefs(
      prefs()->registry());
  content_settings::CookieSettings::RegisterProfilePrefs(prefs()->registry());
  fingerprinting_protection_filter::prefs::RegisterProfilePrefs(
      prefs()->registry());

  // Always set this pref to true as the ThrottleManager unit tests
  // are not testing this functionality.
  prefs()->SetBoolean(::prefs::kFingerprintingProtectionEnabled, true);
  prefs()->SetInteger(
      ::prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kBlockThirdParty));

  host_content_settings_map_ = base::MakeRefCounted<HostContentSettingsMap>(
      prefs(), /*is_off_the_record=*/false, /*store_last_modified=*/false,
      /*restore_session=*/false,
      /*should_record_metrics=*/false);
}

TestSupport::TestSupport() {
  InitializePrefsAndContentSettings();
  tracking_protection_settings_ =
      std::make_unique<privacy_sandbox::TrackingProtectionSettings>(
          prefs(), host_content_settings_map_.get(),
          /*management_service=*/nullptr,
          /*is_incognito=*/false);
}

TestSupport::~TestSupport() {
  tracking_protection_settings_->Shutdown();
  host_content_settings_map_->ShutdownOnUIThread();
}

}  // namespace fingerprinting_protection_filter
