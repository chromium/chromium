// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_auto_signin_permission_context.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/webid/federated_identity_auto_signin_permission_context_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class FederatedIdentityAutoSigninPermissionContextTest : public testing::Test {
 public:
  FederatedIdentityAutoSigninPermissionContextTest() = default;
  ~FederatedIdentityAutoSigninPermissionContextTest() override = default;
  FederatedIdentityAutoSigninPermissionContextTest(
      FederatedIdentityAutoSigninPermissionContextTest&) = delete;
  FederatedIdentityAutoSigninPermissionContextTest& operator=(
      FederatedIdentityAutoSigninPermissionContextTest&) = delete;

  void SetUp() override {
    context_ =
        FederatedIdentityAutoSigninPermissionContextFactory::GetForProfile(
            &profile_);
    host_content_settings_map_ =
        HostContentSettingsMapFactory::GetForProfile(&profile_);
  }

  Profile* profile() { return &profile_; }

 protected:
  base::raw_ptr<FederatedIdentityAutoSigninPermissionContext> context_;
  base::raw_ptr<HostContentSettingsMap> host_content_settings_map_;

  ContentSetting GetContentSetting() {
    return host_content_settings_map_->GetDefaultContentSetting(
        ContentSettingsType::FEDERATED_IDENTITY_AUTO_SIGNIN_PERMISSION,
        nullptr);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

// Test that FedCM auto sign-in is opt-in by default.
TEST_F(FederatedIdentityAutoSigninPermissionContextTest,
       AutoSigninEnabledByDefault) {
  EXPECT_EQ(true, context_->HasAutoSigninPermission());
}
