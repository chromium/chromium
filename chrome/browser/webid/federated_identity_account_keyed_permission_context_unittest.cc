// Copyright 2022 The Chromium Authors
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
#include "url/origin.h"

namespace {

const char kTestIdpOriginKey[] = "identity-provider";

}

class FederatedIdentityAccountKeyedPermissionContextTest
    : public testing::Test {
 public:
  FederatedIdentityAccountKeyedPermissionContextTest() {
    context_ = std::make_unique<FederatedIdentityAccountKeyedPermissionContext>(
        &profile_, ContentSettingsType::FEDERATED_IDENTITY_SHARING,
        kTestIdpOriginKey);
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

// Test that the key is identical reglardless of whether the permission was
// stored using the old format (prior to the relying-party-embedder key being
// added) or the new format.
TEST_F(FederatedIdentityAccountKeyedPermissionContextTest,
       VerifyKeyRequesterAndEmbedderIdentical) {
  const url::Origin rp = url::Origin::Create(GURL("https://rp.example"));
  const url::Origin idp = url::Origin::Create(GURL("https://idp.example"));
  const std::string account("consetogo");

  // Old Format
  {
    base::Value::Dict new_object;
    new_object.Set(kTestIdpOriginKey, idp.Serialize());
    base::Value::List account_list;
    account_list.Append(account);
    new_object.Set("account-ids", base::Value(std::move(account_list)));
    context()->GrantObjectPermission(rp, std::move(new_object));
  }
  auto granted_objects = context()->GetAllGrantedObjects();
  EXPECT_EQ(1u, granted_objects.size());
  std::string key_old_format =
      context()->GetKeyForObject(granted_objects[0]->value);

  // Cleanup
  context()->RevokePermission(rp, rp, idp, account);
  EXPECT_TRUE(context()->GetAllGrantedObjects().empty());

  // New format
  context()->GrantPermission(rp, rp, idp, account);
  granted_objects = context()->GetAllGrantedObjects();
  EXPECT_EQ(1u, granted_objects.size());
  std::string key_new_format =
      context()->GetKeyForObject(granted_objects[0]->value);

  EXPECT_EQ(key_old_format, key_new_format);
}

// Test that '<' in the url::Origin parameters passed to
// FederatedIdentityAccountKeyedPermissionContext::GrantPermission() are
// escaped.
// '<' is used as a separator in
// FederatedIdentityAccountKeyedPermissionContextTest::GetKeyForObject().
TEST_F(FederatedIdentityAccountKeyedPermissionContextTest, VerifyKeySeparator) {
  const url::Origin rp = url::Origin::Create(GURL("https://rp<.example</<?<"));
  const url::Origin idp =
      url::Origin::Create(GURL("https://idp<.example</<?<"));
  const url::Origin rp_embedder =
      url::Origin::Create(GURL("https://rp-embedder<.example</<?<"));
  const std::string account("consetogo");

  context()->GrantPermission(rp, rp_embedder, idp, account);
  auto granted_objects = context()->GetAllGrantedObjects();
  EXPECT_EQ(1u, granted_objects.size());
  std::string key = context()->GetKeyForObject(granted_objects[0]->value);
  EXPECT_EQ("https://idp%3C.example%3C<https://rp-embedder%3C.example%3C", key);
}

// Test calling
// FederatedIdentityAccountKeyedPermissionContextTest::HasPermission() for
// permissions stored with the pre-relying-party-embedder-format.
TEST_F(FederatedIdentityAccountKeyedPermissionContextTest,
       CompatibleWithOldFormat) {
  const url::Origin rp = url::Origin::Create(GURL("https://rp.example"));
  const url::Origin idp1 = url::Origin::Create(GURL("https://idp1.example"));
  const url::Origin idp2 = url::Origin::Create(GURL("https://idp2.example"));
  const url::Origin other_origin =
      url::Origin::Create(GURL("https://other.example"));
  const std::string account_a("conestogo");
  const std::string account_b("woolwich");
  const std::string account_c("wellesley");

  {
    base::Value::Dict new_object;
    new_object.Set(kTestIdpOriginKey, idp1.Serialize());
    base::Value::List account_list;
    account_list.Append(account_a);
    account_list.Append(account_b);
    new_object.Set("account-ids", base::Value(std::move(account_list)));
    context()->GrantObjectPermission(rp, std::move(new_object));
  }
  {
    base::Value::Dict new_object;
    new_object.Set(kTestIdpOriginKey, idp2.Serialize());
    base::Value::List account_list;
    account_list.Append(account_c);
    new_object.Set("account-ids", base::Value(std::move(account_list)));
    context()->GrantObjectPermission(rp, std::move(new_object));
  }

  // Permissions in the old format should only be returned when
  // relying-party-requester == relying-party-embedder.
  EXPECT_TRUE(context()->HasPermission(rp, rp, idp1, account_a));
  EXPECT_TRUE(context()->HasPermission(rp, rp, idp1, account_b));
  EXPECT_TRUE(context()->HasPermission(rp, rp, idp2, account_c));
  EXPECT_FALSE(context()->HasPermission(rp, other_origin, idp1, account_a));

  EXPECT_FALSE(context()->HasPermission(rp, rp, idp1, account_c));
}

namespace {

struct PermissionGrant {
  url::Origin relying_party_requester;
  url::Origin relying_party_embedder;
  url::Origin identity_provider;
  std::string account_id;
};

}  // anonymous namespace

void TestGrantAndRevoke(FederatedIdentityAccountKeyedPermissionContext* context,
                        const PermissionGrant& grant1,
                        const PermissionGrant& grant2) {
  context->GrantPermission(grant1.relying_party_requester,
                           grant1.relying_party_embedder,
                           grant1.identity_provider, grant1.account_id);

  EXPECT_TRUE(context->HasPermission(
      grant1.relying_party_requester, grant1.relying_party_embedder,
      grant1.identity_provider, grant1.account_id));
  EXPECT_FALSE(context->HasPermission(
      grant2.relying_party_requester, grant2.relying_party_embedder,
      grant2.identity_provider, grant2.account_id));

  context->GrantPermission(grant2.relying_party_requester,
                           grant2.relying_party_embedder,
                           grant2.identity_provider, grant2.account_id);

  EXPECT_TRUE(context->HasPermission(
      grant1.relying_party_requester, grant1.relying_party_embedder,
      grant1.identity_provider, grant1.account_id));
  EXPECT_TRUE(context->HasPermission(
      grant2.relying_party_requester, grant2.relying_party_embedder,
      grant2.identity_provider, grant2.account_id));

  context->RevokePermission(grant1.relying_party_requester,
                            grant1.relying_party_embedder,
                            grant1.identity_provider, grant1.account_id);
  EXPECT_FALSE(context->HasPermission(
      grant1.relying_party_requester, grant1.relying_party_embedder,
      grant1.identity_provider, grant1.account_id));
  EXPECT_TRUE(context->HasPermission(
      grant2.relying_party_requester, grant2.relying_party_embedder,
      grant2.identity_provider, grant2.account_id));

  context->RevokePermission(grant2.relying_party_requester,
                            grant2.relying_party_embedder,
                            grant2.identity_provider, grant2.account_id);
  EXPECT_FALSE(context->HasPermission(
      grant1.relying_party_requester, grant1.relying_party_embedder,
      grant1.identity_provider, grant1.account_id));
  EXPECT_FALSE(context->HasPermission(
      grant2.relying_party_requester, grant2.relying_party_embedder,
      grant2.identity_provider, grant2.account_id));

  EXPECT_TRUE(context->GetAllGrantedObjects().empty());
}

// Test granting and revoking a permission.
TEST_F(FederatedIdentityAccountKeyedPermissionContextTest, GrantAndRevoke) {
  const url::Origin rp_requester1 =
      url::Origin::Create(GURL("https://rp1.example"));
  const url::Origin rp_requester2 =
      url::Origin::Create(GURL("https://rp2.example"));
  const url::Origin rp_embedder1 =
      url::Origin::Create(GURL("https://rp-embedder1.example"));
  const url::Origin rp_embedder2 =
      url::Origin::Create(GURL("https://rp-embedder2.example"));
  const url::Origin idp1 = url::Origin::Create(GURL("https://idp1.example"));
  const url::Origin idp2 = url::Origin::Create(GURL("https://idp2.example"));
  const std::string account1("consetogo");
  const std::string account2("woolwich");

  TestGrantAndRevoke(context(), {rp_requester1, rp_embedder1, idp1, account1},
                     {rp_requester2, rp_embedder1, idp1, account1});
  TestGrantAndRevoke(context(), {rp_requester1, rp_embedder1, idp1, account1},
                     {rp_requester1, rp_embedder2, idp1, account1});
  TestGrantAndRevoke(context(), {rp_requester1, rp_embedder1, idp1, account1},
                     {rp_requester1, rp_embedder1, idp2, account1});
  TestGrantAndRevoke(context(), {rp_requester1, rp_embedder1, idp1, account1},
                     {rp_requester1, rp_embedder1, idp1, account2});
}

// Test that granting a permission for an account, if the permission has already
// been granted, is a noop.
TEST_F(FederatedIdentityAccountKeyedPermissionContextTest,
       GrantPermissionForSameAccount) {
  const url::Origin rp_requester =
      url::Origin::Create(GURL("https://rp.example"));
  const url::Origin rp_embedder =
      url::Origin::Create(GURL("https://rp-embedder.example"));
  const url::Origin idp = url::Origin::Create(GURL("https://idp.example"));
  std::string account{"consetogo"};

  context()->GrantPermission(rp_requester, rp_embedder, idp, account);
  auto granted_objects1 = context()->GetAllGrantedObjects();

  context()->GrantPermission(rp_requester, rp_embedder, idp, account);
  auto granted_objects2 = context()->GetAllGrantedObjects();

  EXPECT_EQ(1u, granted_objects1.size());
  EXPECT_EQ(1u, granted_objects2.size());
  EXPECT_EQ(granted_objects1[0]->value, granted_objects2[0]->value);
}

// Test that FederatedIdentityAccountKeyedPermissionContext can recover from
// crbug.com/1381130
TEST_F(FederatedIdentityAccountKeyedPermissionContextTest, RecoverFrom1381130) {
  // crbug.com/1381130 only occurred when RP=IDP.
  const url::Origin site = url::Origin::Create(GURL("https://example.com"));
  std::string account{"conestogo"};

  // Storing data not associated with a signed-in account is bad because it
  // makes the expected behaviour of RevokePermission() unclear.
  base::Value::Dict new_object;
  new_object.Set(kTestIdpOriginKey, site.Serialize());
  new_object.Set("bug", base::Value("wrong"));
  context()->GrantObjectPermission(site, std::move(new_object));

  context()->GrantPermission(site, site, site, account);
  EXPECT_TRUE(context()->HasPermission(site, site, site, account));
}
