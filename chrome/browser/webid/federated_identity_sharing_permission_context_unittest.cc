// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_sharing_permission_context.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/webid/federated_identity_sharing_permission_context_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class FederatedIdentitySharingPermissionContextTest : public testing::Test {
 public:
  FederatedIdentitySharingPermissionContextTest() {
    context_ = FederatedIdentitySharingPermissionContextFactory::GetForProfile(
        &profile_);
  }

  ~FederatedIdentitySharingPermissionContextTest() override = default;

  FederatedIdentitySharingPermissionContextTest(
      FederatedIdentitySharingPermissionContextTest&) = delete;
  FederatedIdentitySharingPermissionContextTest& operator=(
      FederatedIdentitySharingPermissionContextTest&) = delete;

  FederatedIdentitySharingPermissionContext* context() { return context_; }
  TestingProfile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<FederatedIdentitySharingPermissionContext> context_;
  TestingProfile profile_;
};

TEST_F(FederatedIdentitySharingPermissionContextTest,
       GrantAndRevokeAccountSpecificGenericPermission) {
  const auto rp = url::Origin::Create(GURL("https://rp.example"));
  const auto idp = url::Origin::Create(GURL("https://idp.example"));
  std::string account{"consetogo"};

  EXPECT_FALSE(context()->HasSharingPermission(rp, idp, account));

  context()->GrantSharingPermission(rp, idp, account);
  EXPECT_TRUE(context()->HasSharingPermission(rp, idp, account));

  context()->RevokeSharingPermission(rp, idp, account);
  EXPECT_FALSE(context()->HasSharingPermission(rp, idp, account));
}

// Ensure the context can handle multiple accounts per RP/IdP origin.
TEST_F(FederatedIdentitySharingPermissionContextTest,
       GrantTwoAccountSpecificPermissionsAndRevokeThem) {
  const auto rp = url::Origin::Create(GURL("https://rp.example"));
  const auto idp = url::Origin::Create(GURL("https://idp.example"));
  std::string account_a{"consetogo"};
  std::string account_b{"woolwich"};

  EXPECT_FALSE(context()->HasSharingPermission(rp, idp, account_a));
  EXPECT_FALSE(context()->HasSharingPermission(rp, idp, account_b));

  context()->GrantSharingPermission(rp, idp, account_a);
  EXPECT_TRUE(context()->HasSharingPermission(rp, idp, account_a));
  EXPECT_FALSE(context()->HasSharingPermission(rp, idp, account_b));

  context()->GrantSharingPermission(rp, idp, account_b);
  EXPECT_TRUE(context()->HasSharingPermission(rp, idp, account_a));
  EXPECT_TRUE(context()->HasSharingPermission(rp, idp, account_b));

  context()->RevokeSharingPermission(rp, idp, account_a);
  EXPECT_FALSE(context()->HasSharingPermission(rp, idp, account_a));
  EXPECT_TRUE(context()->HasSharingPermission(rp, idp, account_b));

  context()->RevokeSharingPermission(rp, idp, account_b);
  EXPECT_FALSE(context()->HasSharingPermission(rp, idp, account_a));
  EXPECT_FALSE(context()->HasSharingPermission(rp, idp, account_b));
}
