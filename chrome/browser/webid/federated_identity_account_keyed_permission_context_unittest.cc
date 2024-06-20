// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_account_keyed_permission_context.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/schemeful_site.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

constexpr char kTestIdpOriginKey[] = "idp-origin";

using testing::ElementsAre;
using testing::IsEmpty;
}

class FederatedIdentityAccountKeyedPermissionContextTest
    : public testing::Test {
 public:
  FederatedIdentityAccountKeyedPermissionContextTest() {
    context_ = std::make_unique<FederatedIdentityAccountKeyedPermissionContext>(
        &profile_);
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
  context()->RevokePermission(rp, rp, idp, account, base::DoNothing());
  EXPECT_TRUE(context()->GetAllGrantedObjects().empty());

  // New format
  context()->GrantPermission(rp, rp, idp, account);
  granted_objects = context()->GetAllGrantedObjects();
  EXPECT_EQ(1u, granted_objects.size());
  std::string key_new_format =
      context()->GetKeyForObject(granted_objects[0]->value);

  EXPECT_EQ(key_old_format, key_new_format);
}

// Test a URL with a '<' character, which is used as a separator in
// FederatedIdentityAccountKeyedPermissionContext::GetKeyForObject().
TEST_F(FederatedIdentityAccountKeyedPermissionContextTest, VerifyKeySeparator) {
  // Assert that URL host part can't include '<'.
  ASSERT_FALSE(GURL("https://rp<.example/").is_valid());

  // FederatedIdentityAccountKeyedPermissionContext accepts only valid URLs.
  // So test a URL with '<', but not in the host part.
  const url::Origin rp = url::Origin::Create(GURL("https://<@rp.example/<?<"));
  const url::Origin idp =
      url::Origin::Create(GURL("https://<@idp.example/<?<"));
  const url::Origin rp_embedder =
      url::Origin::Create(GURL("https://<@rp-embedder.example/<?<"));
  const std::string account("consetogo");

  context()->GrantPermission(rp, rp_embedder, idp, account);
  auto granted_objects = context()->GetAllGrantedObjects();
  EXPECT_EQ(1u, granted_objects.size());
  std::string key = context()->GetKeyForObject(granted_objects[0]->value);
  EXPECT_EQ("https://idp.example<https://rp-embedder.example", key);
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
  EXPECT_EQ(context()->GetLastUsedTimestamp(rp, rp, idp1, account_a),
            base::Time());
  EXPECT_EQ(context()->GetLastUsedTimestamp(rp, rp, idp1, account_b),
            base::Time());
  EXPECT_EQ(context()->GetLastUsedTimestamp(rp, rp, idp2, account_c),
            base::Time());
  EXPECT_EQ(context()->GetLastUsedTimestamp(rp, other_origin, idp1, account_a),
            std::nullopt);

  EXPECT_EQ(context()->GetLastUsedTimestamp(rp, rp, idp1, account_c),
            std::nullopt);
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

  EXPECT_TRUE(context->GetLastUsedTimestamp(
      grant1.relying_party_requester, grant1.relying_party_embedder,
      grant1.identity_provider, grant1.account_id));
  EXPECT_FALSE(context->GetLastUsedTimestamp(
      grant2.relying_party_requester, grant2.relying_party_embedder,
      grant2.identity_provider, grant2.account_id));

  context->GrantPermission(grant2.relying_party_requester,
                           grant2.relying_party_embedder,
                           grant2.identity_provider, grant2.account_id);

  EXPECT_TRUE(context->GetLastUsedTimestamp(
      grant1.relying_party_requester, grant1.relying_party_embedder,
      grant1.identity_provider, grant1.account_id));
  EXPECT_TRUE(context->GetLastUsedTimestamp(
      grant2.relying_party_requester, grant2.relying_party_embedder,
      grant2.identity_provider, grant2.account_id));

  context->RevokePermission(
      grant1.relying_party_requester, grant1.relying_party_embedder,
      grant1.identity_provider, grant1.account_id, base::DoNothing());
  EXPECT_FALSE(context->GetLastUsedTimestamp(
      grant1.relying_party_requester, grant1.relying_party_embedder,
      grant1.identity_provider, grant1.account_id));
  EXPECT_TRUE(context->GetLastUsedTimestamp(
      grant2.relying_party_requester, grant2.relying_party_embedder,
      grant2.identity_provider, grant2.account_id));

  context->RevokePermission(
      grant2.relying_party_requester, grant2.relying_party_embedder,
      grant2.identity_provider, grant2.account_id, base::DoNothing());
  EXPECT_FALSE(context->GetLastUsedTimestamp(
      grant1.relying_party_requester, grant1.relying_party_embedder,
      grant1.identity_provider, grant1.account_id));
  EXPECT_FALSE(context->GetLastUsedTimestamp(
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
// been granted, is a noop, except for the timestamps.
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

  ASSERT_EQ(1u, granted_objects1.size());
  ASSERT_EQ(1u, granted_objects2.size());

  const std::string strs[] = {"idp-origin", "rp-embedder", "rp-requester"};
  for (const auto& str : strs) {
    std::string* str1 = granted_objects1[0]->value.FindString(str);
    ASSERT_TRUE(str1);
    std::string* str2 = granted_objects1[0]->value.FindString(str);
    ASSERT_TRUE(str2);
    EXPECT_EQ(*str1, *str2);
  }

  base::Value::List* account_list1 =
      granted_objects1[0]->value.FindList("account-ids");
  ASSERT_TRUE(account_list1);
  ASSERT_EQ(account_list1->size(), 1u);
  ASSERT_TRUE(account_list1->front().is_dict());
  const auto& account_dict1 = account_list1->front().GetDict();
  ASSERT_EQ(account_dict1.size(), 2u);
  EXPECT_TRUE(account_dict1.FindString("account-id"));
  EXPECT_TRUE(account_dict1.FindString("timestamp"));

  base::Value::List* account_list2 =
      granted_objects1[0]->value.FindList("account-ids");
  ASSERT_TRUE(account_list2);
  ASSERT_EQ(account_list2->size(), 1u);
  ASSERT_TRUE(account_list2->front().is_dict());
  const auto& account_dict2 = account_list2->front().GetDict();
  ASSERT_EQ(account_dict2.size(), 2u);
  EXPECT_TRUE(account_dict2.FindString("account-id"));
  EXPECT_TRUE(account_dict2.FindString("timestamp"));

  // The strings should match.
  EXPECT_EQ(account_dict1.FindString("account-id"),
            account_dict2.FindString("account-id"));
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
  EXPECT_TRUE(context()->GetLastUsedTimestamp(site, site, site, account));
}

TEST_F(FederatedIdentityAccountKeyedPermissionContextTest,
       HasPermission_SchemefulSite) {
  const url::Origin relying_party_requester =
      url::Origin::Create(GURL("https://www.relying_party_requester.com"));
  const url::Origin relying_party_embedder =
      url::Origin::Create(GURL("https://www.relying_party_embedder.com"));
  const url::Origin identity_provider =
      url::Origin::Create(GURL("https://www.identity_provider.com"));

  context()->GrantPermission(relying_party_requester, relying_party_embedder,
                             identity_provider, "my_account");
  EXPECT_TRUE(
      context()->HasPermission(net::SchemefulSite(relying_party_embedder),
                               net::SchemefulSite(identity_provider)));
  EXPECT_FALSE(
      context()->HasPermission(net::SchemefulSite(identity_provider),
                               net::SchemefulSite(relying_party_embedder)));
  EXPECT_FALSE(
      context()->HasPermission(net::SchemefulSite(identity_provider),
                               net::SchemefulSite(relying_party_requester)));
  EXPECT_FALSE(
      context()->HasPermission(net::SchemefulSite(relying_party_requester),
                               net::SchemefulSite(identity_provider)));
}

TEST_F(FederatedIdentityAccountKeyedPermissionContextTest, RevokeNoMatch) {
  constexpr char kAccountId[] = "account123";
  const url::Origin rpRequester =
      url::Origin::Create(GURL("https://rp.example"));
  const url::Origin rpEmbedder =
      url::Origin::Create(GURL("https://rp-embedder.example"));
  const url::Origin idp = url::Origin::Create(GURL("https://idp.example"));

  // Revoke will not crash if there are no previous permissions.
  context()->RevokePermission(rpRequester, rpEmbedder, idp, kAccountId,
                              base::DoNothing());

  context()->GrantPermission(rpRequester, rpEmbedder, idp, kAccountId);
  EXPECT_TRUE(context()->GetLastUsedTimestamp(rpRequester, rpEmbedder, idp,
                                              kAccountId));

  // Revoke will remove the permission even if the account ID does not
  // match.
  context()->RevokePermission(rpRequester, rpEmbedder, idp, "noMatch",
                              base::DoNothing());
  EXPECT_FALSE(context()->GetLastUsedTimestamp(rpRequester, rpEmbedder, idp,
                                               kAccountId));

  // Revoke will remove the permission when the account ID matches, but
  // only that permission.
  context()->GrantPermission(rpRequester, rpEmbedder, idp, kAccountId);
  context()->GrantPermission(rpRequester, rpEmbedder, idp, "other");
  context()->RevokePermission(rpRequester, rpEmbedder, idp, kAccountId,
                              base::DoNothing());
  EXPECT_FALSE(context()->GetLastUsedTimestamp(rpRequester, rpEmbedder, idp,
                                               kAccountId));
  EXPECT_TRUE(
      context()->GetLastUsedTimestamp(rpRequester, rpEmbedder, idp, "other"));
}

TEST_F(FederatedIdentityAccountKeyedPermissionContextTest,
       GetSharingPermissionGrantsAsContentSettings_FeatureDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(blink::features::kFedCmWithStorageAccessAPI);
  const url::Origin relying_party_requester =
      url::Origin::Create(GURL("https://www.relying_party_requester.com"));
  const url::Origin relying_party_embedder =
      url::Origin::Create(GURL("https://www.relying_party_embedder.com"));
  const url::Origin identity_provider =
      url::Origin::Create(GURL("https://www.identity_provider.com"));

  context()->GrantPermission(relying_party_requester, relying_party_embedder,
                             identity_provider, "my_account");
  ASSERT_TRUE(
      context()->HasPermission(net::SchemefulSite(relying_party_embedder),
                               net::SchemefulSite(identity_provider)));

  EXPECT_THAT(context()->GetSharingPermissionGrantsAsContentSettings(),
              IsEmpty());
}

TEST_F(FederatedIdentityAccountKeyedPermissionContextTest,
       GetSharingPermissionGrantsAsContentSettings_FeatureEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(blink::features::kFedCmWithStorageAccessAPI);
  const url::Origin relying_party_requester =
      url::Origin::Create(GURL("https://www.relying_party_requester.com"));
  const url::Origin relying_party_embedder =
      url::Origin::Create(GURL("https://www.relying_party_embedder.com"));
  const url::Origin identity_provider =
      url::Origin::Create(GURL("https://www.identity_provider.com"));
  constexpr char account_id[] = "my_account";

  net::SchemefulSite rp_embedder_site(relying_party_embedder);
  net::SchemefulSite idp_site(identity_provider);

  context()->GrantPermission(relying_party_requester, relying_party_embedder,
                             identity_provider, account_id);
  ASSERT_TRUE(context()->HasPermission(rp_embedder_site, idp_site));

  EXPECT_THAT(context()->GetSharingPermissionGrantsAsContentSettings(),
              IsEmpty());

  {
    base::test::TestFuture<void> future;
    context()->MarkStorageAccessEligible(rp_embedder_site, idp_site,
                                         future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  EXPECT_THAT(context()->GetSharingPermissionGrantsAsContentSettings(),
              ElementsAre(ContentSettingPatternSource(
                  ContentSettingsPattern::FromURLToSchemefulSitePattern(
                      identity_provider.GetURL()),
                  ContentSettingsPattern::FromURLToSchemefulSitePattern(
                      relying_party_embedder.GetURL()),
                  content_settings::ContentSettingToValue(
                      ContentSetting::CONTENT_SETTING_ALLOW),
                  content_settings::ProviderType::kNone, /*incognito=*/false)));

  base::test::TestFuture<void> future;
  context()->RevokePermission(relying_party_requester, relying_party_embedder,
                              identity_provider, account_id,
                              future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_THAT(context()->GetSharingPermissionGrantsAsContentSettings(),
              IsEmpty());
}

TEST_F(FederatedIdentityAccountKeyedPermissionContextTest,
       PermissionWithAndWithoutTimestamp) {
  const url::Origin relying_party_requester =
      url::Origin::Create(GURL("https://www.relying_party_requester.com"));
  const url::Origin relying_party_embedder =
      url::Origin::Create(GURL("https://www.relying_party_embedder.com"));
  const url::Origin identity_provider =
      url::Origin::Create(GURL("https://www.identity_provider.com"));

  const std::string account_a("conestogo");
  const std::string account_b("woolwich");
  const std::string account_c("wellesley");

  // Add a couple of accounts without timestamps.
  std::string key =
      base::StringPrintf("%s<%s", identity_provider.Serialize().c_str(),
                         relying_party_embedder.Serialize().c_str());

  base::Value::Dict new_object;
  new_object.Set("rp-requester", relying_party_requester.Serialize());
  new_object.Set("rp-embedder", relying_party_embedder.Serialize());
  new_object.Set("idp-origin", identity_provider.Serialize());

  base::Value::List account_list;
  account_list.Append(account_a);
  account_list.Append(account_b);
  new_object.Set("account-ids", base::Value(std::move(account_list)));
  context()->GrantObjectPermission(relying_party_requester,
                                   std::move(new_object));

  EXPECT_TRUE(context()->GetLastUsedTimestamp(relying_party_requester,
                                              relying_party_embedder,
                                              identity_provider, account_a));
  EXPECT_TRUE(context()->GetLastUsedTimestamp(relying_party_requester,
                                              relying_party_embedder,
                                              identity_provider, account_b));
  EXPECT_FALSE(context()->GetLastUsedTimestamp(relying_party_requester,
                                               relying_party_embedder,
                                               identity_provider, account_c));
  EXPECT_TRUE(context()->HasPermission(
      relying_party_requester, relying_party_embedder, identity_provider));

  // RefreshExistingPermission works with an old account but does not work if
  // account does not exist.
  EXPECT_TRUE(context()->RefreshExistingPermission(
      relying_party_requester, relying_party_embedder, identity_provider,
      account_b));
  EXPECT_FALSE(context()->RefreshExistingPermission(
      relying_party_requester, relying_party_embedder, identity_provider,
      account_c));

  // Add a third account, with timestamp.
  context()->GrantPermission(relying_party_requester, relying_party_embedder,
                             identity_provider, account_c);

  EXPECT_TRUE(context()->GetLastUsedTimestamp(relying_party_requester,
                                              relying_party_embedder,
                                              identity_provider, account_a));
  EXPECT_TRUE(context()->GetLastUsedTimestamp(relying_party_requester,
                                              relying_party_embedder,
                                              identity_provider, account_b));
  EXPECT_TRUE(context()->GetLastUsedTimestamp(relying_party_requester,
                                              relying_party_embedder,
                                              identity_provider, account_c));

  // RefreshExistingPermission works with the new format.
  EXPECT_TRUE(context()->RefreshExistingPermission(
      relying_party_requester, relying_party_embedder, identity_provider,
      account_c));

  // GetAllDataKeys() does not crash.
  context()->GetAllDataKeys(base::DoNothing());

  // Revoke works with both formats.
  base::test::TestFuture<void> future;
  context()->RevokePermission(relying_party_requester, relying_party_embedder,
                              identity_provider, account_a,
                              future.GetCallback());
  ASSERT_TRUE(future.Wait());
  // Revoke works with both formats.
  base::test::TestFuture<void> future2;
  context()->RevokePermission(relying_party_requester, relying_party_embedder,
                              identity_provider, account_c,
                              future2.GetCallback());
  ASSERT_TRUE(future2.Wait());

  EXPECT_FALSE(context()->GetLastUsedTimestamp(relying_party_requester,
                                               relying_party_embedder,
                                               identity_provider, account_a));
  EXPECT_TRUE(context()->GetLastUsedTimestamp(relying_party_requester,
                                              relying_party_embedder,
                                              identity_provider, account_b));
  EXPECT_FALSE(context()->GetLastUsedTimestamp(relying_party_requester,
                                               relying_party_embedder,
                                               identity_provider, account_c));
  EXPECT_TRUE(context()->HasPermission(
      relying_party_requester, relying_party_embedder, identity_provider));
}
