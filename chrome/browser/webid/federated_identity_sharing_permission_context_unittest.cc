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

// Test granting permissions for multiple IDPs mapped to the same RP and
// multiple RPs mapped to the same IDP.
TEST_F(FederatedIdentitySharingPermissionContextTest,
       GrantPermissionsMultipleRpsMultipleIdps) {
  const auto rp1 = url::Origin::Create(GURL("https://rp1.example"));
  const auto rp2 = url::Origin::Create(GURL("https://rp2.example"));
  const auto idp1 = url::Origin::Create(GURL("https://idp1.example"));
  const auto idp2 = url::Origin::Create(GURL("https://idp2.example"));

  context()->GrantSharingPermission(rp1, idp1, "consestogo");
  context()->GrantSharingPermission(rp1, idp2, "woolwich");
  context()->GrantSharingPermission(rp2, idp1, "wilmot");

  EXPECT_EQ(3u, context()->GetAllGrantedObjects().size());
  EXPECT_EQ(2u, context()->GetGrantedObjects(rp1).size());
  EXPECT_EQ(1u, context()->GetGrantedObjects(rp2).size());
}

// Test that granting a permission for an account, if the permission has already
// been granted, is a noop.
TEST_F(FederatedIdentitySharingPermissionContextTest,
       GrantPermissionForSameAccount) {
  const auto rp = url::Origin::Create(GURL("https://rp.example"));
  const auto idp = url::Origin::Create(GURL("https://idp.example"));
  std::string account{"consetogo"};

  EXPECT_FALSE(context()->HasSharingPermission(rp, idp, account));

  context()->GrantSharingPermission(rp, idp, account);
  context()->GrantSharingPermission(rp, idp, account);
  EXPECT_TRUE(context()->HasSharingPermission(rp, idp, account));
  auto granted_object = context()->GetGrantedObject(rp, idp.Serialize());
  EXPECT_EQ(1u,
            granted_object->value.GetDict().FindList("account-ids")->size());
}
