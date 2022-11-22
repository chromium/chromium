// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_test_util.h"

#include "base/feature_list.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"

namespace privacy_sandbox_test_util {

MockPrivacySandboxObserver::MockPrivacySandboxObserver() = default;
MockPrivacySandboxObserver::~MockPrivacySandboxObserver() = default;
MockPrivacySandboxSettingsDelegate::MockPrivacySandboxSettingsDelegate() =
    default;
MockPrivacySandboxSettingsDelegate::~MockPrivacySandboxSettingsDelegate() =
    default;

void SetupMinimialTestStateForM1(
    sync_preferences::TestingPrefServiceSyncable* testing_pref_service,
    HostContentSettingsMap* map,
    ContentSetting default_cookie_setting,
    const std::vector<CookieContentSettingException>& user_cookie_exceptions) {
  // Setup cookie content settings.
  auto user_provider = std::make_unique<content_settings::MockProvider>();

  if (default_cookie_setting != kNoSetting) {
    user_provider->SetWebsiteSetting(
        ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
        ContentSettingsType::COOKIES, base::Value(default_cookie_setting));
  }

  for (const auto& exception : user_cookie_exceptions) {
    user_provider->SetWebsiteSetting(
        ContentSettingsPattern::FromString(exception.primary_pattern),
        ContentSettingsPattern::FromString(exception.secondary_pattern),
        ContentSettingsType::COOKIES, base::Value(exception.content_setting));
  }

  content_settings::TestUtils::OverrideProvider(
      map, std::move(user_provider), HostContentSettingsMap::DEFAULT_PROVIDER);
}

void SetupTestState(
    sync_preferences::TestingPrefServiceSyncable* testing_pref_service,
    HostContentSettingsMap* map,
    bool privacy_sandbox_enabled,
    bool block_third_party_cookies,
    ContentSetting default_cookie_setting,
    const std::vector<CookieContentSettingException>& user_cookie_exceptions,
    ContentSetting managed_cookie_setting,
    const std::vector<CookieContentSettingException>&
        managed_cookie_exceptions) {
  // Setup block-third-party-cookies settings.
  testing_pref_service->SetUserPref(
      prefs::kCookieControlsMode,
      base::Value(static_cast<int>(
          block_third_party_cookies
              ? content_settings::CookieControlsMode::kBlockThirdParty
              : content_settings::CookieControlsMode::kOff)));

  // Setup cookie content settings.
  auto user_provider = std::make_unique<content_settings::MockProvider>();
  auto managed_provider = std::make_unique<content_settings::MockProvider>();

  if (default_cookie_setting != kNoSetting) {
    user_provider->SetWebsiteSetting(
        ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
        ContentSettingsType::COOKIES, base::Value(default_cookie_setting));
  }

  for (const auto& exception : user_cookie_exceptions) {
    user_provider->SetWebsiteSetting(
        ContentSettingsPattern::FromString(exception.primary_pattern),
        ContentSettingsPattern::FromString(exception.secondary_pattern),
        ContentSettingsType::COOKIES, base::Value(exception.content_setting));
  }

  if (managed_cookie_setting != kNoSetting) {
    managed_provider->SetWebsiteSetting(
        ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
        ContentSettingsType::COOKIES, base::Value(managed_cookie_setting));
  }

  for (const auto& exception : managed_cookie_exceptions) {
    managed_provider->SetWebsiteSetting(
        ContentSettingsPattern::FromString(exception.primary_pattern),
        ContentSettingsPattern::FromString(exception.secondary_pattern),
        ContentSettingsType::COOKIES, base::Value(exception.content_setting));
  }

  content_settings::TestUtils::OverrideProvider(
      map, std::move(user_provider), HostContentSettingsMap::DEFAULT_PROVIDER);
  content_settings::TestUtils::OverrideProvider(
      map, std::move(managed_provider),
      HostContentSettingsMap::POLICY_PROVIDER);

  // Only adjust the Privacy Sandbox preference which should be being consulted
  // based on feature state.
  if (base::FeatureList::IsEnabled(privacy_sandbox::kPrivacySandboxSettings3)) {
    testing_pref_service->SetUserPref(prefs::kPrivacySandboxApisEnabledV2,
                                      base::Value(privacy_sandbox_enabled));
  } else {
    testing_pref_service->SetUserPref(prefs::kPrivacySandboxApisEnabled,
                                      base::Value(privacy_sandbox_enabled));
  }
}

}  // namespace privacy_sandbox_test_util
