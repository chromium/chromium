// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_auto_reauthn_permission_context.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/trusted_vault/trusted_vault_service_factory.h"
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
    profile_ =
        TestingProfile::Builder()
            .AddTestingFactory(TrustedVaultServiceFactory::GetInstance(),
                               TrustedVaultServiceFactory::GetDefaultFactory())
            .AddTestingFactory(SyncServiceFactory::GetInstance(),
                               SyncServiceFactory::GetDefaultFactory())
            .Build();
    context_ =
        FederatedIdentityAutoReauthnPermissionContextFactory::GetForProfile(
            profile());
    host_content_settings_map_ =
        HostContentSettingsMapFactory::GetForProfile(profile());
  }

  Profile* profile() { return profile_.get(); }

 protected:
  raw_ptr<FederatedIdentityAutoReauthnPermissionContext, DanglingUntriaged>
      context_;
  raw_ptr<HostContentSettingsMap, DanglingUntriaged> host_content_settings_map_;

  ContentSetting GetContentSetting(const GURL& rp_url) {
    return host_content_settings_map_->GetContentSetting(
        rp_url, rp_url,
        ContentSettingsType::FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION);
  }

  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfile> profile_;
};

// Test that FedCM auto re-authn is opt-in by default.
TEST_F(FederatedIdentityAutoReauthnPermissionContextTest,
       AutoReauthnEnabledByDefault) {
  GURL rp_url("https://rp.com");
  EXPECT_TRUE(context_->IsAutoReauthnSettingEnabled());
  EXPECT_FALSE(context_->IsAutoReauthnEmbargoed(url::Origin::Create(rp_url)));
}

// Test that
// FederatedIdentityAutoReauthnPermissionContext::RecordEmbargoForAutoReauthn()
// blocks the permission if it is enabled.
TEST_F(FederatedIdentityAutoReauthnPermissionContextTest, EnabledEmbargo) {
  GURL rp_url("https://rp.com");
  EXPECT_FALSE(context_->IsAutoReauthnEmbargoed(url::Origin::Create(rp_url)));

  // Embargoing `rp_url` should block the content setting for `rp_url`.
  context_->RecordEmbargoForAutoReauthn(url::Origin::Create(rp_url));
  EXPECT_TRUE(context_->IsAutoReauthnEmbargoed(url::Origin::Create(rp_url)));
}

// Test that auto re-authn embargo only lasts for 10 mins (see
// |kFederatedIdentityAutoReauthnEmbargoDuration| defined in
// components/permissions/permission_decision_auto_blocker.cc)
TEST_F(FederatedIdentityAutoReauthnPermissionContextTest, EmbargoAutoReset) {
  GURL rp_url("https://rp.com");
  EXPECT_FALSE(context_->IsAutoReauthnEmbargoed(url::Origin::Create(rp_url)));

  // Auto re-authn flow triggers embargo.
  context_->RecordEmbargoForAutoReauthn(url::Origin::Create(rp_url));
  EXPECT_TRUE(context_->IsAutoReauthnEmbargoed(url::Origin::Create(rp_url)));

  // Auto re-authn is still in embargo state after 9 mins.
  task_environment()->FastForwardBy(base::Minutes(9));
  EXPECT_TRUE(context_->IsAutoReauthnEmbargoed(url::Origin::Create(rp_url)));

  // Auto re-authn is no longer in embargo state after 11 mins.
  task_environment()->FastForwardBy(base::Minutes(2));
  EXPECT_FALSE(context_->IsAutoReauthnEmbargoed(url::Origin::Create(rp_url)));
}

// Test that embargo reset does not affect the `RequiresUserMediation` bit.
TEST_F(FederatedIdentityAutoReauthnPermissionContextTest,
       EmbargoResetDoesNotAffectRequiresUserMediation) {
  GURL rp_url("https://rp.com");
  host_content_settings_map_->SetDefaultContentSetting(
      ContentSettingsType::FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION,
      CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW, GetContentSetting(rp_url));
  EXPECT_FALSE(context_->IsAutoReauthnEmbargoed(url::Origin::Create(rp_url)));

  // Auto re-authn flow triggers embargo.
  context_->RecordEmbargoForAutoReauthn(url::Origin::Create(rp_url));
  EXPECT_TRUE(context_->IsAutoReauthnEmbargoed(url::Origin::Create(rp_url)));

  // User signing out sets the `RequiresUserMediation` bit.
  context_->SetRequiresUserMediation(url::Origin::Create(rp_url), true);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetContentSetting(rp_url));

  // Auto re-authn is still in embargo state.
  EXPECT_TRUE(context_->IsAutoReauthnEmbargoed(url::Origin::Create(rp_url)));

  // Auto re-authn is no longer in embargo state after 10 mins.
  task_environment()->FastForwardBy(base::Minutes(10));
  EXPECT_FALSE(context_->IsAutoReauthnEmbargoed(url::Origin::Create(rp_url)));

  // The`RequiresUserMediation` bit is not reset.
  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetContentSetting(rp_url));
}

// Test that auto re-authn embargo can be reset.
TEST_F(FederatedIdentityAutoReauthnPermissionContextTest, ResetEmbargo) {
  GURL rp_url("https://rp.com");

  // Auto re-authn flow triggers embargo.
  context_->RecordEmbargoForAutoReauthn(url::Origin::Create(rp_url));
  EXPECT_TRUE(context_->IsAutoReauthnEmbargoed(url::Origin::Create(rp_url)));

  context_->RemoveEmbargoForAutoReauthn(url::Origin::Create(rp_url));
  EXPECT_FALSE(context_->IsAutoReauthnEmbargoed(url::Origin::Create(rp_url)));
}
