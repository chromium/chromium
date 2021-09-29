// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_request_permission_context.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/webid/federated_identity_request_permission_context_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class FederatedIdentityRequestPermissionContextTest : public testing::Test {
 public:
  FederatedIdentityRequestPermissionContextTest() {
    context_ = FederatedIdentityRequestPermissionContextFactory::GetForProfile(
        &profile_);
  }

  ~FederatedIdentityRequestPermissionContextTest() override = default;

  FederatedIdentityRequestPermissionContextTest(
      FederatedIdentityRequestPermissionContextTest&) = delete;
  FederatedIdentityRequestPermissionContextTest& operator=(
      FederatedIdentityRequestPermissionContextTest&) = delete;

  FederatedIdentityRequestPermissionContext* context() { return context_; }
  TestingProfile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<FederatedIdentityRequestPermissionContext> context_;
  TestingProfile profile_;
};

TEST_F(FederatedIdentityRequestPermissionContextTest,
       GrantAndRevokeSinglePermission) {
  const auto rp_origin = url::Origin::Create(GURL("https://rp.example"));
  const auto idp_origin = url::Origin::Create(GURL("https://idp.example"));

  EXPECT_FALSE(context()->HasRequestPermission(rp_origin, idp_origin));

  context()->GrantRequestPermission(rp_origin, idp_origin);
  EXPECT_TRUE(context()->HasRequestPermission(rp_origin, idp_origin));

  context()->RevokeRequestPermission(rp_origin, idp_origin);
  EXPECT_FALSE(context()->HasRequestPermission(rp_origin, idp_origin));
}

// Ensure the context can handle multiple IdPs per RP origin.
TEST_F(FederatedIdentityRequestPermissionContextTest,
       GrantTwoPermissionsAndRevokeOne) {
  const auto rp_origin = url::Origin::Create(GURL("https://rp.example"));
  const auto idp_origin1 = url::Origin::Create(GURL("https://idp1.example"));
  const auto idp_origin2 = url::Origin::Create(GURL("https://idp2.example"));

  EXPECT_FALSE(context()->HasRequestPermission(rp_origin, idp_origin1));
  EXPECT_FALSE(context()->HasRequestPermission(rp_origin, idp_origin2));

  context()->GrantRequestPermission(rp_origin, idp_origin1);
  EXPECT_TRUE(context()->HasRequestPermission(rp_origin, idp_origin1));
  EXPECT_FALSE(context()->HasRequestPermission(rp_origin, idp_origin2));
  context()->GrantRequestPermission(rp_origin, idp_origin2);
  EXPECT_TRUE(context()->HasRequestPermission(rp_origin, idp_origin1));
  EXPECT_TRUE(context()->HasRequestPermission(rp_origin, idp_origin2));

  context()->RevokeRequestPermission(rp_origin, idp_origin1);
  EXPECT_FALSE(context()->HasRequestPermission(rp_origin, idp_origin1));
  EXPECT_TRUE(context()->HasRequestPermission(rp_origin, idp_origin2));
}
