// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_api_permission_context.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/webid/federated_identity_api_permission_context_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using FederatedIdentityPermissionStatus =
    content::FederatedIdentityApiPermissionContextDelegate::PermissionStatus;

class FederatedIdentityApiPermissionContextTest : public testing::Test {
 public:
  FederatedIdentityApiPermissionContextTest() = default;
  ~FederatedIdentityApiPermissionContextTest() override = default;
  FederatedIdentityApiPermissionContextTest(
      FederatedIdentityApiPermissionContextTest&) = delete;
  FederatedIdentityApiPermissionContextTest& operator=(
      FederatedIdentityApiPermissionContextTest&) = delete;

  void SetUp() override {
    context_ =
        FederatedIdentityApiPermissionContextFactory::GetForProfile(&profile_);
    host_content_settings_map_ =
        HostContentSettingsMapFactory::GetForProfile(&profile_);
  }

  Profile* profile() { return &profile_; }

 protected:
  raw_ptr<FederatedIdentityApiPermissionContext, DanglingUntriaged> context_;
  raw_ptr<HostContentSettingsMap, DanglingUntriaged> host_content_settings_map_;

  ContentSetting GetContentSetting(const GURL& rp_url) {
    return host_content_settings_map_->GetContentSetting(
        rp_url, rp_url, ContentSettingsType::FEDERATED_IDENTITY_API);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

// Test that FederatedIdentityApiPermissionContext::RecordDismissAndEmbargo()
// clears the permission if it is enabled.
TEST_F(FederatedIdentityApiPermissionContextTest, EnabledEmbargo) {
  GURL rp_url("https://rp.com");
  host_content_settings_map_->SetDefaultContentSetting(
      ContentSettingsType::FEDERATED_IDENTITY_API, CONTENT_SETTING_BLOCK);
  host_content_settings_map_->SetContentSettingDefaultScope(
      rp_url, rp_url, ContentSettingsType::FEDERATED_IDENTITY_API,
      CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW, GetContentSetting(rp_url));

  // Embargoing `rp_url` should return the default content setting for `rp_url`.
  context_->RecordDismissAndEmbargo(url::Origin::Create(rp_url));
  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetContentSetting(rp_url));
}

// Test that FedCM is not disabled if the user disabled all third-party cookies
// but whitelisted the relying party.
TEST_F(FederatedIdentityApiPermissionContextTest,
       WhitelistedSiteForThirdPartyCookies) {
  const GURL kRpUrl("https://rp.com");

  profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kBlockThirdParty));
  CookieSettingsFactory::GetForProfile(profile())->SetThirdPartyCookieSetting(
      kRpUrl, ContentSetting::CONTENT_SETTING_ALLOW);

  EXPECT_EQ(FederatedIdentityPermissionStatus::GRANTED,
            context_->GetApiPermissionStatus(url::Origin::Create(kRpUrl)));
}
