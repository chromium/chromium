// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/model/management_data.h"

#include <optional>
#include <sstream>
#include <string>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/test/gtest_util.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/web_applications/proto/web_app_management_data.pb.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

TEST(ManagementDataTest, EmptyByDefault) {
  ManagementData data;
  EXPECT_FALSE(data.HasAnySources());
  EXPECT_TRUE(data.GetSources().empty());
  EXPECT_TRUE(data.AsDebugValue().empty());
  EXPECT_THAT(data.GetAdditionalPolicyIds(), IsEmpty());

  for (WebAppManagement::Type type : WebAppManagementTypes::All()) {
    EXPECT_FALSE(data.HasSource(type));
    EXPECT_FALSE(data.IsPlaceholder(type));
    EXPECT_THAT(data.GetInstallUrls(type), IsEmpty());
    EXPECT_FALSE(data.RemoveInstallUrl(type, GURL("https://example.com/")));
  }
  EXPECT_FALSE(data.HasInstallUrl(GURL("https://example.com/")));
}

TEST(ManagementDataTest, AllSourcesAddQueryRemoveAndParity) {
  ManagementData data;
  WebAppManagementTypes expected;

  for (WebAppManagement::Type type : WebAppManagementTypes::All()) {
    data.AddSource(type);
    expected.Put(type);
    EXPECT_TRUE(data.HasSource(type));
    EXPECT_TRUE(data.HasAnySources());
    EXPECT_EQ(data.GetSources(), expected);
    EXPECT_EQ(data.AsDebugValue().size(), expected.size());
    base::expected<ManagementData, ManagementData::ParseError> round_tripped =
        ManagementData::Parse(data.ToProto());
    ASSERT_TRUE(round_tripped.has_value());
    EXPECT_EQ(round_tripped->GetSources(), expected);
  }

  for (WebAppManagement::Type type : WebAppManagementTypes::All()) {
    data.RemoveSource(type);
    expected.Remove(type);
    EXPECT_FALSE(data.HasSource(type));
    EXPECT_EQ(data.GetSources(), expected);
    EXPECT_EQ(data.AsDebugValue().size(), expected.size());
    base::expected<ManagementData, ManagementData::ParseError> round_tripped =
        ManagementData::Parse(data.ToProto());
    ASSERT_TRUE(round_tripped.has_value());
    EXPECT_EQ(round_tripped->GetSources(), expected);
  }

  EXPECT_FALSE(data.HasAnySources());
}

TEST(ManagementDataTest, AddSourceIsIdempotentAndPreservesData) {
  ManagementData data;
  const GURL kPolicyUrl("https://policy.example/install");
  const GURL kStoreUrl("https://store.example/install");

  data.AddSource(WebAppManagement::kPolicy);
  data.AddInstallUrl(WebAppManagement::kPolicy, kPolicyUrl);
  data.SetIsPlaceholder(WebAppManagement::kPolicy, true);
  data.AddSource(WebAppManagement::kPolicy);

  EXPECT_TRUE(data.IsPlaceholder(WebAppManagement::kPolicy));
  EXPECT_THAT(data.GetInstallUrls(WebAppManagement::kPolicy),
              ElementsAre(kPolicyUrl));

  data.AddSource(WebAppManagement::kWebAppStore);
  data.AddInstallUrl(WebAppManagement::kWebAppStore, kStoreUrl);
  data.AddAdditionalPolicyId("policy-id-1");
  data.AddSource(WebAppManagement::kWebAppStore);

  EXPECT_THAT(data.GetInstallUrls(WebAppManagement::kWebAppStore),
              ElementsAre(kStoreUrl));
  EXPECT_THAT(data.GetAdditionalPolicyIds(), ElementsAre("policy-id-1"));
}

TEST(ManagementDataTest, InstallUrlMutatorsAndEmptyStructAccessors) {
  ManagementData data;
  const GURL kDefaultUrl1("https://default1.example/");
  const GURL kDefaultUrl2("https://default2.example/");
  const GURL kPolicyUrl("https://policy.example/");

  data.AddSource(WebAppManagement::kSync);
  data.AddSource(WebAppManagement::kUserInstalled);
  data.AddSource(WebAppManagement::kIwaUserInstalled);
  EXPECT_THAT(data.GetInstallUrls(WebAppManagement::kSync), IsEmpty());
  EXPECT_THAT(data.GetInstallUrls(WebAppManagement::kUserInstalled), IsEmpty());
  EXPECT_THAT(data.GetInstallUrls(WebAppManagement::kIwaUserInstalled),
              IsEmpty());
  EXPECT_FALSE(data.RemoveInstallUrl(WebAppManagement::kSync, kDefaultUrl1));

  // Verify the empty-set reference remains valid across calls.
  const base::flat_set<GURL>& empty_sync =
      data.GetInstallUrls(WebAppManagement::kSync);
  const base::flat_set<GURL>& empty_absent =
      data.GetInstallUrls(WebAppManagement::kOem);
  EXPECT_EQ(&empty_sync, &empty_absent);

  data.AddSource(WebAppManagement::kDefault);
  data.AddSource(WebAppManagement::kPolicy);
  data.AddInstallUrl(WebAppManagement::kDefault, kDefaultUrl1);
  data.AddInstallUrl(WebAppManagement::kDefault, kDefaultUrl2);
  data.AddInstallUrl(WebAppManagement::kPolicy, kPolicyUrl);
  EXPECT_TRUE(data.HasSource(WebAppManagement::kDefault));
  EXPECT_TRUE(data.HasSource(WebAppManagement::kPolicy));
  EXPECT_THAT(data.GetInstallUrls(WebAppManagement::kDefault),
              UnorderedElementsAre(kDefaultUrl1, kDefaultUrl2));
  EXPECT_TRUE(data.HasInstallUrl(kDefaultUrl1));
  EXPECT_TRUE(data.HasInstallUrl(kPolicyUrl));
  EXPECT_FALSE(data.HasInstallUrl(GURL("https://missing.example/")));

  // Removing every install URL keeps the source entry intact.
  EXPECT_TRUE(data.RemoveInstallUrl(WebAppManagement::kDefault, kDefaultUrl1));
  EXPECT_FALSE(data.RemoveInstallUrl(WebAppManagement::kDefault, kDefaultUrl1));
  EXPECT_TRUE(data.HasSource(WebAppManagement::kDefault));
  EXPECT_TRUE(data.RemoveInstallUrl(WebAppManagement::kDefault, kDefaultUrl2));
  EXPECT_TRUE(data.HasSource(WebAppManagement::kDefault));
  EXPECT_THAT(data.GetInstallUrls(WebAppManagement::kDefault), IsEmpty());
  EXPECT_FALSE(data.HasInstallUrl(kDefaultUrl2));
}

TEST(ManagementDataTest, PlaceholderAndPolicyIdAccessors) {
  ManagementData data;
  EXPECT_FALSE(data.IsPlaceholder(WebAppManagement::kPolicy));
  EXPECT_FALSE(data.IsPlaceholder(WebAppManagement::kKiosk));

  const base::flat_set<std::string>& empty_absent =
      data.GetAdditionalPolicyIds();
  EXPECT_THAT(empty_absent, IsEmpty());

  data.AddSource(WebAppManagement::kPolicy);
  data.AddSource(WebAppManagement::kKiosk);
  data.SetIsPlaceholder(WebAppManagement::kPolicy, true);
  data.SetIsPlaceholder(WebAppManagement::kKiosk, true);
  EXPECT_TRUE(data.HasSource(WebAppManagement::kPolicy));
  EXPECT_TRUE(data.HasSource(WebAppManagement::kKiosk));
  EXPECT_TRUE(data.IsPlaceholder(WebAppManagement::kPolicy));
  EXPECT_TRUE(data.IsPlaceholder(WebAppManagement::kKiosk));
  EXPECT_EQ(&data.GetAdditionalPolicyIds(), &empty_absent);

  data.SetIsPlaceholder(WebAppManagement::kPolicy, false);
  EXPECT_FALSE(data.IsPlaceholder(WebAppManagement::kPolicy));
  EXPECT_TRUE(data.HasSource(WebAppManagement::kPolicy));

  data.AddSource(WebAppManagement::kWebAppStore);
  data.AddAdditionalPolicyId("id-a");
  data.AddAdditionalPolicyId("id-b");
  data.AddAdditionalPolicyId("id-a");
  EXPECT_TRUE(data.HasSource(WebAppManagement::kWebAppStore));
  EXPECT_THAT(data.GetAdditionalPolicyIds(),
              UnorderedElementsAre("id-a", "id-b"));
}

TEST(ManagementDataDeathTest, SetIsPlaceholderUnsupportedSource) {
  ManagementData data;
  EXPECT_CHECK_DEATH(data.SetIsPlaceholder(WebAppManagement::kDefault, true));
}

TEST(ManagementDataDeathTest, AddInstallUrlInvalidUrl) {
  ManagementData data;
  EXPECT_CHECK_DEATH(data.AddInstallUrl(WebAppManagement::kDefault, GURL()));
}

TEST(ManagementDataDeathTest, AddInstallUrlNonExternalSources) {
  ManagementData data;
  EXPECT_CHECK_DEATH(data.AddInstallUrl(WebAppManagement::kSync,
                                        GURL("https://example.com/")));
  EXPECT_CHECK_DEATH(data.AddInstallUrl(WebAppManagement::kUserInstalled,
                                        GURL("https://example.com/")));
}

TEST(ManagementDataDeathTest, AddInstallUrlIwaSources) {
  ManagementData data;
  EXPECT_CHECK_DEATH(data.AddInstallUrl(WebAppManagement::kIwaPolicy,
                                        GURL("https://example.com/")));
}

TEST(ManagementDataDeathTest, AddAdditionalPolicyIdChecks) {
  ManagementData data;
  EXPECT_CHECK_DEATH(data.AddAdditionalPolicyId("policy-id"));
  data.AddSource(WebAppManagement::kWebAppStore);
  EXPECT_CHECK_DEATH(data.AddAdditionalPolicyId(""));
}

TEST(ManagementDataDeathTest, MutatorsRequireActiveSource) {
  ManagementData data;
  EXPECT_CHECK_DEATH(data.SetIsPlaceholder(WebAppManagement::kPolicy, true));
  EXPECT_CHECK_DEATH(data.AddInstallUrl(WebAppManagement::kDefault,
                                        GURL("https://example.com/")));
}

TEST(ManagementDataTest, SourceCapabilitySetsMatchBehavior) {
  const GURL kUrl("https://example.com/install");
  for (WebAppManagement::Type type : WebAppManagementTypes::All()) {
    ManagementData data;
    data.AddSource(type);
    if (kSourcesWithInstallUrls.Has(type)) {
      data.AddInstallUrl(type, kUrl);
      EXPECT_THAT(data.GetInstallUrls(type), ElementsAre(kUrl));
    } else {
      EXPECT_THAT(data.GetInstallUrls(type), IsEmpty());
    }

    if (kSourcesWithPlaceholder.Has(type)) {
      ManagementData placeholder_data;
      placeholder_data.AddSource(type);
      placeholder_data.SetIsPlaceholder(type, true);
      EXPECT_TRUE(placeholder_data.IsPlaceholder(type));
    }

    if (kSourcesWithAdditionalPolicyIds.Has(type)) {
      EXPECT_EQ(type, WebAppManagement::kWebAppStore);
      ManagementData store_data;
      store_data.AddSource(type);
      store_data.AddAdditionalPolicyId("policy-id");
      EXPECT_TRUE(store_data.HasSource(type));
    }
  }
}

TEST(ManagementDataTest, ParseAndToProtoAll14SourcesRoundTrip) {
  ManagementData original;
  for (WebAppManagement::Type type : WebAppManagementTypes::All()) {
    original.AddSource(type);
  }
  original.AddInstallUrl(WebAppManagement::kSystem,
                         GURL("https://system.example/"));
  original.AddInstallUrl(WebAppManagement::kPolicy,
                         GURL("https://policy.example/"));
  original.SetIsPlaceholder(WebAppManagement::kPolicy, true);
  original.AddInstallUrl(WebAppManagement::kWebAppStore,
                         GURL("https://store.example/"));
  original.AddAdditionalPolicyId("policy-1");
  original.AddAdditionalPolicyId("policy-2");
  original.AddInstallUrl(WebAppManagement::kDefault,
                         GURL("https://default.example/"));
  original.AddInstallUrl(WebAppManagement::kSubApp,
                         GURL("https://subapp.example/"));
  original.AddInstallUrl(WebAppManagement::kKiosk,
                         GURL("https://kiosk.example/"));
  original.SetIsPlaceholder(WebAppManagement::kKiosk, false);
  original.AddInstallUrl(WebAppManagement::kOem, GURL("https://oem.example/"));
  original.AddInstallUrl(WebAppManagement::kOneDriveIntegration,
                         GURL("https://onedrive.example/"));
  original.AddInstallUrl(WebAppManagement::kApsDefault,
                         GURL("https://aps.example/"));

  proto::ManagementData proto = original.ToProto();
  EXPECT_TRUE(proto.has_system());
  EXPECT_TRUE(proto.has_policy());
  EXPECT_TRUE(proto.policy().has_is_placeholder());
  EXPECT_TRUE(proto.policy().is_placeholder());
  EXPECT_TRUE(proto.has_web_app_store());
  EXPECT_TRUE(proto.has_sync());
  EXPECT_TRUE(proto.has_default_source());
  EXPECT_TRUE(proto.has_sub_app());
  EXPECT_TRUE(proto.has_kiosk());
  EXPECT_TRUE(proto.kiosk().has_is_placeholder());
  EXPECT_FALSE(proto.kiosk().is_placeholder());
  EXPECT_TRUE(proto.has_oem());
  EXPECT_TRUE(proto.has_one_drive_integration());
  EXPECT_TRUE(proto.has_aps_default());
  EXPECT_TRUE(proto.has_iwa_shimless_rma());
  EXPECT_TRUE(proto.has_iwa_policy());
  EXPECT_TRUE(proto.has_iwa_user_installed());
  EXPECT_TRUE(proto.has_user_installed());

  base::expected<ManagementData, ManagementData::ParseError> parsed =
      ManagementData::Parse(proto);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(*parsed, original);
  EXPECT_EQ(parsed->GetSources(), WebAppManagementTypes::All());
  EXPECT_EQ(parsed->ToProto().SerializeAsString(), proto.SerializeAsString());
}

TEST(ManagementDataTest, ParseAndToProtoPreservesEmptyUrlBearingSource) {
  ManagementData original;
  original.AddSource(WebAppManagement::kDefault);
  original.AddSource(WebAppManagement::kOem);
  original.AddInstallUrl(WebAppManagement::kOem, GURL("https://oem.example/"));
  ASSERT_TRUE(original.RemoveInstallUrl(WebAppManagement::kOem,
                                        GURL("https://oem.example/")));

  proto::ManagementData proto = original.ToProto();
  EXPECT_TRUE(proto.has_default_source());
  EXPECT_THAT(proto.default_source().install_urls(), IsEmpty());
  EXPECT_TRUE(proto.has_oem());
  EXPECT_THAT(proto.oem().install_urls(), IsEmpty());

  base::expected<ManagementData, ManagementData::ParseError> parsed =
      ManagementData::Parse(proto);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(*parsed, original);
  EXPECT_TRUE(parsed->HasSource(WebAppManagement::kDefault));
  EXPECT_TRUE(parsed->HasSource(WebAppManagement::kOem));
  EXPECT_THAT(parsed->GetInstallUrls(WebAppManagement::kDefault), IsEmpty());
  EXPECT_THAT(parsed->GetInstallUrls(WebAppManagement::kOem), IsEmpty());
  EXPECT_EQ(parsed->ToProto().SerializeAsString(), proto.SerializeAsString());
}

TEST(ManagementDataTest, ParseEmptyProtoSucceedsForUninstallingApps) {
  proto::ManagementData empty_proto;
  base::expected<ManagementData, ManagementData::ParseError> parsed =
      ManagementData::Parse(empty_proto);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_FALSE(parsed->HasAnySources());
  EXPECT_EQ(*parsed, ManagementData());
}

TEST(ManagementDataTest, ParseErrors) {
  {
    proto::ManagementData proto;
    proto.mutable_default_source()->add_install_urls("");
    EXPECT_EQ(ManagementData::Parse(proto),
              base::unexpected(ManagementData::ParseError::kInvalidInstallUrl));
  }
  {
    proto::ManagementData proto;
    proto.mutable_policy()->add_install_urls("not-a-valid-url");
    EXPECT_EQ(ManagementData::Parse(proto),
              base::unexpected(ManagementData::ParseError::kInvalidInstallUrl));
  }
  {
    proto::ManagementData proto;
    proto.mutable_web_app_store()->add_additional_policy_ids("");
    EXPECT_EQ(ManagementData::Parse(proto),
              base::unexpected(ManagementData::ParseError::kEmptyPolicyId));
  }
  {
    proto::ManagementData proto;
    proto.mutable_web_app_store()->add_install_urls("https://store.example/");
    proto.mutable_web_app_store()->add_additional_policy_ids("valid-policy");
    proto.mutable_web_app_store()->add_additional_policy_ids("");
    EXPECT_EQ(ManagementData::Parse(proto),
              base::unexpected(ManagementData::ParseError::kEmptyPolicyId));
  }
}

TEST(ManagementDataTest, AsDebugValueEqualityAndStreamOutput) {
  ManagementData data;
  data.AddSource(WebAppManagement::kSync);
  data.AddSource(WebAppManagement::kDefault);
  data.AddInstallUrl(WebAppManagement::kDefault,
                     GURL("https://default.example/"));
  data.AddSource(WebAppManagement::kPolicy);
  data.AddInstallUrl(WebAppManagement::kPolicy,
                     GURL("https://policy.example/"));
  data.SetIsPlaceholder(WebAppManagement::kPolicy, true);
  data.AddSource(WebAppManagement::kKiosk);
  data.AddInstallUrl(WebAppManagement::kKiosk, GURL("https://kiosk.example/"));
  data.SetIsPlaceholder(WebAppManagement::kKiosk, false);
  data.AddSource(WebAppManagement::kWebAppStore);
  data.AddInstallUrl(WebAppManagement::kWebAppStore,
                     GURL("https://store.example/"));
  data.AddAdditionalPolicyId("store-policy");

  base::DictValue debug = data.AsDebugValue();
  const base::DictValue* sync_dict = debug.FindDict("Sync");
  ASSERT_TRUE(sync_dict);
  EXPECT_TRUE(sync_dict->empty());

  const base::DictValue* default_dict = debug.FindDict("Default");
  ASSERT_TRUE(default_dict);
  EXPECT_EQ(default_dict->size(), 1u);
  const base::ListValue* default_urls = default_dict->FindList("install_urls");
  ASSERT_TRUE(default_urls);
  EXPECT_EQ(default_urls->size(), 1u);
  EXPECT_EQ((*default_urls)[0].GetString(), "https://default.example/");

  const base::DictValue* policy_dict = debug.FindDict("Policy");
  ASSERT_TRUE(policy_dict);
  EXPECT_EQ(policy_dict->FindBool("is_placeholder"), std::optional<bool>(true));
  ASSERT_TRUE(policy_dict->FindList("install_urls"));

  const base::DictValue* kiosk_dict = debug.FindDict("Kiosk");
  ASSERT_TRUE(kiosk_dict);
  EXPECT_EQ(kiosk_dict->FindBool("is_placeholder"), std::optional<bool>(false));
  ASSERT_TRUE(kiosk_dict->FindList("install_urls"));

  const base::DictValue* store_dict = debug.FindDict("WebAppStore");
  ASSERT_TRUE(store_dict);
  const base::ListValue* policy_ids =
      store_dict->FindList("additional_policy_ids");
  ASSERT_TRUE(policy_ids);
  EXPECT_EQ((*policy_ids)[0].GetString(), "store-policy");
  EXPECT_FALSE(store_dict->contains("is_placeholder"));

  ManagementData different = data;
  different.RemoveSource(WebAppManagement::kSync);
  EXPECT_NE(data, different);

  ManagementData same_keys_diff_payload = data;
  same_keys_diff_payload.SetIsPlaceholder(WebAppManagement::kPolicy, false);
  EXPECT_NE(data, same_keys_diff_payload);

  std::ostringstream oss;
  oss << data;
  EXPECT_EQ(oss.str(), debug.DebugString());
}

TEST(ManagementDataTest, MoveResetsMovedFromObject) {
  ManagementData original;
  const GURL kPolicyUrl("https://policy.example/app");
  original.AddSource(WebAppManagement::kPolicy);
  original.AddInstallUrl(WebAppManagement::kPolicy, kPolicyUrl);
  original.SetIsPlaceholder(WebAppManagement::kPolicy, true);
  original.AddSource(WebAppManagement::kWebAppStore);
  original.AddAdditionalPolicyId("policy-id-1");

  ManagementData move_constructed(std::move(original));
  // NOLINTBEGIN(bugprone-use-after-move)
  EXPECT_FALSE(original.HasAnySources());
  EXPECT_TRUE(original.GetSources().empty());
  EXPECT_TRUE(original.AsDebugValue().empty());
  EXPECT_FALSE(original.HasSource(WebAppManagement::kPolicy));
  EXPECT_FALSE(original.HasSource(WebAppManagement::kWebAppStore));
  EXPECT_EQ(original, ManagementData());
  // NOLINTEND(bugprone-use-after-move)

  EXPECT_EQ(move_constructed.GetSources(),
            (WebAppManagementTypes{WebAppManagement::kPolicy,
                                   WebAppManagement::kWebAppStore}));
  EXPECT_EQ(move_constructed.AsDebugValue().size(), 2u);
  EXPECT_TRUE(move_constructed.IsPlaceholder(WebAppManagement::kPolicy));
  EXPECT_THAT(move_constructed.GetInstallUrls(WebAppManagement::kPolicy),
              ElementsAre(kPolicyUrl));
  EXPECT_THAT(move_constructed.GetAdditionalPolicyIds(),
              ElementsAre("policy-id-1"));

  ManagementData move_assigned;
  move_assigned.AddSource(WebAppManagement::kDefault);
  move_assigned = std::move(move_constructed);

  // NOLINTBEGIN(bugprone-use-after-move)
  EXPECT_FALSE(move_constructed.HasAnySources());
  EXPECT_TRUE(move_constructed.GetSources().empty());
  EXPECT_TRUE(move_constructed.AsDebugValue().empty());
  EXPECT_FALSE(move_constructed.HasSource(WebAppManagement::kPolicy));
  EXPECT_FALSE(move_constructed.HasSource(WebAppManagement::kWebAppStore));
  EXPECT_EQ(move_constructed, ManagementData());
  // NOLINTEND(bugprone-use-after-move)

  EXPECT_EQ(move_assigned.GetSources(),
            (WebAppManagementTypes{WebAppManagement::kPolicy,
                                   WebAppManagement::kWebAppStore}));
  EXPECT_EQ(move_assigned.AsDebugValue().size(), 2u);
  EXPECT_FALSE(move_assigned.HasSource(WebAppManagement::kDefault));
  EXPECT_TRUE(move_assigned.IsPlaceholder(WebAppManagement::kPolicy));
  EXPECT_THAT(move_assigned.GetInstallUrls(WebAppManagement::kPolicy),
              ElementsAre(kPolicyUrl));
  EXPECT_THAT(move_assigned.GetAdditionalPolicyIds(),
              ElementsAre("policy-id-1"));
}

}  // namespace
}  // namespace web_app
