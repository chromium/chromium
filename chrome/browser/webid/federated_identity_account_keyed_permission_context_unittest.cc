// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_account_keyed_permission_context.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class FederatedIdentityAccountKeyedPermissionContextTest
    : public testing::Test {
 public:
  FederatedIdentityAccountKeyedPermissionContextTest() {
    context_ = std::make_unique<FederatedIdentityAccountKeyedPermissionContext>(
        &profile_, ContentSettingsType::FEDERATED_IDENTITY_SHARING,
        "identity-provider");
  }

  void TearDown() override { context_.reset(); }

  ~FederatedIdentityAccountKeyedPermissionContextTest() override = default;

  FederatedIdentityAccountKeyedPermissionContextTest(
      FederatedIdentityAccountKeyedPermissionContextTest&) = delete;
  FederatedIdentityAccountKeyedPermissionContextTest& operator=(
      FederatedIdentityAccountKeyedPermissionContextTest&) = delete;

  FederatedIdentityAccountKeyedPermissionContext* context() {
    return context_.get();
  }
  TestingProfile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<FederatedIdentityAccountKeyedPermissionContext> context_;
  TestingProfile profile_;
};

TEST_F(FederatedIdentityAccountKeyedPermissionContextTest,
       GrantAndRevokeAccountSpecificGenericPermission) {
  const auto rp = url::Origin::Create(GURL("https://rp.example"));
  const auto idp = url::Origin::Create(GURL("https://idp.example"));
  std::string account{"consetogo"};

  EXPECT_FALSE(context()->HasPermission(rp, idp, account));

  context()->GrantPermission(rp, idp, account);
  EXPECT_TRUE(context()->HasPermission(rp, idp, account));

  context()->RevokePermission(rp, idp, account);
  EXPECT_FALSE(context()->HasPermission(rp, idp, account));
}

// Ensure the context can handle multiple accounts per RP/IdP origin.
TEST_F(FederatedIdentityAccountKeyedPermissionContextTest,
       GrantTwoAccountSpecificPermissionsAndRevokeThem) {
  const auto rp = url::Origin::Create(GURL("https://rp.example"));
  const auto idp = url::Origin::Create(GURL("https://idp.example"));
  std::string account_a{"consetogo"};
  std::string account_b{"woolwich"};

  EXPECT_FALSE(context()->HasPermission(rp, idp, account_a));
  EXPECT_FALSE(context()->HasPermission(rp, idp, account_b));

  context()->GrantPermission(rp, idp, account_a);
  EXPECT_TRUE(context()->HasPermission(rp, idp, account_a));
  EXPECT_FALSE(context()->HasPermission(rp, idp, account_b));

  context()->GrantPermission(rp, idp, account_b);
  EXPECT_TRUE(context()->HasPermission(rp, idp, account_a));
  EXPECT_TRUE(context()->HasPermission(rp, idp, account_b));

  context()->RevokePermission(rp, idp, account_a);
  EXPECT_FALSE(context()->HasPermission(rp, idp, account_a));
  EXPECT_TRUE(context()->HasPermission(rp, idp, account_b));

  context()->RevokePermission(rp, idp, account_b);
  EXPECT_FALSE(context()->HasPermission(rp, idp, account_a));
  EXPECT_FALSE(context()->HasPermission(rp, idp, account_b));
}

// Test granting permissions for multiple IDPs mapped to the same RP and
// multiple RPs mapped to the same IDP.
TEST_F(FederatedIdentityAccountKeyedPermissionContextTest,
       GrantPermissionsMultipleRpsMultipleIdps) {
  const auto rp1 = url::Origin::Create(GURL("https://rp1.example"));
  const auto rp2 = url::Origin::Create(GURL("https://rp2.example"));
  const auto idp1 = url::Origin::Create(GURL("https://idp1.example"));
  const auto idp2 = url::Origin::Create(GURL("https://idp2.example"));

  context()->GrantPermission(rp1, idp1, "consestogo");
  context()->GrantPermission(rp1, idp2, "woolwich");
  context()->GrantPermission(rp2, idp1, "wilmot");

  EXPECT_EQ(3u, context()->GetAllGrantedObjects().size());
  EXPECT_EQ(2u, context()->GetGrantedObjects(rp1).size());
  EXPECT_EQ(1u, context()->GetGrantedObjects(rp2).size());
}

// Test that granting a permission for an account, if the permission has already
// been granted, is a noop.
TEST_F(FederatedIdentityAccountKeyedPermissionContextTest,
       GrantPermissionForSameAccount) {
  const auto rp = url::Origin::Create(GURL("https://rp.example"));
  const auto idp = url::Origin::Create(GURL("https://idp.example"));
  std::string account{"consetogo"};

  EXPECT_FALSE(context()->HasPermission(rp, idp, account));

  context()->GrantPermission(rp, idp, account);
  context()->GrantPermission(rp, idp, account);
  EXPECT_TRUE(context()->HasPermission(rp, idp, account));
  auto granted_object = context()->GetGrantedObject(rp, idp.Serialize());
  EXPECT_EQ(1u,
            granted_object->value.GetDict().FindList("account-ids")->size());
}
