// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_grouper_sections.h"

#include <iterator>
#include <memory>

#include "components/omnibox/browser/autocomplete_match.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/groups.pb.h"

namespace {
AutocompleteMatch CreateMatch(int relevance,
                              omnibox::GroupId group_id,
                              bool allowed_to_be_default = false) {
  AutocompleteMatch match;
  match.relevance = relevance;
  match.suggestion_group_id = group_id;
  match.allowed_to_be_default_match = allowed_to_be_default;
  return match;
}

void VerifyMatches(const ACMatches& matches,
                   std::vector<int> expected_relevances) {
  std::vector<int> relevances = {};
  base::ranges::transform(matches, std::back_inserter(relevances),
                          [&](const auto& match) { return match.relevance; });

  EXPECT_THAT(relevances, testing::ElementsAreArray(expected_relevances));
}

}  // namespace

// Tests a section with no groups.
TEST(AutocompleteGrouperSectionsTest, Section) {
  auto test = [](ACMatches matches, std::vector<int> expected_relevances) {
    PSections sections;
    omnibox::GroupConfigMap group_configs;
    sections.push_back(std::make_unique<Section>(2, Groups{}, group_configs));
    auto out_matches = Section::GroupMatches(std::move(sections), matches);
    VerifyMatches(out_matches, expected_relevances);
  };

  // Given no matches, should return no matches.
  test({}, {});

  // Matches not qualifying for the section should not be added.
  test({CreateMatch(1, omnibox::GROUP_SEARCH)}, {});
}

TEST(AutocompleteGrouperGroupsTest, SortingUsesProperComparator) {
  class TestZpsSection : public ZpsSection {
   public:
    // Up to 2 items of the following types.
    explicit TestZpsSection(omnibox::GroupConfigMap& group_configs)
        : ZpsSection(2,
                     {{1, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX},
                      {1, omnibox::GROUP_MOBILE_CLIPBOARD},
                      {1, omnibox::GROUP_MOBILE_MOST_VISITED},
                      {1, omnibox::GROUP_VISITED_DOC_RELATED},
                      {1, omnibox::GROUP_RELATED_QUERIES}},
                     group_configs) {}
  };

  auto test = [](ACMatches matches, std::vector<int> expected_relevances) {
    PSections sections;
    omnibox::GroupConfigMap group_configs;
    sections.push_back(std::make_unique<TestZpsSection>(group_configs));
    auto out_matches = Section::GroupMatches(std::move(sections), matches);
    VerifyMatches(out_matches, expected_relevances);
  };

  {
    SCOPED_TRACE("Confirm appropriate comparator is used to sort suggestions");
    test({CreateMatch(1, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX),
          CreateMatch(2, omnibox::GROUP_MOBILE_CLIPBOARD),
          CreateMatch(3, omnibox::GROUP_MOBILE_MOST_VISITED),
          CreateMatch(4, omnibox::GROUP_VISITED_DOC_RELATED),
          CreateMatch(5, omnibox::GROUP_RELATED_QUERIES)},
         {1, 2});
  }
}

// Tests the groups, limits, and rules for the ZPS section.
TEST(AutocompleteGrouperSectionsTest, ZpsSection) {
  auto test = [](ACMatches matches, std::vector<int> expected_relevances) {
    PSections sections;
    omnibox::GroupConfigMap group_configs;
    sections.push_back(std::make_unique<DesktopNTPZpsSection>(group_configs));
    auto out_matches = Section::GroupMatches(std::move(sections), matches);
    VerifyMatches(out_matches, expected_relevances);
  };

  {
    SCOPED_TRACE("Given no matches, should return no matches.");
    test({}, {});
  }

  {
    SCOPED_TRACE("Matches that qualify for no groups should not be added.");
    test(
        {
            CreateMatch(100, omnibox::GROUP_DOCUMENT),
            CreateMatch(99, omnibox::GROUP_SEARCH),
            CreateMatch(98, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
        },
        {98});
  }

  {
    SCOPED_TRACE(
        "Matches should be ranked by group, not relevance or add order.");
    ACMatches matches;
    // `GROUP_TRENDS` matches come 3rd and should not be added.
    for (size_t i = 0; i < 10; ++i) {
      matches.push_back(CreateMatch(90 - i, omnibox::GROUP_TRENDS));
    }
    // `GROUP_PERSONALIZED_ZERO_SUGGEST` matches come 1st and should be added.
    for (size_t i = 0; i < 10; ++i) {
      matches.push_back(
          CreateMatch(80 - i, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST));
    }
    // `GROUP_PREVIOUS_SEARCH_RELATED` matches should not be added.
    for (size_t i = 0; i < 10; ++i) {
      matches.push_back(
          CreateMatch(70 - i, omnibox::GROUP_PREVIOUS_SEARCH_RELATED));
    }
    std::vector<int> expected_relevances;
    for (size_t i = 80; i > 80 - 8; --i) {
      expected_relevances.push_back(i);
    }
    test(matches, expected_relevances);
  }

  {
    SCOPED_TRACE("Matches should be added up to their group limit.");
    ACMatches matches;
    for (size_t i = 0; i < 10; ++i) {
      matches.push_back(
          CreateMatch(80 - i, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST));
    }
    std::vector<int> expected_relevances;
    for (size_t i = 80; i > 80 - 8; --i) {
      expected_relevances.push_back(i);
    }
    test(matches, expected_relevances);
  }

  {
    SCOPED_TRACE("Matches should be added up to the section limit.");
    ACMatches matches;
    // `GROUP_TRENDS` matches should be added up to the remaining section limit
    // (3).
    for (size_t i = 0; i < 10; ++i) {
      matches.push_back(CreateMatch(90 - i, omnibox::GROUP_TRENDS));
    }
    // `GROUP_PERSONALIZED_ZERO_SUGGEST` matches should all be added.
    for (size_t i = 0; i < 5; ++i) {
      matches.push_back(
          CreateMatch(80 - i, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST));
    }
    test(matches, {80, 79, 78, 77, 76, 90, 89, 88});
  }
}

// Tests the groups, limits, and rules for the Desktop non-ZPS section.
TEST(AutocompleteGrouperSectionsTest, DesktopNonZpsSection) {
  auto test = [](ACMatches matches, std::vector<int> expected_relevances) {
    PSections sections;
    omnibox::GroupConfigMap group_configs;
    sections.push_back(std::make_unique<DesktopNonZpsSection>(group_configs));
    auto out_matches = Section::GroupMatches(std::move(sections), matches);
    VerifyMatches(out_matches, expected_relevances);
  };

  {
    SCOPED_TRACE("Given no matches, should return no matches.");
    test({}, {});
  }

  {
    SCOPED_TRACE("Rank groups: default > starter pack > searches > navs.");
    test(
        {
            CreateMatch(100, omnibox::GROUP_SEARCH),
            CreateMatch(99, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(98, omnibox::GROUP_SEARCH),
            CreateMatch(96, omnibox::GROUP_SEARCH, true),
            // Only the 1st default-able suggestion should be ranked 1st.
            CreateMatch(97, omnibox::GROUP_OTHER_NAVS, true),
            CreateMatch(95, omnibox::GROUP_STARTER_PACK),
        },
        {96, 95, 100, 98, 99, 97});
  }

  {
    SCOPED_TRACE("Matches that qualify for no groups, should not be added.");
    test(
        {
            CreateMatch(100, omnibox::GROUP_SEARCH, true),
            CreateMatch(99, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(98, omnibox::GROUP_MOBILE_CLIPBOARD),
        },
        {100});
  }

  {
    SCOPED_TRACE(
        "A match that qualifies for multiple groups, should only be added "
        "once.");
    test(
        {
            CreateMatch(100, omnibox::GROUP_SEARCH, true),
        },
        {100});
  }

  {
    SCOPED_TRACE("Show at least 1 search.");
    test(
        {
            CreateMatch(100, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(99, omnibox::GROUP_OTHER_NAVS, true),
            CreateMatch(98, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(97, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(96, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(95, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(94, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(93, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(92, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(91, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(90, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(89, omnibox::GROUP_SEARCH),
            CreateMatch(88, omnibox::GROUP_SEARCH),
            CreateMatch(87, omnibox::GROUP_SEARCH),
        },
        {99, 89, 100, 98, 97, 96, 95, 94});
  }

  {
    SCOPED_TRACE("Show at least 1 search unless there are no searches.");
    test(
        {
            CreateMatch(100, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(99, omnibox::GROUP_OTHER_NAVS, true),
            CreateMatch(98, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(97, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(96, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(95, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(94, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(93, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(92, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(91, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(90, omnibox::GROUP_OTHER_NAVS),
        },
        {99, 100, 98, 97, 96, 95, 94, 93});
  }

  {
    SCOPED_TRACE(
        "Show at least 1 search; if the default is a search, that counts too.");
    test(
        {
            CreateMatch(100, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(99, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(98, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(97, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(96, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(95, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(94, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(93, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(92, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(91, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(90, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(89, omnibox::GROUP_SEARCH, true),
            CreateMatch(88, omnibox::GROUP_SEARCH),
            CreateMatch(87, omnibox::GROUP_SEARCH),
        },
        {89, 100, 99, 98, 97, 96, 95, 94});
  }

  {
    SCOPED_TRACE("Show at most 8 suggestions.");
    test(
        {
            CreateMatch(100, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(99, omnibox::GROUP_SEARCH),
            CreateMatch(98, omnibox::GROUP_SEARCH, true),
            CreateMatch(97, omnibox::GROUP_SEARCH),
            CreateMatch(96, omnibox::GROUP_SEARCH),
            CreateMatch(95, omnibox::GROUP_SEARCH),
            CreateMatch(94, omnibox::GROUP_SEARCH),
            CreateMatch(93, omnibox::GROUP_SEARCH),
            CreateMatch(92, omnibox::GROUP_SEARCH),
            CreateMatch(91, omnibox::GROUP_SEARCH),
            CreateMatch(90, omnibox::GROUP_SEARCH),
        },
        {98, 99, 97, 96, 95, 94, 93, 100});
  }

  {
    SCOPED_TRACE(
        "Show at most 8 suggestions; unless there are no navs, then show up to "
        "10.");
    test(
        {
            CreateMatch(100, omnibox::GROUP_SEARCH),
            CreateMatch(99, omnibox::GROUP_SEARCH),
            CreateMatch(98, omnibox::GROUP_SEARCH, true),
            CreateMatch(97, omnibox::GROUP_SEARCH),
            CreateMatch(96, omnibox::GROUP_SEARCH),
            CreateMatch(95, omnibox::GROUP_SEARCH),
            CreateMatch(94, omnibox::GROUP_SEARCH),
            CreateMatch(93, omnibox::GROUP_SEARCH),
            CreateMatch(92, omnibox::GROUP_SEARCH),
            CreateMatch(91, omnibox::GROUP_SEARCH),
            CreateMatch(90, omnibox::GROUP_SEARCH),
        },
        {98, 100, 99, 97, 96, 95, 94, 93, 92, 91});
  }

  {
    SCOPED_TRACE(
        "Show at most 8 suggestions; unless there are no navs, then show up to "
        "10, even if there are navs after the 10th suggestion.");
    test(
        {
            CreateMatch(100, omnibox::GROUP_SEARCH),
            CreateMatch(99, omnibox::GROUP_SEARCH),
            CreateMatch(98, omnibox::GROUP_SEARCH, true),
            CreateMatch(97, omnibox::GROUP_SEARCH),
            CreateMatch(96, omnibox::GROUP_SEARCH),
            CreateMatch(95, omnibox::GROUP_SEARCH),
            CreateMatch(94, omnibox::GROUP_SEARCH),
            CreateMatch(93, omnibox::GROUP_SEARCH),
            CreateMatch(92, omnibox::GROUP_SEARCH),
            CreateMatch(91, omnibox::GROUP_SEARCH),
            CreateMatch(90, omnibox::GROUP_SEARCH),
            CreateMatch(89, omnibox::GROUP_OTHER_NAVS),
        },
        {98, 100, 99, 97, 96, 95, 94, 93, 92, 91});
  }

  {
    SCOPED_TRACE(
        "Show at most 8 suggestions; unless the 10th suggestion is the 1st "
        "nav, then show up to 9.");
    test(
        {
            CreateMatch(100, omnibox::GROUP_SEARCH),
            CreateMatch(99, omnibox::GROUP_SEARCH),
            CreateMatch(98, omnibox::GROUP_SEARCH, true),
            CreateMatch(97, omnibox::GROUP_SEARCH),
            CreateMatch(96, omnibox::GROUP_SEARCH),
            CreateMatch(95, omnibox::GROUP_SEARCH),
            CreateMatch(94, omnibox::GROUP_SEARCH),
            CreateMatch(93, omnibox::GROUP_SEARCH),
            CreateMatch(92, omnibox::GROUP_SEARCH),
            CreateMatch(91, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(90, omnibox::GROUP_SEARCH),
        },
        {98, 100, 99, 97, 96, 95, 94, 93, 92});
  }

  {
    SCOPED_TRACE(
        "Show at most 8 suggestions if the 9th suggestion is the 1st nav.");
    test(
        {
            CreateMatch(100, omnibox::GROUP_SEARCH),
            CreateMatch(99, omnibox::GROUP_SEARCH),
            CreateMatch(98, omnibox::GROUP_SEARCH, true),
            CreateMatch(97, omnibox::GROUP_SEARCH),
            CreateMatch(96, omnibox::GROUP_SEARCH),
            CreateMatch(95, omnibox::GROUP_SEARCH),
            CreateMatch(94, omnibox::GROUP_SEARCH),
            CreateMatch(93, omnibox::GROUP_SEARCH),
            CreateMatch(92, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(91, omnibox::GROUP_SEARCH),
            CreateMatch(90, omnibox::GROUP_SEARCH),
        },
        {98, 100, 99, 97, 96, 95, 94, 93});
  }

  {
    SCOPED_TRACE(
        "Show at most 8 suggestions if the default suggestion is a nav.");
    test(
        {
            CreateMatch(100, omnibox::GROUP_OTHER_NAVS, true),
            CreateMatch(99, omnibox::GROUP_SEARCH),
            CreateMatch(98, omnibox::GROUP_SEARCH),
            CreateMatch(97, omnibox::GROUP_SEARCH),
            CreateMatch(96, omnibox::GROUP_SEARCH),
            CreateMatch(95, omnibox::GROUP_SEARCH),
            CreateMatch(94, omnibox::GROUP_SEARCH),
            CreateMatch(93, omnibox::GROUP_SEARCH),
            CreateMatch(92, omnibox::GROUP_SEARCH),
            CreateMatch(91, omnibox::GROUP_SEARCH),
            CreateMatch(90, omnibox::GROUP_SEARCH),
        },
        {100, 99, 98, 97, 96, 95, 94, 93});
  }

  {
    SCOPED_TRACE("Show at most 1 default.");
    test(
        {
            CreateMatch(100, omnibox::GROUP_OTHER_NAVS, true),
            CreateMatch(99, omnibox::GROUP_OTHER_NAVS, true),
            CreateMatch(98, omnibox::GROUP_OTHER_NAVS, true),
            CreateMatch(97, omnibox::GROUP_STARTER_PACK, true),
            CreateMatch(96, omnibox::GROUP_SEARCH, true),
            CreateMatch(95, omnibox::GROUP_SEARCH, true),
        },
        {100, 97, 96, 95, 99, 98});
  }

  {
    SCOPED_TRACE("Group history clusters with searches.");
    test(
        {
            CreateMatch(100, omnibox::GROUP_SEARCH, true),
            CreateMatch(99, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(98, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(97, omnibox::GROUP_SEARCH),
            CreateMatch(96, omnibox::GROUP_HISTORY_CLUSTER),
            CreateMatch(95, omnibox::GROUP_SEARCH),
        },
        {100, 97, 96, 95, 99, 98});
  }

  {
    SCOPED_TRACE("Show at most 1 history cluster.");
    test(
        {
            CreateMatch(100, omnibox::GROUP_SEARCH, true),
            CreateMatch(99, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(98, omnibox::GROUP_OTHER_NAVS),
            CreateMatch(97, omnibox::GROUP_SEARCH),
            CreateMatch(96, omnibox::GROUP_HISTORY_CLUSTER),
            CreateMatch(95, omnibox::GROUP_HISTORY_CLUSTER),
            CreateMatch(94, omnibox::GROUP_SEARCH),
        },
        {100, 97, 96, 94, 99, 98});
  }

  {
    SCOPED_TRACE("History cluster should count against search limit.");
    test(
        {
            CreateMatch(100, omnibox::GROUP_SEARCH, true),
            CreateMatch(99, omnibox::GROUP_SEARCH),
            CreateMatch(98, omnibox::GROUP_SEARCH),
            CreateMatch(97, omnibox::GROUP_SEARCH),
            CreateMatch(96, omnibox::GROUP_HISTORY_CLUSTER),
            CreateMatch(95, omnibox::GROUP_SEARCH),
            CreateMatch(94, omnibox::GROUP_SEARCH),
            CreateMatch(93, omnibox::GROUP_SEARCH),
            CreateMatch(92, omnibox::GROUP_SEARCH),
            CreateMatch(91, omnibox::GROUP_SEARCH),
            CreateMatch(90, omnibox::GROUP_SEARCH),
            CreateMatch(89, omnibox::GROUP_SEARCH),
            CreateMatch(88, omnibox::GROUP_SEARCH),
            CreateMatch(87, omnibox::GROUP_SEARCH),
            CreateMatch(86, omnibox::GROUP_SEARCH),
            CreateMatch(85, omnibox::GROUP_SEARCH),
            CreateMatch(84, omnibox::GROUP_SEARCH),
            CreateMatch(83, omnibox::GROUP_SEARCH),
        },
        {100, 99, 98, 97, 96, 95, 94, 93, 92, 91});
  }
}

// Tests the groups, limits, and rules for the Android SRP ZPS section.
TEST(AutocompleteGrouperSectionsTest, AndroidSRPZpsSection) {
  auto test = [](ACMatches matches, std::vector<int> expected_relevances) {
    PSections sections;
    omnibox::GroupConfigMap group_configs;
    sections.push_back(std::make_unique<AndroidSRPZpsSection>(group_configs));
    auto out_matches = Section::GroupMatches(std::move(sections), matches);
    VerifyMatches(out_matches, expected_relevances);
  };

  {
    SCOPED_TRACE("Given no matches, should return no matches.");
    test({}, {});
  }
  {
    SCOPED_TRACE("Android/ZPS with extra searches.");
    // Verify that the Clipboard suggestion is retained on top.
    test(
        {
            CreateMatch(100, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST, true),
            CreateMatch(99, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(98, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(97, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(96, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(95, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(94, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(93, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(92, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(91, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(89, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(88, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(87, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(86, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(85, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(84, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
        },
        {100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88, 87, 86});
  }
  {
    SCOPED_TRACE("Android/ZPS with Clipboard entries.");
    // Verify that the Clipboard suggestion is retained on top.
    test(
        {
            CreateMatch(100, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST, true),
            CreateMatch(99, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(98, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(97, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(96, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(95, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(94, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(93, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(92, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(91, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(89, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(88, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(87, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(86, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(85, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(84, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(3, omnibox::GROUP_MOBILE_CLIPBOARD),
            // Bogus, repetitive, only one allowed.
            CreateMatch(2, omnibox::GROUP_MOBILE_CLIPBOARD),
            CreateMatch(1, omnibox::GROUP_MOBILE_CLIPBOARD),
        },
        {3, 100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88, 87});
  }
  {
    SCOPED_TRACE("Android/ZPS with Search Ready Omnibox.");
    // Verify that the Clipboard suggestion is retained on top.
    test(
        {
            CreateMatch(100, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST, true),
            CreateMatch(99, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(98, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(97, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(96, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(95, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(94, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(93, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(92, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(91, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(89, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(88, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(87, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(86, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(85, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(84, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            // Not allowed.
            CreateMatch(2, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX),
            CreateMatch(1, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX),
            CreateMatch(0, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX),
        },
        {2, 100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88, 87});
  }
  {
    SCOPED_TRACE("Android/ZPS on SRP with recent searches only.");
    // Verify that the Clipboard suggestion is retained on top.
    test(
        {
            CreateMatch(100, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST, true),
            CreateMatch(99, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(98, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(97, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(96, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(95, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(94, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(93, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(92, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(91, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(89, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(88, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(87, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(86, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(85, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(84, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(2, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX),
        },
        {2, 100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88, 87});
  }
  {
    SCOPED_TRACE("Android/ZPS with MV Tiles.");
    // Verify that the Clipboard suggestion is retained on top.
    test(
        {
            CreateMatch(100, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST, true),
            CreateMatch(99, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(98, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(97, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(96, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(95, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(94, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(93, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(92, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(91, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(89, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(88, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(87, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(86, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(85, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(84, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            // Not allowed.
            CreateMatch(4, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(3, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(2, omnibox::GROUP_MOBILE_MOST_VISITED),
        },
        {100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88, 87, 86});
  }
  {
    SCOPED_TRACE("Android/ZPS with multiple auxiliary suggestions.");
    // Verify that the Clipboard suggestion is retained on top.
    test(
        {
            CreateMatch(100, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST, true),
            CreateMatch(99, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(98, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(97, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(96, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(95, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(94, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(93, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(92, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(91, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(89, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(88, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(87, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(86, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(85, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(84, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            // SRO should always be shown first, despite low relevance.
            // Only one item permitted.
            CreateMatch(2, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX),
            CreateMatch(1, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX),
            // Clipboard should always be shown after SRO, if both are present.
            // Only one item permitted.
            CreateMatch(20, omnibox::GROUP_MOBILE_CLIPBOARD),
            CreateMatch(19, omnibox::GROUP_MOBILE_CLIPBOARD),
            // MV Tiles should always be on the third position if both SRO and
            // Clipboard are present.
            // Currently only one item is permitted.
            CreateMatch(40, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(39, omnibox::GROUP_MOBILE_MOST_VISITED),
        },
        // Observe that PERSONALIZED_ZERO_SUGGEST and VISITED_DOC suggestions
        // are grouped together. VISITED_DOC_RELATED are prioritized over the
        // PERSONALIZED_ZERO_SUGGEST because these are more context relevant.
        {2, 20, 99, 97, 95, 93, 91, 89, 87, 85, 100, 98, 96, 94, 92});
  }
  {
    SCOPED_TRACE("No Inspire Me content shown in the core ZPS content");
    test(
        {
            CreateMatch(100, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(99, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(98, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(97, omnibox::GROUP_RELATED_QUERIES),
            CreateMatch(96, omnibox::GROUP_TRENDS),
            CreateMatch(95, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(94, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(93, omnibox::GROUP_RELATED_QUERIES),
            CreateMatch(92, omnibox::GROUP_TRENDS),
            CreateMatch(91, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(89, omnibox::GROUP_RELATED_QUERIES),
            CreateMatch(88, omnibox::GROUP_TRENDS),
            CreateMatch(87, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(86, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(85, omnibox::GROUP_RELATED_QUERIES),
            CreateMatch(84, omnibox::GROUP_TRENDS),
            // Auxiliary suggestions.
            CreateMatch(2, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX),
            CreateMatch(3, omnibox::GROUP_MOBILE_CLIPBOARD),
            // Not allowed.
            CreateMatch(4, omnibox::GROUP_MOBILE_MOST_VISITED),
        },
        {2, 3, 99, 95, 91, 87, 100, 98, 94, 90, 86});
  }
}

// Tests the groups, limits, and rules for the Android Web ZPS section.
TEST(AutocompleteGrouperSectionsTest, AndroidWebZpsSection) {
  auto test = [](ACMatches matches, std::vector<int> expected_relevances) {
    PSections sections;
    omnibox::GroupConfigMap group_configs;
    sections.push_back(std::make_unique<AndroidWebZpsSection>(group_configs));
    auto out_matches = Section::GroupMatches(std::move(sections), matches);
    VerifyMatches(out_matches, expected_relevances);
  };

  {
    SCOPED_TRACE("Given no matches, should return no matches.");
    test({}, {});
  }
  {
    SCOPED_TRACE("Android/ZPS with extra searches.");
    // Verify that the Clipboard suggestion is retained on top.
    test(
        {
            CreateMatch(100, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST, true),
            CreateMatch(99, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(98, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(97, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(96, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(95, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(94, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(93, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(92, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(91, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(89, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(88, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(87, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(86, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(85, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(84, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
        },
        {100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88, 87, 86});
  }
  {
    SCOPED_TRACE("Android/ZPS with Clipboard entries.");
    // Verify that the Clipboard suggestion is retained on top.
    test(
        {
            CreateMatch(100, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST, true),
            CreateMatch(99, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(98, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(97, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(96, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(95, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(94, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(93, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(92, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(91, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(89, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(88, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(87, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(86, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(85, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(84, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(3, omnibox::GROUP_MOBILE_CLIPBOARD),
            // Bogus, repetitive, only one allowed.
            CreateMatch(2, omnibox::GROUP_MOBILE_CLIPBOARD),
            CreateMatch(1, omnibox::GROUP_MOBILE_CLIPBOARD),
        },
        {3, 100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88, 87});
  }
  {
    SCOPED_TRACE("Android/ZPS with Search Ready Omnibox.");
    // Verify that the Clipboard suggestion is retained on top.
    test(
        {
            CreateMatch(100, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST, true),
            CreateMatch(99, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(98, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(97, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(96, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(95, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(94, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(93, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(92, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(91, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(89, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(88, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(87, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(86, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(85, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(84, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(2, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX),
            // Bogus, repetitive, only one allowed.
            CreateMatch(1, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX),
            CreateMatch(0, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX),
        },
        {2, 100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88, 87});
  }
  {
    SCOPED_TRACE("Android/ZPS on Web with recent searches only.");
    // Verify that the Clipboard suggestion is retained on top.
    test(
        {
            CreateMatch(100, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST, true),
            CreateMatch(99, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(98, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(97, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(96, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(95, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(94, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(93, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(92, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(91, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(89, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(88, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(87, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(86, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(85, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(84, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(2, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX),
        },
        {2, 100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88, 87});
  }
  {
    SCOPED_TRACE("Android/ZPS with MV Tiles.");
    // Verify that the Clipboard suggestion is retained on top.
    test(
        {
            CreateMatch(100, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST, true),
            CreateMatch(99, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(98, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(97, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(96, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(95, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(94, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(93, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(92, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(91, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(89, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(88, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(87, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(86, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(85, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(84, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            // Slotted in horizontal render group.
            CreateMatch(4, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(3, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(2, omnibox::GROUP_MOBILE_MOST_VISITED),
        },
        {4, 3, 2, 100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88, 87});
  }
  {
    SCOPED_TRACE("Android/ZPS with multiple auxiliary suggestions.");
    // Verify that the Clipboard suggestion is retained on top.
    test(
        {
            CreateMatch(100, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST, true),
            CreateMatch(99, omnibox::GROUP_VISITED_DOC_RELATED),
            CreateMatch(98, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(97, omnibox::GROUP_VISITED_DOC_RELATED),
            CreateMatch(96, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(95, omnibox::GROUP_VISITED_DOC_RELATED),
            CreateMatch(94, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(93, omnibox::GROUP_VISITED_DOC_RELATED),
            CreateMatch(92, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(91, omnibox::GROUP_VISITED_DOC_RELATED),
            CreateMatch(90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(89, omnibox::GROUP_VISITED_DOC_RELATED),
            CreateMatch(88, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(87, omnibox::GROUP_VISITED_DOC_RELATED),
            CreateMatch(86, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(85, omnibox::GROUP_VISITED_DOC_RELATED),
            CreateMatch(84, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            // SRO should always be shown first, despite low relevance.
            // Only one item permitted.
            CreateMatch(2, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX),
            CreateMatch(1, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX),
            // Clipboard should always be shown after SRO, if both are present.
            // Only one item permitted.
            CreateMatch(20, omnibox::GROUP_MOBILE_CLIPBOARD),
            CreateMatch(19, omnibox::GROUP_MOBILE_CLIPBOARD),
            // MV Tiles should always be on the third position if both SRO and
            // Clipboard are present.
            // Slotted in horizontal render group.
            CreateMatch(40, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(39, omnibox::GROUP_MOBILE_MOST_VISITED),
        },
        // Observe that PERSONALIZED_ZERO_SUGGEST and VISITED_DOC suggestions
        // are grouped together. VISITED_DOC_RELATED are prioritized over the
        // PERSONALIZED_ZERO_SUGGEST because these are more context relevant.
        {2, 20, 40, 39, 99, 97, 95, 93, 91, 89, 87, 85, 100, 98, 96, 94});
  }
  {
    SCOPED_TRACE("No Inspire Me content shown in the core ZPS content");
    test(
        {
            CreateMatch(100, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(99, omnibox::GROUP_VISITED_DOC_RELATED),
            CreateMatch(98, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(97, omnibox::GROUP_RELATED_QUERIES),
            CreateMatch(96, omnibox::GROUP_TRENDS),
            CreateMatch(95, omnibox::GROUP_VISITED_DOC_RELATED),
            CreateMatch(94, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(93, omnibox::GROUP_RELATED_QUERIES),
            CreateMatch(92, omnibox::GROUP_TRENDS),
            CreateMatch(91, omnibox::GROUP_VISITED_DOC_RELATED),
            CreateMatch(90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(89, omnibox::GROUP_RELATED_QUERIES),
            CreateMatch(88, omnibox::GROUP_TRENDS),
            CreateMatch(87, omnibox::GROUP_VISITED_DOC_RELATED),
            CreateMatch(86, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(85, omnibox::GROUP_RELATED_QUERIES),
            CreateMatch(84, omnibox::GROUP_TRENDS),
            // Auxiliary suggestions.
            CreateMatch(2, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX),
            CreateMatch(3, omnibox::GROUP_MOBILE_CLIPBOARD),
            CreateMatch(4, omnibox::GROUP_MOBILE_MOST_VISITED),
        },
        {2, 3, 4, 99, 95, 91, 87, 100, 98, 94, 90, 86});
  }
}

// Tests the groups, limits, and rules for the Android NTP ZPS+Inspire Me.
TEST(AutocompleteGrouperSectionsTest, AndroidNTPZpsSection_withInspireMe) {
  auto test = [](ACMatches matches, std::vector<int> expected_relevances) {

    PSections sections;
    omnibox::GroupConfigMap group_configs;
    sections.push_back(std::make_unique<AndroidNTPZpsSection>(group_configs));
    auto out_matches = Section::GroupMatches(std::move(sections), matches);
    VerifyMatches(out_matches, expected_relevances);
  };

  {
    SCOPED_TRACE("Given no matches, should return no matches.");
    test({}, {});
  }
  {
    SCOPED_TRACE("Given no InspireMe matches, should return no matches.");
    // Verify that the Clipboard suggestion is retained on top.
    test(
        {
            // PSUGGEST to show on the NTP ZPS.
            CreateMatch(100, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(99, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(98, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(97, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(96, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(95, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(94, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(93, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(92, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(91, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(89, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(88, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(87, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(86, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(85, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(84, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
        },
        {100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88, 87, 86});
  }
  {
    SCOPED_TRACE("Clipboard suggestion is always shown when available.");
    test(
        {
            CreateMatch(3, omnibox::GROUP_MOBILE_CLIPBOARD),
            // Auxiliary matches not valid for NTP ZPS.
            CreateMatch(2, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX),
            CreateMatch(1, omnibox::GROUP_MOBILE_MOST_VISITED),
            // PSUGGEST to show on the NTP ZPS.
            CreateMatch(100, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(99, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(98, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(97, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(96, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(95, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(94, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(93, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(92, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(91, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(89, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(88, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(87, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(86, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(85, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(84, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
        },
        {3, 100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88, 87, 86});
  }
  {
    SCOPED_TRACE("No Trending Queries Backfill");
    // Verify that Trending queries don't backfill unoccupied Related queries
    // slots.
    // Must offer less PREVIOUS_SEARCH_RELATED than MAX_PREVIOUS_SEARCH_RELATED,
    // and more TRENDS than MAX_TRENDING_QUERIES.
    test(
        {
            CreateMatch(20, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(19, omnibox::GROUP_TRENDS),
            CreateMatch(18, omnibox::GROUP_TRENDS),
            CreateMatch(17, omnibox::GROUP_TRENDS),
            CreateMatch(16, omnibox::GROUP_TRENDS),
            CreateMatch(15, omnibox::GROUP_TRENDS),
        },
        {19, 18, 17, 16, 15});
  }
  {
    SCOPED_TRACE("No Related Queries Backfill");
    // Verify that Related queries are completely ignored by Inspire Me.
    // Must offer less TRENDS than MAX_TRENDING_QUERIES, and more
    // PREVIOUS_SEARCH_RELATED than MAX_PREVIOUS_SEARCH_RELATED.
    test(
        {
            CreateMatch(20, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(19, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(18, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(17, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(16, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(15, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(14, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(13, omnibox::GROUP_TRENDS),
        },
        {13});
  }
  {
    SCOPED_TRACE("Conform to Limits");
    test(
        {
            CreateMatch(20, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(19, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(18, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(17, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(16, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(15, omnibox::GROUP_TRENDS),
            CreateMatch(14, omnibox::GROUP_TRENDS),
            CreateMatch(13, omnibox::GROUP_TRENDS),
            CreateMatch(12, omnibox::GROUP_TRENDS),
            CreateMatch(11, omnibox::GROUP_TRENDS),
            CreateMatch(10, omnibox::GROUP_TRENDS),
        },
        // No more than MAX_PREVIOUS_SEARCH_RELATED + MAX_TRENDING_QUERIES.
        {15, 14, 13, 12, 11});
  }
}

TEST(AutocompleteGrouperSectionsTest, AndroidWebZpsSection_mostVisitedTiles) {
  const ACMatches tail = {
      CreateMatch(105, omnibox::GROUP_VISITED_DOC_RELATED),
      CreateMatch(104, omnibox::GROUP_VISITED_DOC_RELATED),
      CreateMatch(103, omnibox::GROUP_VISITED_DOC_RELATED),
      CreateMatch(102, omnibox::GROUP_VISITED_DOC_RELATED),
      CreateMatch(101, omnibox::GROUP_VISITED_DOC_RELATED),
      CreateMatch(10, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
      CreateMatch(9, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
      CreateMatch(8, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
      CreateMatch(7, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
      CreateMatch(6, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
      CreateMatch(5, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
      CreateMatch(4, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
      CreateMatch(3, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
      CreateMatch(2, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
      CreateMatch(1, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
  };

  auto test = [&tail](ACMatches matches, bool append_tail_suggestions,
                      std::vector<int> expected_relevances) {
    if (append_tail_suggestions) {
      matches.insert(matches.end(), tail.begin(), tail.end());
    }

    PSections sections;
    omnibox::GroupConfigMap group_configs;
    sections.push_back(std::make_unique<AndroidWebZpsSection>(group_configs));
    auto out_matches = Section::GroupMatches(std::move(sections), matches);
    VerifyMatches(out_matches, expected_relevances);
  };

  {
    SCOPED_TRACE("No Search Ready Omnibox. No MV Tiles.");
    test({}, true, {105, 104, 103, 102, 101, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1});
  }

  {
    SCOPED_TRACE("Search Ready Omnibox, no MV Tiles.");
    // Verify that the Clipboard suggestion is retained on top.
    test(
        {
            CreateMatch(200, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX),
        },
        true, {200, 105, 104, 103, 102, 101, 10, 9, 8, 7, 6, 5, 4, 3, 2});
  }

  {
    // This test verifies that when we append MV Tiles, we don't do this at
    // expense of Search suggestions.
    SCOPED_TRACE("Search Ready Omnibox and 1 MV Tile.");
    test(
        {
            CreateMatch(300, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX),
            CreateMatch(200, omnibox::GROUP_MOBILE_MOST_VISITED),
        },
        true, {300, 200, 105, 104, 103, 102, 101, 10, 9, 8, 7, 6, 5, 4, 3});
  }

  {
    // This test verifies that if we have no suggestions, we don't spend the
    // limit on excessive MV Tiles.
    SCOPED_TRACE("Excessive number of MV Tiles.");
    test(
        {
            CreateMatch(215, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(214, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(213, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(212, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(211, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(210, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(209, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(208, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(207, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(206, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(205, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(204, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(203, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(202, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(201, omnibox::GROUP_MOBILE_MOST_VISITED),
        },
        false, {215, 214, 213, 212, 211, 210, 209, 208, 207, 206});
  }

  {
    // This test verifies that if we have both MV Tiles and suggestions, we
    // don't lose search suggestions slots on MV tiles.
    SCOPED_TRACE("Search Ready Omnibox and many MV Tiles.");
    test(
        {
            CreateMatch(300, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX),
            CreateMatch(215, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(214, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(213, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(212, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(211, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(210, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(209, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(208, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(207, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(206, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(205, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(204, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(203, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(202, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(201, omnibox::GROUP_MOBILE_MOST_VISITED),
        },
        true,
        {// SRO
         300,
         // 10 MV Tiles
         215, 214, 213, 212, 211, 210, 209, 208, 207, 206,
         // 13 search suggestions.
         105, 104, 103, 102, 101, 10, 9, 8, 7, 6, 5, 4, 3});
  }
}

TEST(AutocompleteGrouperSectionsTest, IOSNTPZpsSection) {
  auto test = [](ACMatches matches, std::vector<int> expected_relevances) {
    size_t max_trending_queries = 3;
    size_t max_psuggest_queries = 3;
    PSections sections;
    omnibox::GroupConfigMap group_configs;
    sections.push_back(std::make_unique<IOSNTPZpsSection>(
        max_trending_queries, max_psuggest_queries, group_configs));
    auto out_matches = Section::GroupMatches(std::move(sections), matches);
    VerifyMatches(out_matches, expected_relevances);
  };

  {
    SCOPED_TRACE("Given no matches, should return no matches.");
    test({}, {});
  }

  {
    SCOPED_TRACE(
        "Given no trend matches and only psuggest, should only display "
        "psuggest following the psuggest count limit");
    test({CreateMatch(100, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
          CreateMatch(99, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
          CreateMatch(98, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
          CreateMatch(97, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST)},
         {100, 99, 98});
  }

  {
    SCOPED_TRACE(
        "Given no psuggest matches and only trends, should only display trends "
        "following the trends count limit");
    test({CreateMatch(100, omnibox::GROUP_TRENDS),
          CreateMatch(99, omnibox::GROUP_TRENDS),
          CreateMatch(98, omnibox::GROUP_TRENDS),
          CreateMatch(97, omnibox::GROUP_TRENDS)},
         {100, 99, 98});
  }

  {
    SCOPED_TRACE(
        "Given both psuggest and trends matches, should display both groups "
        "following their count limit");
    test({CreateMatch(100, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
          CreateMatch(99, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
          CreateMatch(98, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
          CreateMatch(97, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
          CreateMatch(96, omnibox::GROUP_TRENDS),
          CreateMatch(95, omnibox::GROUP_TRENDS),
          CreateMatch(94, omnibox::GROUP_TRENDS),
          CreateMatch(93, omnibox::GROUP_TRENDS)},
         {100, 99, 98, 96, 95, 94});
  }
}
