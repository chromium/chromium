// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/public/utils.h"

#include <optional>

#include "base/uuid.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#include "components/tab_groups/tab_group_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tab_groups {
namespace {
class UtilsUnitTest : public testing::Test {
 public:
  UtilsUnitTest() = default;
  ~UtilsUnitTest() override = default;
};

TEST_F(UtilsUnitTest, NullGroup) {
  auto log = TabGroupToShortLogString("prefix", nullptr);
  EXPECT_EQ("[Null]", log);
}

TEST_F(UtilsUnitTest, IdsOnly) {
  base::Uuid uuid1 =
      base::Uuid::ParseLowercase("21abd97f-73e8-4b88-9389-a9fee6abda5e");
  auto log1 = TabGroupIdsToShortLogString(
      "prefix", uuid1, std::optional<syncer::CollaborationId>());
  EXPECT_EQ(
      "prefix\n"
      "  ID: 21abd97f-73e8-4b88-9389-a9fee6abda5e\n"
      "  Collab ID: N/A\n",
      log1);

  base::Uuid uuid2 =
      base::Uuid::ParseLowercase("31abd97f-73e8-4b88-9389-a9fee6abda5e");
  std::optional<syncer::CollaborationId> collab_id2("/?-group_id");
  auto log2 = TabGroupIdsToShortLogString("prefix", uuid2, collab_id2);
  EXPECT_EQ(
      "prefix\n"
      "  ID: 31abd97f-73e8-4b88-9389-a9fee6abda5e\n"
      "  Collab ID: /?-group_id\n",
      log2);
}

TEST_F(UtilsUnitTest, TestValidGroups) {
  SavedTabGroup g1(
      u"title1", tab_groups::TabGroupColorId::kBlue,
      /*tabs=*/{},
      /*position=*/std::nullopt,
      base::Uuid::ParseLowercase("31abd97f-73e8-4b88-9389-a9fee6abda5e"));

  std::string log1 = TabGroupToShortLogString("prefix", &g1);
  EXPECT_EQ(
      "prefix\n"
      "  Title: title1\n"
      "  ID: 31abd97f-73e8-4b88-9389-a9fee6abda5e\n"
      "  Originating ID: N/A\n"
      "  Local ID: N/A\n"
      "  Collab ID: N/A\n"
      "  Hidden: 0\n"
      "  Transition to Shared: 0\n"
      "  Transition to Saved: 0\n"
      "  # Tabs: 0\n",
      log1);

  base::Uuid id2 =
      base::Uuid::ParseLowercase("41abd97f-73e8-4b88-9389-a9fee6abda5e");
  SavedTabGroupTab tab(GURL("www.google.com"), u"title1", id2, /*position=*/0);

  std::optional<base::Token> local_id_token2 =
      base::Token::FromString("0123456789ABCDEF5A5A5A5AA5A5A5A5");
  EXPECT_TRUE(local_id_token2.has_value());
#if BUILDFLAG(IS_ANDROID)
  LocalTabGroupID local_id2 = local_id_token2.value();
#else
  LocalTabGroupID local_id2 = TabGroupId::FromRawToken(local_id_token2.value());
#endif

  SavedTabGroup g2(u"title1", tab_groups::TabGroupColorId::kBlue,
                   /*tabs=*/{tab},
                   /*position=*/std::nullopt, id2, local_id2);
  g2.SetCollaborationId(syncer::CollaborationId("/?-group_id"));
  g2.SetIsHidden(true);
  g2.SetIsTransitioningToSaved(true);
  g2.MarkTransitioningToSharedForTesting();
  g2.SetOriginatingTabGroupGuid(
      base::Uuid::ParseLowercase("41abd97f-73e8-4b88-9389-b9fee6abda5e"),
      /*use_originating_tab_group_guid=*/true);
  std::string log2 = TabGroupToShortLogString("prefix2", &g2);
  EXPECT_EQ(
      "prefix2\n"
      "  Title: title1\n"
      "  ID: 41abd97f-73e8-4b88-9389-a9fee6abda5e\n"
      "  Originating ID: 41abd97f-73e8-4b88-9389-b9fee6abda5e\n"
      "  Local ID: 0123456789ABCDEF5A5A5A5AA5A5A5A5\n"
      "  Collab ID: /?-group_id\n"
      "  Hidden: 1\n"
      "  Transition to Shared: 1\n"
      "  Transition to Saved: 1\n"
      "  # Tabs: 1\n",
      log2);
}

}  // namespace
}  // namespace tab_groups
