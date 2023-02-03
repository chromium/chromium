// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_grouper_sections.h"

#include <iterator>
#include <memory>

#include "base/ranges/ranges.h"
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

// Tests the groups, limits, and rules for the ZPS section.
TEST(AutocompleteGrouperSectionsTest, ZpsSection) {
  auto test = [](ACMatches matches, std::vector<int> expected_relevances) {
    PSections sections;
    omnibox::GroupConfigMap group_configs;
    sections.push_back(std::make_unique<DesktopZpsSection>(group_configs));
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
    // `GROUP_PERSONALIZED_ZERO_SUGGEST` matches come 2nd and should not be
    // added.
    for (size_t i = 0; i < 10; ++i) {
      matches.push_back(
          CreateMatch(80 - i, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST));
    }
    // `GROUP_PREVIOUS_SEARCH_RELATED` matches come 1st and should be added.
    for (size_t i = 0; i < 10; ++i) {
      matches.push_back(
          CreateMatch(70 - i, omnibox::GROUP_PREVIOUS_SEARCH_RELATED));
    }
    std::vector<int> expected_relevances;
    for (size_t i = 70; i > 70 - 8; --i) {
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
