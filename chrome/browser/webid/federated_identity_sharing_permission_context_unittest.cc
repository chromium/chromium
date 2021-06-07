// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_sharing_permission_context.h"

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
  FederatedIdentitySharingPermissionContext* context_;
  TestingProfile profile_;
};

TEST_F(FederatedIdentitySharingPermissionContextTest,
       GrantAndRevokeSinglePermission) {
  const auto rp_origin = url::Origin::Create(GURL("https://rp.example"));
  const auto idp_origin = url::Origin::Create(GURL("https://idp.example"));

  EXPECT_FALSE(context()->HasSharingPermission(idp_origin, rp_origin));

  context()->GrantSharingPermission(idp_origin, rp_origin);
  EXPECT_TRUE(context()->HasSharingPermission(idp_origin, rp_origin));

  context()->RevokeSharingPermission(idp_origin, rp_origin);
  EXPECT_FALSE(context()->HasSharingPermission(idp_origin, rp_origin));
}

// Ensure the context can handle multiple RPs per IdP origin.
TEST_F(FederatedIdentitySharingPermissionContextTest,
       GrantTwoPermissionsAndRevokeOne) {
  const auto rp_origin1 = url::Origin::Create(GURL("https://rp1.example"));
  const auto rp_origin2 = url::Origin::Create(GURL("https://rp2.example"));
  const auto idp_origin = url::Origin::Create(GURL("https://idp.example"));

  EXPECT_FALSE(context()->HasSharingPermission(idp_origin, rp_origin1));
  EXPECT_FALSE(context()->HasSharingPermission(idp_origin, rp_origin2));

  context()->GrantSharingPermission(idp_origin, rp_origin1);
  EXPECT_TRUE(context()->HasSharingPermission(idp_origin, rp_origin1));
  EXPECT_FALSE(context()->HasSharingPermission(idp_origin, rp_origin2));
  context()->GrantSharingPermission(idp_origin, rp_origin2);
  EXPECT_TRUE(context()->HasSharingPermission(idp_origin, rp_origin1));
  EXPECT_TRUE(context()->HasSharingPermission(idp_origin, rp_origin2));

  context()->RevokeSharingPermission(idp_origin, rp_origin1);
  EXPECT_FALSE(context()->HasSharingPermission(idp_origin, rp_origin1));
  EXPECT_TRUE(context()->HasSharingPermission(idp_origin, rp_origin2));
}
