// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_auto_reauthn_permission_context.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/webid/federated_identity_auto_reauthn_permission_context_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

class FederatedIdentityAutoReauthnPermissionContextTest : public testing::Test {
 public:
  FederatedIdentityAutoReauthnPermissionContextTest() = default;
  ~FederatedIdentityAutoReauthnPermissionContextTest() override = default;
  FederatedIdentityAutoReauthnPermissionContextTest(
      FederatedIdentityAutoReauthnPermissionContextTest&) = delete;
  FederatedIdentityAutoReauthnPermissionContextTest& operator=(
      FederatedIdentityAutoReauthnPermissionContextTest&) = delete;

  void SetUp() override {
    context_ =
        FederatedIdentityAutoReauthnPermissionContextFactory::GetForProfile(
            &profile_);
    host_content_settings_map_ =
        HostContentSettingsMapFactory::GetForProfile(&profile_);
  }

  Profile* profile() { return &profile_; }

 protected:
  base::raw_ptr<FederatedIdentityAutoReauthnPermissionContext> context_;
  base::raw_ptr<HostContentSettingsMap> host_content_settings_map_;

  ContentSetting GetContentSetting(const GURL& rp_url) {
    return host_content_settings_map_->GetContentSetting(
        rp_url, rp_url,
        ContentSettingsType::FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

// Test that FedCM auto re-authn is opt-in by default.
TEST_F(FederatedIdentityAutoReauthnPermissionContextTest,
       AutoReauthnEnabledByDefault) {
  GURL rp_url("https://rp.com");
  EXPECT_TRUE(context_->HasAutoReauthnContentSetting());
  EXPECT_FALSE(context_->IsAutoReauthnEmbargoed(url::Origin::Create(rp_url)));
}

// Test that
// FederatedIdentityAutoReauthnPermissionContext::RecordDisplayAndEmbargo()
// blocks the permission if it is enabled.
TEST_F(FederatedIdentityAutoReauthnPermissionContextTest, EnabledEmbargo) {
  GURL rp_url("https://rp.com");
  host_content_settings_map_->SetDefaultContentSetting(
      ContentSettingsType::FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION,
      CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW, GetContentSetting(rp_url));

  // Embargoing `rp_url` should block the content setting for `rp_url`.
  context_->RecordDisplayAndEmbargo(url::Origin::Create(rp_url));
  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetContentSetting(rp_url));
}
