// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_active_session_permission_context.h"

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/webid/federated_identity_active_session_permission_context_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class FederatedIdentityActiveSessionPermissionContextTest
    : public testing::Test {
 public:
  FederatedIdentityActiveSessionPermissionContextTest() {
    context_ =
        FederatedIdentityActiveSessionPermissionContextFactory::GetForProfile(
            &profile_);
  }

  ~FederatedIdentityActiveSessionPermissionContextTest() override = default;

  FederatedIdentityActiveSessionPermissionContextTest(
      FederatedIdentityActiveSessionPermissionContextTest&) = delete;
  FederatedIdentityActiveSessionPermissionContextTest& operator=(
      FederatedIdentityActiveSessionPermissionContextTest&) = delete;

  FederatedIdentityActiveSessionPermissionContext* context() {
    return context_;
  }
  TestingProfile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<FederatedIdentityActiveSessionPermissionContext> context_;
  TestingProfile profile_;
};

TEST_F(FederatedIdentityActiveSessionPermissionContextTest,
       GrantAndRevokeSinglePermission) {
  const auto rp_origin = url::Origin::Create(GURL("https://rp.example"));
  const auto idp_origin = url::Origin::Create(GURL("https://idp.example"));
  std::string account_id = "account123";

  EXPECT_FALSE(context()->HasActiveSession(rp_origin, idp_origin, account_id));

  context()->GrantActiveSession(rp_origin, idp_origin, account_id);
  EXPECT_TRUE(context()->HasActiveSession(rp_origin, idp_origin, account_id));

  context()->RevokeActiveSession(rp_origin, idp_origin, account_id);
  EXPECT_FALSE(context()->HasActiveSession(rp_origin, idp_origin, account_id));
}

// Ensure the context can handle multiple IdPs per RP origin.
TEST_F(FederatedIdentityActiveSessionPermissionContextTest,
       GrantTwoPermissionsAndRevokeOne) {
  const auto rp_origin = url::Origin::Create(GURL("https://rp.example"));
  const auto idp_origin1 = url::Origin::Create(GURL("https://idp1.example"));
  const auto idp_origin2 = url::Origin::Create(GURL("https://idp2.example"));
  std::string account_id = "account123";

  EXPECT_FALSE(context()->HasActiveSession(rp_origin, idp_origin1, account_id));
  EXPECT_FALSE(context()->HasActiveSession(rp_origin, idp_origin2, account_id));

  context()->GrantActiveSession(rp_origin, idp_origin1, account_id);
  EXPECT_TRUE(context()->HasActiveSession(rp_origin, idp_origin1, account_id));
  EXPECT_FALSE(context()->HasActiveSession(rp_origin, idp_origin2, account_id));

  context()->GrantActiveSession(rp_origin, idp_origin2, account_id);
  EXPECT_TRUE(context()->HasActiveSession(rp_origin, idp_origin1, account_id));
  EXPECT_TRUE(context()->HasActiveSession(rp_origin, idp_origin2, account_id));

  context()->RevokeActiveSession(rp_origin, idp_origin1, account_id);
  EXPECT_FALSE(context()->HasActiveSession(rp_origin, idp_origin1, account_id));
  EXPECT_TRUE(context()->HasActiveSession(rp_origin, idp_origin2, account_id));
}

// Ensure the context can handle multiple accounts per RP/IdP pair.
TEST_F(FederatedIdentityActiveSessionPermissionContextTest,
       MultipleAccountsWithSameIdp) {
  const auto rp_origin = url::Origin::Create(GURL("https://rp.example"));
  const auto idp_origin = url::Origin::Create(GURL("https://idp.example"));
  std::string account_id1 = "account123";
  std::string account_id2 = "account456";

  EXPECT_FALSE(context()->HasActiveSession(rp_origin, idp_origin, account_id1));
  EXPECT_FALSE(context()->HasActiveSession(rp_origin, idp_origin, account_id2));

  context()->GrantActiveSession(rp_origin, idp_origin, account_id1);
  EXPECT_TRUE(context()->HasActiveSession(rp_origin, idp_origin, account_id1));
  EXPECT_FALSE(context()->HasActiveSession(rp_origin, idp_origin, account_id2));

  context()->GrantActiveSession(rp_origin, idp_origin, account_id2);
  EXPECT_TRUE(context()->HasActiveSession(rp_origin, idp_origin, account_id1));
  EXPECT_TRUE(context()->HasActiveSession(rp_origin, idp_origin, account_id2));

  context()->RevokeActiveSession(rp_origin, idp_origin, account_id1);
  EXPECT_FALSE(context()->HasActiveSession(rp_origin, idp_origin, account_id1));
  EXPECT_TRUE(context()->HasActiveSession(rp_origin, idp_origin, account_id2));

  context()->RevokeActiveSession(rp_origin, idp_origin, account_id2);
  EXPECT_FALSE(context()->HasActiveSession(rp_origin, idp_origin, account_id1));
  EXPECT_FALSE(context()->HasActiveSession(rp_origin, idp_origin, account_id2));
}

// Ensure duplicate sessions cannot be added.
TEST_F(FederatedIdentityActiveSessionPermissionContextTest,
       AddingDuplicateSessionIsIgnored) {
  const auto rp_origin = url::Origin::Create(GURL("https://rp.example"));
  const auto idp_origin = url::Origin::Create(GURL("https://idp.example"));
  std::string account_id = "account123";

  EXPECT_FALSE(context()->HasActiveSession(rp_origin, idp_origin, account_id));

  context()->GrantActiveSession(rp_origin, idp_origin, account_id);
  EXPECT_TRUE(context()->HasActiveSession(rp_origin, idp_origin, account_id));

  context()->GrantActiveSession(rp_origin, idp_origin, account_id);
  EXPECT_TRUE(context()->HasActiveSession(rp_origin, idp_origin, account_id));

  // Revoking a single time after adding twice should result in the session
  // being removed.
  context()->RevokeActiveSession(rp_origin, idp_origin, account_id);
  EXPECT_FALSE(context()->HasActiveSession(rp_origin, idp_origin, account_id));
}
