// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/suggestion_group_util.h"

#include "base/test/scoped_feature_list.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/groups.pb.h"
#include "ui/base/l10n/l10n_util.h"

// Ensures that accessing unset fields is safe and verifies the default values.
// https://developers.google.com/protocol-buffers/docs/reference/cpp-generated
TEST(SuggestionGroupTest, DefaultValuesForUnsetFields) {
  omnibox::GroupConfig group_config;

  ASSERT_FALSE(group_config.has_header_text());
  ASSERT_EQ("", group_config.header_text());

  ASSERT_FALSE(group_config.has_side_type());
  ASSERT_EQ(omnibox::GroupConfig_SideType_DEFAULT_PRIMARY,
            group_config.side_type());

  ASSERT_FALSE(group_config.has_render_type());
  ASSERT_EQ(omnibox::GroupConfig_RenderType_DEFAULT_VERTICAL,
            group_config.render_type());

  ASSERT_FALSE(group_config.has_visibility());
  ASSERT_EQ(omnibox::GroupConfig_Visibility_DEFAULT_VISIBLE,
            group_config.visibility());

  ASSERT_FALSE(group_config.has_why_this_result_reason());
  ASSERT_EQ(0U, group_config.why_this_result_reason());

  ASSERT_FALSE(group_config.has_section());
  ASSERT_EQ(omnibox::SECTION_DEFAULT, group_config.section());
}

// Ensures that omnibox::GroupIdForNumber() returns the omnibox::GroupId enum
// object corresponding to the given value; and returns omnibox::GROUP_INVALID
// when there is no corresponding enum object.
TEST(SuggestionGroupTest, GroupIdForNumber) {
  ASSERT_EQ(omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST,
            omnibox::GroupIdForNumber(40000));
  ASSERT_EQ(omnibox::GROUP_INVALID, omnibox::GroupIdForNumber(-1));
  ASSERT_EQ(omnibox::GROUP_INVALID, omnibox::GroupIdForNumber(0));
  ASSERT_EQ(omnibox::GROUP_INVALID, omnibox::GroupIdForNumber(123456789));
}

TEST(SuggestionGroupTest, SectionMobileMostVisited_HorizontalRenderType) {
  omnibox::ResetDefaultGroupsForTest();

  base::test::ScopedFeatureList features;
  features.InitWithFeatures({omnibox::kMostVisitedTilesHorizontalRenderGroup},
                            {});

  auto default_groups = omnibox::BuildDefaultGroups();
  auto most_visited_group_config =
      default_groups.find(omnibox::GROUP_MOBILE_MOST_VISITED);

  ASSERT_NE(most_visited_group_config, default_groups.end());
  ASSERT_EQ(omnibox::GroupConfig_RenderType_HORIZONTAL,
            most_visited_group_config->second.render_type());
}

TEST(SuggestionGroupTest, SectionMobileMostVisited_VerticalRenderType) {
  omnibox::ResetDefaultGroupsForTest();

  base::test::ScopedFeatureList features;
  features.InitWithFeatures({},
                            {omnibox::kMostVisitedTilesHorizontalRenderGroup});

  auto default_groups = omnibox::BuildDefaultGroups();
  auto most_visited_group_config =
      default_groups.find(omnibox::GROUP_MOBILE_MOST_VISITED);

  ASSERT_NE(most_visited_group_config, default_groups.end());
  ASSERT_EQ(omnibox::GroupConfig_RenderType_DEFAULT_VERTICAL,
            most_visited_group_config->second.render_type());
}
