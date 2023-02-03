// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_grouper_groups.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/groups.pb.h"

TEST(AutocompleteGrouperGroupsTest, Group) {
  Group group = {4,
                 Group::GroupIdLimitsAndCounts{{omnibox::GROUP_SEARCH, {2}},
                                               {omnibox::GROUP_DOCUMENT, {4}}}};
  AutocompleteMatch search_match{};
  search_match.suggestion_group_id = omnibox::GROUP_SEARCH;
  AutocompleteMatch doc_match{};
  doc_match.suggestion_group_id = omnibox::GROUP_DOCUMENT;
  AutocompleteMatch other_match{};
  other_match.suggestion_group_id = omnibox::GROUP_OTHER_NAVS;

  EXPECT_TRUE(group.CanAdd(search_match));
  EXPECT_TRUE(group.CanAdd(doc_match));
  EXPECT_FALSE(group.CanAdd(other_match));

  // Verify the group limit with 2/2 searches, 0/4 docs, and 2/4 total.
  group.Add(search_match);  // 1/2 searches, 0/4 docs, 1/4 total.
  EXPECT_TRUE(group.CanAdd(search_match));
  group.Add(search_match);  // 2/2 searches, 0/4 docs, 2/4 total.
  EXPECT_FALSE(group.CanAdd(search_match));
  EXPECT_TRUE(group.CanAdd(doc_match));
  EXPECT_FALSE(group.CanAdd(other_match));

  // Verify the total `Group` limit with 2/2 searches, 2/4 docs, and 4/4 total.
  group.Add(doc_match);  // 2/2 searches, 1/4 docs, 3/4 total.
  EXPECT_TRUE(group.CanAdd(doc_match));
  group.Add(doc_match);  // 2/2 searches, 2/4 docs, 4/4 total.
  EXPECT_FALSE(group.CanAdd(search_match));
  EXPECT_FALSE(group.CanAdd(doc_match));
  EXPECT_FALSE(group.CanAdd(other_match));

  // Verify with 0/2 searches, 4/4 docs, and 4/4 total.
  group = {4, Group::GroupIdLimitsAndCounts{{omnibox::GROUP_SEARCH, {2}},
                                            {omnibox::GROUP_DOCUMENT, {4}}}};
  group.Add(doc_match);  // 0/2 searches, 1/4 docs, 1/4 total.
  group.Add(doc_match);  // 0/2 searches, 2/4 docs, 2/4 total.
  group.Add(doc_match);  // 0/2 searches, 3/4 docs, 3/4 total.
  EXPECT_TRUE(group.CanAdd(search_match));
  EXPECT_TRUE(group.CanAdd(doc_match));
  group.Add(doc_match);  // 0/2 searches, 4/4 docs, 4/4 total.
  EXPECT_FALSE(group.CanAdd(search_match));
  EXPECT_FALSE(group.CanAdd(doc_match));
  EXPECT_FALSE(group.CanAdd(other_match));
}

TEST(AutocompleteGrouperGroupsTest, DefaultGroup) {
  Group default_group({1,
                       {{omnibox::GROUP_STARTER_PACK, {1}},
                        {omnibox::GROUP_SEARCH, {1}},
                        {omnibox::GROUP_OTHER_NAVS, {1}}},
                       /*is_default=*/true});
  ACMatches matches{{}, {}, {}};

  // Can't be added because `allowed_to_be_default` is false.
  AutocompleteMatch non_default_match{};
  non_default_match.suggestion_group_id = omnibox::GROUP_STARTER_PACK;
  // Can't be added because `GROUP_DOCUMENT` is not allowed.
  AutocompleteMatch default_match_doc{};
  default_match_doc.suggestion_group_id = omnibox::GROUP_DOCUMENT;
  default_match_doc.allowed_to_be_default_match = true;
  // Can be added.
  AutocompleteMatch default_match_search{};
  default_match_search.suggestion_group_id = omnibox::GROUP_SEARCH;
  default_match_search.allowed_to_be_default_match = true;

  EXPECT_FALSE(default_group.CanAdd(non_default_match));
  EXPECT_TRUE(default_group.CanAdd(default_match_search));
  EXPECT_FALSE(default_group.CanAdd(default_match_doc));

  default_group.Add(default_match_search);
  EXPECT_FALSE(default_group.CanAdd(non_default_match));
  EXPECT_FALSE(default_group.CanAdd(default_match_search));
  EXPECT_FALSE(default_group.CanAdd(default_match_doc));
}
