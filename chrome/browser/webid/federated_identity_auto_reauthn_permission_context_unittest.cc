// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_auto_reauthn_permission_context.h"

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/trusted_vault/trusted_vault_service_factory.h"
#include "chrome/browser/webid/federated_identity_auto_reauthn_permission_context_factory.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/password_manager/core/browser/fake_password_manager_settings_service.h"
#include "components/password_manager/core/browser/password_manager_setting.h"
#include "components/password_manager/core/browser/password_manager_settings_service.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
CreateAndRegisterPrefs() {
  auto pref_service =
      std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
  HostContentSettingsMap::RegisterProfilePrefs(pref_service->registry());
  return pref_service;
}

class FederatedIdentityAutoReauthnPermissionContextTest : public testing::Test {
 public:
  FederatedIdentityAutoReauthnPermissionContextTest() {
    context_.OnPasswordManagerSettingsServiceInitialized(
        &password_manager_settings_service_);
  }

  FederatedIdentityAutoReauthnPermissionContextTest(
      const FederatedIdentityAutoReauthnPermissionContextTest&) = delete;
  FederatedIdentityAutoReauthnPermissionContextTest& operator=(
      const FederatedIdentityAutoReauthnPermissionContextTest&) = delete;

  ~FederatedIdentityAutoReauthnPermissionContextTest() override {
    context_.Shutdown();
    host_content_settings_map_->ShutdownOnUIThread();
  }

  FederatedIdentityAutoReauthnPermissionContext& context() { return context_; }

  HostContentSettingsMap& host_content_settings_map() {
    return *host_content_settings_map_;
  }

  ContentSetting GetContentSetting(const GURL& rp_url) {
    return host_content_settings_map().GetContentSetting(
        rp_url, rp_url,
        ContentSettingsType::FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION);
  }

  base::test::SingleThreadTaskEnvironment* task_environment() {
    return &task_environment_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_ =
      CreateAndRegisterPrefs();
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_ =
      base::MakeRefCounted<HostContentSettingsMap>(
          pref_service_.get(),
          /*is_off_the_record=*/false,
          /*store_last_modified=*/false,
          /*restore_session=*/false,
          /*should_record_metrics=*/false);
  permissions::PermissionDecisionAutoBlocker permission_decision_auto_blocker_{
      host_content_settings_map_.get()};
  FederatedIdentityAutoReauthnPermissionContext context_{
      host_content_settings_map_.get(), &permission_decision_auto_blocker_};
  password_manager::FakePasswordManagerSettingsService
      password_manager_settings_service_;
};

// Test that FedCM auto re-authn is opt-in by default.
TEST_F(FederatedIdentityAutoReauthnPermissionContextTest,
       AutoReauthnEnabledByDefault) {
  GURL rp_url("https://rp.com");
  EXPECT_TRUE(context().IsAutoReauthnSettingEnabled());
  EXPECT_FALSE(context().IsAutoReauthnEmbargoed(url::Origin::Create(rp_url)));
}

// Test that
// FederatedIdentityAutoReauthnPermissionContext::RecordEmbargoForAutoReauthn()
// blocks the permission if it is enabled.
TEST_F(FederatedIdentityAutoReauthnPermissionContextTest, EnabledEmbargo) {
  GURL rp_url("https://rp.com");
  EXPECT_FALSE(context().IsAutoReauthnEmbargoed(url::Origin::Create(rp_url)));

  // Embargoing `rp_url` should block the content setting for `rp_url`.
  context().RecordEmbargoForAutoReauthn(url::Origin::Create(rp_url));
  EXPECT_TRUE(context().IsAutoReauthnEmbargoed(url::Origin::Create(rp_url)));
}

// Test that auto re-authn embargo only lasts for 10 mins (see
// |kFederatedIdentityAutoReauthnEmbargoDuration| defined in
// components/permissions/permission_decision_auto_blocker.cc)
TEST_F(FederatedIdentityAutoReauthnPermissionContextTest, EmbargoAutoReset) {
  GURL rp_url("https://rp.com");
  EXPECT_FALSE(context().IsAutoReauthnEmbargoed(url::Origin::Create(rp_url)));

  // Auto re-authn flow triggers embargo.
  context().RecordEmbargoForAutoReauthn(url::Origin::Create(rp_url));
  EXPECT_TRUE(context().IsAutoReauthnEmbargoed(url::Origin::Create(rp_url)));

  // Auto re-authn is still in embargo state after 9 mins.
  task_environment()->FastForwardBy(base::Minutes(9));
  EXPECT_TRUE(context().IsAutoReauthnEmbargoed(url::Origin::Create(rp_url)));

  // Auto re-authn is no longer in embargo state after 11 mins.
  task_environment()->FastForwardBy(base::Minutes(2));
  EXPECT_FALSE(context().IsAutoReauthnEmbargoed(url::Origin::Create(rp_url)));
}

// Test that embargo reset does not affect the `RequiresUserMediation` bit.
TEST_F(FederatedIdentityAutoReauthnPermissionContextTest,
       EmbargoResetDoesNotAffectRequiresUserMediation) {
  GURL rp_url("https://rp.com");
  host_content_settings_map().SetDefaultContentSetting(
      ContentSettingsType::FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION,
      CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW, GetContentSetting(rp_url));
  EXPECT_FALSE(context().IsAutoReauthnEmbargoed(url::Origin::Create(rp_url)));

  // Auto re-authn flow triggers embargo.
  context().RecordEmbargoForAutoReauthn(url::Origin::Create(rp_url));
  EXPECT_TRUE(context().IsAutoReauthnEmbargoed(url::Origin::Create(rp_url)));

  // User signing out sets the `RequiresUserMediation` bit.
  context().SetRequiresUserMediation(url::Origin::Create(rp_url), true);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetContentSetting(rp_url));

  // Auto re-authn is still in embargo state.
  EXPECT_TRUE(context().IsAutoReauthnEmbargoed(url::Origin::Create(rp_url)));

  // Auto re-authn is no longer in embargo state after 10 mins.
  task_environment()->FastForwardBy(base::Minutes(10));
  EXPECT_FALSE(context().IsAutoReauthnEmbargoed(url::Origin::Create(rp_url)));

  // The`RequiresUserMediation` bit is not reset.
  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetContentSetting(rp_url));
}

// Test that auto re-authn embargo can be reset.
TEST_F(FederatedIdentityAutoReauthnPermissionContextTest, ResetEmbargo) {
  GURL rp_url("https://rp.com");

  // Auto re-authn flow triggers embargo.
  context().RecordEmbargoForAutoReauthn(url::Origin::Create(rp_url));
  EXPECT_TRUE(context().IsAutoReauthnEmbargoed(url::Origin::Create(rp_url)));

  context().RemoveEmbargoForAutoReauthn(url::Origin::Create(rp_url));
  EXPECT_FALSE(context().IsAutoReauthnEmbargoed(url::Origin::Create(rp_url)));
}

}  // namespace
