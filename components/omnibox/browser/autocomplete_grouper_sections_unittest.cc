// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_grouper_sections.h"

#include <iterator>
#include <memory>

#include "base/test/scoped_feature_list.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
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
  std::ranges::transform(matches, std::back_inserter(relevances),
                         [&](const auto& match) { return match.relevance; });

  EXPECT_THAT(relevances, testing::ElementsAreArray(expected_relevances));
}

void VerifyMatches(
    const ACMatches& matches,
    const std::vector<std::pair<int, omnibox::GroupId>>& expected) {
  std::vector<std::pair<int, omnibox::GroupId>> actual;
  std::ranges::transform(
      matches, std::back_inserter(actual), [](const auto& match) {
        return std::make_pair(match.relevance,
                              match.suggestion_group_id.value());
      });
  EXPECT_THAT(actual, testing::ElementsAreArray(expected));
}

}  // namespace

// Tests rules for Section.
TEST(AutocompleteGrouperSectionsTest, Section) {
  class TestSection : public Section {
   public:
    // Up to 1 item of the following types.
    explicit TestSection(omnibox::GroupConfigMap& group_configs)
        : Section(
              1,
              {
                  Group(
                      1,
                      {
                          {omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS,
                           1},
                      }),
                  Group(1,
                        {
                            {omnibox::GROUP_PREVIOUS_SEARCH_RELATED, 1},
                        }),
              },
              group_configs,
              omnibox::GroupConfig_SideType_DEFAULT_PRIMARY) {}
  };

  auto test = [](ACMatches matches, std::vector<int> expected_relevances) {
    PSections sections;
    // A `Section` ensures all matches in `Group`s have the same `SideType` as
    // the `Section`.
    omnibox::GroupConfig group;
    group.set_side_type(omnibox::GroupConfig_SideType_SECONDARY);
    omnibox::GroupConfigMap group_configs{
        {omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS, group}};

    sections.push_back(std::make_unique<TestSection>(group_configs));
    auto out_matches = Section::GroupMatches(std::move(sections), matches);
    VerifyMatches(out_matches, expected_relevances);
  };

  // Given no matches, should return no matches.
  test({}, {});

  // Matches not qualifying for the section should not be added.
  test({CreateMatch(1, omnibox::GROUP_SEARCH)}, {});
}

// Tests rules for Section.
TEST(AutocompleteGrouperGroupsTest, Section) {
  class TestSection : public Section {
   public:
    // Up to 2 items of the following types.
    explicit TestSection(omnibox::GroupConfigMap& group_configs)
        : Section(2,
                  {
                      Group(1,
                            {
                                {omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX, 1},
                            }),
                      Group(1,
                            {
                                {omnibox::GROUP_MOBILE_CLIPBOARD, 1},
                            }),
                      Group(1,
                            {
                                {omnibox::GROUP_MOBILE_MOST_VISITED, 1},
                            }),
                      Group(1,
                            {
                                {omnibox::GROUP_VISITED_DOC_RELATED, 1},
                            }),
                      Group(1,
                            {
                                {omnibox::GROUP_RELATED_QUERIES, 1},
                            }),
                  },
                  group_configs,
                  omnibox::GroupConfig_SideType_DEFAULT_PRIMARY) {}
  };

  auto test = [](ACMatches matches, std::vector<int> expected_relevances) {
    PSections sections;
    omnibox::GroupConfigMap group_configs;
    sections.push_back(std::make_unique<TestSection>(group_configs));
    auto out_matches = Section::GroupMatches(std::move(sections), matches);
    VerifyMatches(out_matches, expected_relevances);
  };

  {
    SCOPED_TRACE(
        "Matches are ranked by the group order and added up to the limits.");
    test(
        {
            CreateMatch(6, omnibox::GROUP_RELATED_QUERIES),
            CreateMatch(5, omnibox::GROUP_VISITED_DOC_RELATED),
            CreateMatch(4, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(3, omnibox::GROUP_MOBILE_CLIPBOARD),
            CreateMatch(2, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX),
            CreateMatch(1, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX),
        },
        {5, 6});
  }
}

// Tests the groups, limits, and rules for the Desktop NTP ZPS section.
TEST(AutocompleteGrouperSectionsTest, DesktopNTPZpsSection) {
  auto test = [](ACMatches matches, std::vector<int> expected_relevances) {
    PSections sections;
    omnibox::GroupConfigMap group_configs;
    sections.push_back(
        std::make_unique<DesktopNTPZpsSection>(group_configs, 8u, false));
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
    SCOPED_TRACE("Personalized suggestions get precedence over trending ones");
    test(
        {
            // `GROUP_TRENDS` matches are more relevant but will not be added.
            CreateMatch(90, omnibox::GROUP_TRENDS),
            CreateMatch(89, omnibox::GROUP_TRENDS),
            CreateMatch(88, omnibox::GROUP_TRENDS),
            CreateMatch(87, omnibox::GROUP_TRENDS),
            CreateMatch(86, omnibox::GROUP_TRENDS),
            CreateMatch(85, omnibox::GROUP_TRENDS),
            CreateMatch(84, omnibox::GROUP_TRENDS),
            CreateMatch(83, omnibox::GROUP_TRENDS),
            CreateMatch(82, omnibox::GROUP_TRENDS),
            CreateMatch(81, omnibox::GROUP_TRENDS),
            // `GROUP_PERSONALIZED_ZERO_SUGGEST` matches should be added.
            CreateMatch(80, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(79, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(78, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(77, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(76, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(75, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(74, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(73, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(72, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(71, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
        },
        {
            80,
            79,
            78,
            77,
            76,
            75,
            74,
            73,
        });
  }
  {
    SCOPED_TRACE("Matches should be added up to their group limit.");
    test(
        {
            CreateMatch(80, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(79, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(78, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(77, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(76, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(75, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(74, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(73, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(72, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(71, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
        },
        {
            80,
            79,
            78,
            77,
            76,
            75,
            74,
            73,
        });
  }
  {
    SCOPED_TRACE("Matches should be added up to the section limit.");
    test(
        {
            // `GROUP_TRENDS` matches should be added up to the remaining
            // section limit (3).
            CreateMatch(90, omnibox::GROUP_TRENDS),
            CreateMatch(89, omnibox::GROUP_TRENDS),
            CreateMatch(88, omnibox::GROUP_TRENDS),
            CreateMatch(87, omnibox::GROUP_TRENDS),
            CreateMatch(86, omnibox::GROUP_TRENDS),
            CreateMatch(85, omnibox::GROUP_TRENDS),
            CreateMatch(84, omnibox::GROUP_TRENDS),
            CreateMatch(83, omnibox::GROUP_TRENDS),
            CreateMatch(82, omnibox::GROUP_TRENDS),
            CreateMatch(81, omnibox::GROUP_TRENDS),
            // `GROUP_PERSONALIZED_ZERO_SUGGEST` matches should all be added.
            CreateMatch(78, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(77, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(76, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(75, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(74, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
        },
        {
            78,
            77,
            76,
            75,
            74,
            90,
            89,
            88,
        });
  }
}

// Tests the groups, limits, and rules for the Desktop NTP ZPS section.
TEST(AutocompleteGrouperSectionsTest, DesktopNTPZpsSectionWithMIA) {
  auto test = [](std::vector<std::pair<int, omnibox::GroupId>> input,
                 bool mia_enabled,
                 std::vector<std::pair<int, omnibox::GroupId>> output) {
    ACMatches in_matches;
    for (const auto& [relevance, group_id] : input) {
      in_matches.push_back(CreateMatch(relevance, group_id));
    }
    PSections sections;
    omnibox::GroupConfigMap group_configs;
    sections.push_back(
        std::make_unique<DesktopNTPZpsSection>(group_configs, 8u, mia_enabled));
    auto out_matches = Section::GroupMatches(std::move(sections), in_matches);
    VerifyMatches(out_matches, output);
  };

  {
    SCOPED_TRACE(
        "MIA above pSuggest - local history zps takes precedence over Trends.");
    test(
        {
            // `GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA` and
            // `GROUP_MIA_RECOMMENDATIONS` matches should all be added and
            // appear first due to their higher relevance scores.
            {90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {89, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {88, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {87, omnibox::GROUP_MIA_RECOMMENDATIONS},
            // remote `GROUP_PERSONALIZED_ZERO_SUGGEST` should all be added.
            {86, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {85, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            // `GROUP_TRENDS` matches should not be added.
            {84, omnibox::GROUP_TRENDS},
            {83, omnibox::GROUP_TRENDS},
            {82, omnibox::GROUP_TRENDS},
            {81, omnibox::GROUP_TRENDS},
            // local `GROUP_PERSONALIZED_ZERO_SUGGEST` should be added up to the
            // remaining section limit (2) despite having lower relevance than
            // `GROUP_TRENDS`.
            {50, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {49, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {48, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {47, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
        },
        /*mia_enabled=*/true,
        {
            {90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {89, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {88, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {87, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {86, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {85, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {50, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {49, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
        });
  }
  {
    SCOPED_TRACE(
        "MIA below pSuggest - Local history zps is grouped with pSuggest but "
        "doesn't take precedence over non-Trends.");
    test(
        {
            // remote `GROUP_PERSONALIZED_ZERO_SUGGEST` should all be added.
            {90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {89, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            // `GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA` and
            // `GROUP_MIA_RECOMMENDATIONS` matches should should all be added
            // and appear last due to their lower relevance scores.
            {88, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {87, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {86, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {85, omnibox::GROUP_MIA_RECOMMENDATIONS},
            // `GROUP_TRENDS` matches should not be added.
            {84, omnibox::GROUP_TRENDS},
            {83, omnibox::GROUP_TRENDS},
            {82, omnibox::GROUP_TRENDS},
            {81, omnibox::GROUP_TRENDS},
            // local `GROUP_PERSONALIZED_ZERO_SUGGEST` should be added up to the
            // remaining section limit (2).
            {50, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {49, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {48, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {47, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
        },
        /*mia_enabled=*/true,
        {
            {90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {89, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {50, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {49, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {88, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {87, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {86, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {85, omnibox::GROUP_MIA_RECOMMENDATIONS},
        });
  }
  {
    SCOPED_TRACE(
        "MIA and no pSuggest - Local history zps doesn't take precedence over "
        "non-Trends.");
    test(
        {
            // `GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA` and
            // `GROUP_MIA_RECOMMENDATIONS` matches should all be added and
            // appear first due to their higher relevance scores.
            {90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {89, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {88, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {87, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {86, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {85, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {84, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {83, omnibox::GROUP_MIA_RECOMMENDATIONS},
            // `GROUP_TRENDS` matches should not be added.
            {82, omnibox::GROUP_TRENDS},
            {81, omnibox::GROUP_TRENDS},
            // local `GROUP_PERSONALIZED_ZERO_SUGGEST` should not be added.
            {50, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {49, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {48, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {47, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
        },
        /*mia_enabled=*/true,
        {
            {90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {89, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {88, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {87, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {86, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {85, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {84, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {83, omnibox::GROUP_MIA_RECOMMENDATIONS},
        });
  }
  {
    SCOPED_TRACE(
        "MIA and no pSuggest - Local history zps added but doesn't take "
        "precedence over non-Trends.");
    test(
        {
            // `GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA` and
            // `GROUP_MIA_RECOMMENDATIONS` matches should all be added and
            // appear first due to their higher relevance scores.
            {90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {89, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {88, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {87, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {86, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {85, omnibox::GROUP_MIA_RECOMMENDATIONS},
            // `GROUP_TRENDS` matches should not be added.
            {84, omnibox::GROUP_TRENDS},
            {83, omnibox::GROUP_TRENDS},
            {82, omnibox::GROUP_TRENDS},
            {81, omnibox::GROUP_TRENDS},
            // local `GROUP_PERSONALIZED_ZERO_SUGGEST` should not be added.
            {50, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {49, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {48, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {47, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
        },
        /*mia_enabled=*/true,
        {
            {90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {89, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {88, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {87, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {86, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {85, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {50, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {49, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
        });
  }
  {
    SCOPED_TRACE("MIA is not added if feature is disabled.");
    test(
        {
            // remote `GROUP_PERSONALIZED_ZERO_SUGGEST` should all be added.
            {90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {89, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            // `GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA` and
            // `GROUP_MIA_RECOMMENDATIONS` matches should not be added.
            {88, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {87, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {86, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {85, omnibox::GROUP_MIA_RECOMMENDATIONS},
            // `GROUP_TRENDS` matches should all be added.
            {84, omnibox::GROUP_TRENDS},
            {83, omnibox::GROUP_TRENDS},
            {82, omnibox::GROUP_TRENDS},
            {81, omnibox::GROUP_TRENDS},
            // local `GROUP_PERSONALIZED_ZERO_SUGGEST` should be added.
            {50, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
        },
        /*mia_enabled=*/false,
        {
            {90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {89, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {50, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {84, omnibox::GROUP_TRENDS},
            {83, omnibox::GROUP_TRENDS},
            {82, omnibox::GROUP_TRENDS},
            {81, omnibox::GROUP_TRENDS},
        });
  }
}

TEST(AutocompleteGrouperSectionsTest, AndroidNTPZpsSectionWithMIA) {
  auto test = [](std::vector<std::pair<int, omnibox::GroupId>> input,
                 bool mia_enabled, bool suppress_psuggest_backfill_with_mia,
                 std::vector<std::pair<int, omnibox::GroupId>> output) {
    ACMatches in_matches;
    for (const auto& [relevance, group_id] : input) {
      in_matches.push_back(CreateMatch(relevance, group_id));
    }
    PSections sections;
    omnibox::GroupConfigMap group_configs;

    // Cache the fieldtrial state from the singleton instance: we will be
    // overriding the state for the purpose of the test, and should revert the
    // default value right after.
    auto& mia_zps = const_cast<omnibox_feature_configs::MiaZPS&>(
        omnibox_feature_configs::MiaZPS::Get());
    bool default_suppress_psuggest_backfill_with_mia =
        mia_zps.suppress_psuggest_backfill_with_mia;
    mia_zps.suppress_psuggest_backfill_with_mia =
        suppress_psuggest_backfill_with_mia;

    sections.push_back(
        std::make_unique<AndroidNTPZpsSection>(group_configs, mia_enabled));
    auto out_matches = Section::GroupMatches(std::move(sections), in_matches);

    // Restore the backfill state as we can't reset the singleton.
    mia_zps.suppress_psuggest_backfill_with_mia =
        default_suppress_psuggest_backfill_with_mia;

    VerifyMatches(out_matches, output);
  };

  {
    SCOPED_TRACE(
        "MIA above pSuggest - local history zps takes precedence over Trends.");
    test(
        {
            {90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {89, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {88, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {87, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {86, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {85, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {84, omnibox::GROUP_TRENDS},
            {83, omnibox::GROUP_TRENDS},
            {82, omnibox::GROUP_TRENDS},
            {81, omnibox::GROUP_TRENDS},
            {50, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {49, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {48, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {47, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
        },
        /*mia_enabled=*/true,
        /*supppress_psuggest_backfill_with_mia=*/false,
        {
            // First: MIA
            {90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {89, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {88, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {87, omnibox::GROUP_MIA_RECOMMENDATIONS},
            // Next: backfill
            {86, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {85, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {50, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {49, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {48, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {47, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            // Lastly: Inspire Me
            {84, omnibox::GROUP_TRENDS},
            {83, omnibox::GROUP_TRENDS},
            {82, omnibox::GROUP_TRENDS},
            {81, omnibox::GROUP_TRENDS},
        });
  }

  {
    SCOPED_TRACE("MIA above pSuggest - local history suppressed.");
    test(
        {
            {90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {89, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {88, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {87, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {86, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {85, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {84, omnibox::GROUP_TRENDS},
            {83, omnibox::GROUP_TRENDS},
            {82, omnibox::GROUP_TRENDS},
            {81, omnibox::GROUP_TRENDS},
            {50, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {49, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {48, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {47, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
        },
        /*mia_enabled=*/true,
        /*supppress_psuggest_backfill_with_mia=*/true,
        {
            // First: MIA
            {90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {89, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {88, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {87, omnibox::GROUP_MIA_RECOMMENDATIONS},
            // No backfill this time
            // Lastly - Inspire Me
            {84, omnibox::GROUP_TRENDS},
            {83, omnibox::GROUP_TRENDS},
            {82, omnibox::GROUP_TRENDS},
            {81, omnibox::GROUP_TRENDS},
        });
  }

  {
    SCOPED_TRACE("PSuggest backfill present with no MIA");
    test(
        {
            {86, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {85, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {84, omnibox::GROUP_TRENDS},
            {83, omnibox::GROUP_TRENDS},
            {82, omnibox::GROUP_TRENDS},
            {81, omnibox::GROUP_TRENDS},
            {50, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {49, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {48, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {47, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
        },
        /*mia_enabled=*/true,
        /*supppress_psuggest_backfill_with_mia=*/true,
        {
            // Show backfill
            {86, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {85, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {50, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {49, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {48, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {47, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            // and then Inspire Me
            {84, omnibox::GROUP_TRENDS},
            {83, omnibox::GROUP_TRENDS},
            {82, omnibox::GROUP_TRENDS},
            {81, omnibox::GROUP_TRENDS},
        });
  }
}

TEST(AutocompleteGrouperSectionsTest, IosNTPZpsSectionWithMIA) {
  auto test = [](std::vector<std::pair<int, omnibox::GroupId>> input,
                 bool mia_enabled, bool suppress_psuggest_backfill_with_mia,
                 std::vector<std::pair<int, omnibox::GroupId>> output) {
    ACMatches in_matches;
    for (const auto& [relevance, group_id] : input) {
      in_matches.push_back(CreateMatch(relevance, group_id));
    }
    PSections sections;
    omnibox::GroupConfigMap group_configs;

    // Cache the fieldtrial state from the singleton instance: we will be
    // overriding the state for the purpose of the test, and should revert the
    // default value right after.
    auto& mia_zps = const_cast<omnibox_feature_configs::MiaZPS&>(
        omnibox_feature_configs::MiaZPS::Get());
    bool default_suppress_psuggest_backfill_with_mia =
        mia_zps.suppress_psuggest_backfill_with_mia;
    mia_zps.suppress_psuggest_backfill_with_mia =
        suppress_psuggest_backfill_with_mia;

    sections.push_back(
        std::make_unique<IOSNTPZpsSection>(group_configs, mia_enabled));
    auto out_matches = Section::GroupMatches(std::move(sections), in_matches);

    // Restore the backfill state as we can't reset the singleton.
    mia_zps.suppress_psuggest_backfill_with_mia =
        default_suppress_psuggest_backfill_with_mia;

    VerifyMatches(out_matches, output);
  };

  {
    SCOPED_TRACE(
        "MIA above pSuggest - local history zps takes precedence over Trends.");
    test(
        {
            {90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {89, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {88, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {87, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {86, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {85, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {84, omnibox::GROUP_TRENDS},
            {83, omnibox::GROUP_TRENDS},
            {82, omnibox::GROUP_TRENDS},
            {81, omnibox::GROUP_TRENDS},
            {50, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {49, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {48, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {47, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
        },
        /*mia_enabled=*/true,
        /*suppress_psuggest_backfill_with_mia=*/false,
        {
            // First: MIA
            {90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {89, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {88, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {87, omnibox::GROUP_MIA_RECOMMENDATIONS},
            // Next: backfill
            {86, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {85, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {50, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {49, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {48, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {47, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            // Lastly: Inspire Me
            {84, omnibox::GROUP_TRENDS},
            {83, omnibox::GROUP_TRENDS},
            {82, omnibox::GROUP_TRENDS},
            {81, omnibox::GROUP_TRENDS},
        });
  }

  {
    SCOPED_TRACE("MIA above pSuggest - local history suppressed.");
    test(
        {
            {90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {89, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {88, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {87, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {86, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {85, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {84, omnibox::GROUP_TRENDS},
            {83, omnibox::GROUP_TRENDS},
            {82, omnibox::GROUP_TRENDS},
            {81, omnibox::GROUP_TRENDS},
            {50, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {49, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {48, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {47, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
        },
        /*mia_enabled=*/true,
        /*suppress_psuggest_backfill_with_mia=*/true,
        {
            // First: MIA
            {90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {89, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {88, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {87, omnibox::GROUP_MIA_RECOMMENDATIONS},
            // No backfill this time
            // Lastly - Inspire Me
            {84, omnibox::GROUP_TRENDS},
            {83, omnibox::GROUP_TRENDS},
            {82, omnibox::GROUP_TRENDS},
            {81, omnibox::GROUP_TRENDS},
        });
  }

  {
    SCOPED_TRACE("PSuggest backfill present with no MIA");
    test(
        {
            {86, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {85, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {84, omnibox::GROUP_TRENDS},
            {83, omnibox::GROUP_TRENDS},
            {82, omnibox::GROUP_TRENDS},
            {81, omnibox::GROUP_TRENDS},
            {50, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {49, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {48, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {47, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
        },
        /*mia_enabled=*/true,
        /*suppress_psuggest_backfill_with_mia=*/true,
        {
            // Show backfill
            {86, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {85, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {50, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {49, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {48, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {47, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            // and then Inspire Me
            {84, omnibox::GROUP_TRENDS},
            {83, omnibox::GROUP_TRENDS},
            {82, omnibox::GROUP_TRENDS},
            {81, omnibox::GROUP_TRENDS},
        });
  }
}

TEST(AutocompleteGrouperSectionsTest, IosIPadNTPZpsSectionWithMIA) {
  auto test = [](std::vector<std::pair<int, omnibox::GroupId>> input,
                 bool mia_enabled, bool suppress_psuggest_backfill_with_mia,
                 std::vector<std::pair<int, omnibox::GroupId>> output) {
    ACMatches in_matches;
    for (const auto& [relevance, group_id] : input) {
      in_matches.push_back(CreateMatch(relevance, group_id));
    }
    PSections sections;
    omnibox::GroupConfigMap group_configs;

    // Cache the fieldtrial state from the singleton instance: we will be
    // overriding the state for the purpose of the test, and should revert the
    // default value right after.
    auto& mia_zps = const_cast<omnibox_feature_configs::MiaZPS&>(
        omnibox_feature_configs::MiaZPS::Get());
    bool default_suppress_psuggest_backfill_with_mia =
        mia_zps.suppress_psuggest_backfill_with_mia;
    mia_zps.suppress_psuggest_backfill_with_mia =
        suppress_psuggest_backfill_with_mia;

    sections.push_back(std::make_unique<IOSIpadNTPZpsSection>(
        /*trends_count=*/5,
        /*total_count=*/20, group_configs, mia_enabled));
    auto out_matches = Section::GroupMatches(std::move(sections), in_matches);

    // Restore the backfill state as we can't reset the singleton.
    mia_zps.suppress_psuggest_backfill_with_mia =
        default_suppress_psuggest_backfill_with_mia;

    VerifyMatches(out_matches, output);
  };

  {
    SCOPED_TRACE(
        "MIA above pSuggest - local history zps takes precedence over Trends.");
    test(
        {
            {90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {89, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {88, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {87, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {86, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {85, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {84, omnibox::GROUP_TRENDS},
            {83, omnibox::GROUP_TRENDS},
            {82, omnibox::GROUP_TRENDS},
            {81, omnibox::GROUP_TRENDS},
            {50, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {49, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {48, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {47, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
        },
        /*mia_enabled=*/true,
        /*suppress_psuggest_backfill_with_mia=*/false,
        {
            // First: MIA
            {90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {89, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {88, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {87, omnibox::GROUP_MIA_RECOMMENDATIONS},
            // Next: backfill
            {86, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {85, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {50, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {49, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {48, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {47, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            // Lastly: Inspire Me
            {84, omnibox::GROUP_TRENDS},
            {83, omnibox::GROUP_TRENDS},
            {82, omnibox::GROUP_TRENDS},
            {81, omnibox::GROUP_TRENDS},
        });
  }

  {
    SCOPED_TRACE("MIA above pSuggest - local history suppressed.");
    test(
        {
            {90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {89, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {88, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {87, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {86, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {85, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {84, omnibox::GROUP_TRENDS},
            {83, omnibox::GROUP_TRENDS},
            {82, omnibox::GROUP_TRENDS},
            {81, omnibox::GROUP_TRENDS},
            {50, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {49, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {48, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {47, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
        },
        /*mia_enabled=*/true,
        /*suppress_psuggest_backfill_with_mia=*/true,
        {
            // First: MIA
            {90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {89, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA},
            {88, omnibox::GROUP_MIA_RECOMMENDATIONS},
            {87, omnibox::GROUP_MIA_RECOMMENDATIONS},
            // No backfill this time
            // Lastly - Inspire Me
            {84, omnibox::GROUP_TRENDS},
            {83, omnibox::GROUP_TRENDS},
            {82, omnibox::GROUP_TRENDS},
            {81, omnibox::GROUP_TRENDS},
        });
  }

  {
    SCOPED_TRACE("PSuggest backfill present with no MIA");
    test(
        {
            {86, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {85, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {84, omnibox::GROUP_TRENDS},
            {83, omnibox::GROUP_TRENDS},
            {82, omnibox::GROUP_TRENDS},
            {81, omnibox::GROUP_TRENDS},
            {50, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {49, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {48, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {47, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
        },
        /*mia_enabled=*/true,
        /*suppress_psuggest_backfill_with_mia=*/true,
        {
            // Show backfill
            {86, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {85, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {50, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {49, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {48, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            {47, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
            // and then Inspire Me
            {84, omnibox::GROUP_TRENDS},
            {83, omnibox::GROUP_TRENDS},
            {82, omnibox::GROUP_TRENDS},
            {81, omnibox::GROUP_TRENDS},
        });
  }
}

// Tests the groups, limits, and rules for the Desktop NTP ZPS section.
TEST(AutocompleteGrouperSectionsTest, DesktopNTPZpsSection_WithIPH) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(omnibox::kStarterPackIPH);

  auto test = [](ACMatches matches, std::vector<int> expected_relevances) {
    PSections sections;
    omnibox::GroupConfigMap group_configs;
    sections.push_back(
        std::make_unique<DesktopNTPZpsSection>(group_configs, 7u, false));
    sections.push_back(
        std::make_unique<DesktopNTPZpsIPHSection>(group_configs));
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
    SCOPED_TRACE("Personalized suggestions get precedence over trending ones");
    test(
        {
            // `GROUP_TRENDS` matches are more relevant but will not be added.
            CreateMatch(90, omnibox::GROUP_TRENDS),
            CreateMatch(89, omnibox::GROUP_TRENDS),
            CreateMatch(88, omnibox::GROUP_TRENDS),
            CreateMatch(87, omnibox::GROUP_TRENDS),
            CreateMatch(86, omnibox::GROUP_TRENDS),
            CreateMatch(85, omnibox::GROUP_TRENDS),
            CreateMatch(84, omnibox::GROUP_TRENDS),
            CreateMatch(83, omnibox::GROUP_TRENDS),
            CreateMatch(82, omnibox::GROUP_TRENDS),
            CreateMatch(81, omnibox::GROUP_TRENDS),
            // `GROUP_PERSONALIZED_ZERO_SUGGEST` matches should be added.
            CreateMatch(80, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(79, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(78, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(77, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(76, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(75, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(74, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(73, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(72, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(71, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            // `GROUP_PREVIOUS_SEARCH_RELATED` matches should not be added.
            CreateMatch(70, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(69, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(68, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(67, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(66, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(65, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(64, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(63, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(62, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(61, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            // `GROUP_ZERO_SUGGEST_IN_PRODUCT_HELP` suggestions have their
            // own section, so one should be added.
            CreateMatch(60, omnibox::GROUP_ZERO_SUGGEST_IN_PRODUCT_HELP),
            CreateMatch(59, omnibox::GROUP_ZERO_SUGGEST_IN_PRODUCT_HELP),
        },
        {
            80,
            79,
            78,
            77,
            76,
            75,
            74,
            60,
        });
  }
  {
    SCOPED_TRACE("Matches should be added up to their group limit.");
    test(
        {
            CreateMatch(80, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(79, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(78, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(77, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(76, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(75, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(74, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(73, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(72, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(71, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
        },
        {
            80,
            79,
            78,
            77,
            76,
            75,
            74,
        });
  }
  {
    SCOPED_TRACE("Matches should be added up to the section limit.");
    test(
        {
            // `GROUP_TRENDS` matches should be added up to the remaining
            // section limit (2).
            CreateMatch(90, omnibox::GROUP_TRENDS),
            CreateMatch(89, omnibox::GROUP_TRENDS),
            CreateMatch(88, omnibox::GROUP_TRENDS),
            CreateMatch(87, omnibox::GROUP_TRENDS),
            CreateMatch(86, omnibox::GROUP_TRENDS),
            CreateMatch(85, omnibox::GROUP_TRENDS),
            CreateMatch(84, omnibox::GROUP_TRENDS),
            CreateMatch(83, omnibox::GROUP_TRENDS),
            CreateMatch(82, omnibox::GROUP_TRENDS),
            CreateMatch(81, omnibox::GROUP_TRENDS),
            // `GROUP_PERSONALIZED_ZERO_SUGGEST` matches should all be added.
            CreateMatch(80, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(79, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(78, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(77, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(76, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            // `GROUP_ZERO_SUGGEST_IN_PRODUCT_HELP` should be added up to its
            // section limit (1).
            CreateMatch(75, omnibox::GROUP_ZERO_SUGGEST_IN_PRODUCT_HELP),
            CreateMatch(74, omnibox::GROUP_ZERO_SUGGEST_IN_PRODUCT_HELP),
        },
        {
            80,
            79,
            78,
            77,
            76,
            90,
            89,
            75,
        });
  }
  {
    SCOPED_TRACE(
        "IPH match should be added regardless of matches in first section.");
    test(
        {
            // `GROUP_PERSONALIZED_ZERO_SUGGEST` matches should all be added.
            CreateMatch(80, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(77, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(76, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            // `GROUP_ZERO_SUGGEST_IN_PRODUCT_HELP` should be added up to its
            // section limit even if the first section is not full. (1).
            CreateMatch(75, omnibox::GROUP_ZERO_SUGGEST_IN_PRODUCT_HELP),
            CreateMatch(74, omnibox::GROUP_ZERO_SUGGEST_IN_PRODUCT_HELP),
        },
        {
            80,
            77,
            76,
            75,
        });
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
    test(
        {
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
    SCOPED_TRACE("Android/ZPS with Clipboard entries.");
    // Verify that up to one Clipboard suggestion is retained on top.
    test(
        {
            CreateMatch(200, omnibox::GROUP_MOBILE_CLIPBOARD),
            CreateMatch(199, omnibox::GROUP_MOBILE_CLIPBOARD),
            CreateMatch(198, omnibox::GROUP_MOBILE_CLIPBOARD),
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
        {200, 100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88, 87});
  }
  {
    SCOPED_TRACE("Android/ZPS with Search Ready Omnibox.");
    // Verify that up to one SRO suggestion is retained on top.
    test(
        {
            CreateMatch(200, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX),
            CreateMatch(199, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX),
            CreateMatch(198, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX),
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
        {200, 100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88, 87});
  }
  {
    SCOPED_TRACE("Android/ZPS on SRP with recent searches only.");
    // Verify that recent searches are shown up to the section limit.
    test(
        {
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
    SCOPED_TRACE("Android/ZPS with MV Tiles.");
    // Verify that the MV suggestions are not allowed.
    test(
        {
            CreateMatch(300, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(299, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(298, omnibox::GROUP_MOBILE_MOST_VISITED),
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
    SCOPED_TRACE("Android/ZPS with multiple auxiliary suggestions.");
    test(
        {
            // Up to one SRO should be shown first.
            CreateMatch(300, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX),
            CreateMatch(299, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX),
            // Up to one Clipboard should be shown after SRO.
            CreateMatch(298, omnibox::GROUP_MOBILE_CLIPBOARD),
            CreateMatch(297, omnibox::GROUP_MOBILE_CLIPBOARD),
            // MV Tiles are not allowed.
            CreateMatch(296, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(295, omnibox::GROUP_MOBILE_MOST_VISITED),
            // Previous Search Related and recent searches should be shown up to
            // the remaining section limit.
            CreateMatch(100, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(99, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(98, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(97, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(96, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(95, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(94, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
            CreateMatch(93, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
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
        {300, 298, 100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88});
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
    test(
        {
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
    SCOPED_TRACE("Android/ZPS with Clipboard entries.");
    // Verify that up to one Clipboard suggestion is retained on top.
    test(
        {
            CreateMatch(200, omnibox::GROUP_MOBILE_CLIPBOARD),
            CreateMatch(199, omnibox::GROUP_MOBILE_CLIPBOARD),
            CreateMatch(198, omnibox::GROUP_MOBILE_CLIPBOARD),
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
        {200, 100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88, 87});
  }
  {
    SCOPED_TRACE("Android/ZPS with Search Ready Omnibox.");
    // Verify that up to one SRO suggestion is retained on top.
    test(
        {
            CreateMatch(200, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX),
            CreateMatch(199, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX),
            CreateMatch(198, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX),
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
        {200, 100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88, 87});
  }
  {
    SCOPED_TRACE("Android/ZPS with MV Tiles.");
    // Verify that the MV suggestions are retained on top.
    test(
        {
            // Slotted in horizontal render group, taking up 1 row.
            CreateMatch(300, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(299, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(298, omnibox::GROUP_MOBILE_MOST_VISITED),
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
        {300, 299, 298, 100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88,
         87});
  }
  {
    SCOPED_TRACE("Android/ZPS with multiple auxiliary suggestions.");
    test(
        {
            // Up to one SRO should be shown first.
            CreateMatch(300, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX),
            CreateMatch(299, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX),
            // Up to one Clipboard should be shown after SRO.
            CreateMatch(298, omnibox::GROUP_MOBILE_CLIPBOARD),
            CreateMatch(297, omnibox::GROUP_MOBILE_CLIPBOARD),
            CreateMatch(297, omnibox::GROUP_MOBILE_CLIPBOARD),
            // MV Tiles are slotted in horizontal render group, taking up 1
            // row.
            CreateMatch(296, omnibox::GROUP_MOBILE_MOST_VISITED),
            CreateMatch(295, omnibox::GROUP_MOBILE_MOST_VISITED),
            // Visited Doc Related and recent searches should be shown up to
            // the remaining section limit.
            CreateMatch(100, omnibox::GROUP_VISITED_DOC_RELATED),
            CreateMatch(99, omnibox::GROUP_VISITED_DOC_RELATED),
            CreateMatch(98, omnibox::GROUP_VISITED_DOC_RELATED),
            CreateMatch(97, omnibox::GROUP_VISITED_DOC_RELATED),
            CreateMatch(96, omnibox::GROUP_VISITED_DOC_RELATED),
            CreateMatch(95, omnibox::GROUP_VISITED_DOC_RELATED),
            CreateMatch(94, omnibox::GROUP_VISITED_DOC_RELATED),
            CreateMatch(93, omnibox::GROUP_VISITED_DOC_RELATED),
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
        {300, 298, 296, 295, 100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89});
  }
}

// Tests the groups, limits, and rules for the Android NTP ZPS + Inspire Me.
TEST(AutocompleteGrouperSectionsTest, AndroidNTPZpsSection_withInspireMe) {
  auto test = [](ACMatches matches, std::vector<int> expected_relevances) {

    PSections sections;
    omnibox::GroupConfigMap group_configs;
    sections.push_back(
        std::make_unique<AndroidNTPZpsSection>(group_configs, false));
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

// Tests the groups, limits, and rules for the Android Web ZPS + MV tiles.
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

// Tests the groups, limits, and rules for the iOS NTP ZPS.
TEST(AutocompleteGrouperSectionsTest, IOSNTPZpsSection) {
  auto test = [](ACMatches matches, std::vector<int> expected_relevances) {
    PSections sections;
    omnibox::GroupConfigMap group_configs;
    sections.push_back(
        std::make_unique<IOSNTPZpsSection>(group_configs, false));
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
         {100, 99, 98, 97});
  }

  {
    SCOPED_TRACE(
        "Given no psuggest matches and only trends, should only display trends "
        "following the trends count limit");
    test({CreateMatch(100, omnibox::GROUP_TRENDS),
          CreateMatch(99, omnibox::GROUP_TRENDS),
          CreateMatch(98, omnibox::GROUP_TRENDS),
          CreateMatch(97, omnibox::GROUP_TRENDS),
          CreateMatch(96, omnibox::GROUP_TRENDS),
          CreateMatch(95, omnibox::GROUP_TRENDS)},
         {100, 99, 98, 97, 96});
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
         {100, 99, 98, 97, 96, 95, 94, 93});
  }
}

// Tests the groups and limits for DesktopSecondaryNTPZpsSection.
TEST(AutocompleteGrouperSectionsTest, DesktopSecondaryNTPZpsSection) {
  // Explicitly enable RealboxContextualAndTrendingSuggestions feature and set
  // params.
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::RealboxContextualAndTrendingSuggestions>
      scoped_config;
  scoped_config.Get().enabled = true;
  scoped_config.Get().total_limit = 4;
  scoped_config.Get().contextual_suggestions_limit = 4;
  scoped_config.Get().trending_suggestions_limit = 4;
  auto test = [](ACMatches matches, std::vector<int> expected_relevances) {
    PSections sections;
    omnibox::GroupConfigMap group_configs;
    const auto group1 = omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS;
    const auto group2 = omnibox::GROUP_PREVIOUS_SEARCH_RELATED;
    const auto group3 = omnibox::GROUP_TRENDS;
    group_configs[group1].set_side_type(
        omnibox::GroupConfig_SideType_SECONDARY);
    group_configs[group2].set_side_type(
        omnibox::GroupConfig_SideType_SECONDARY);
    group_configs[group3].set_side_type(
        omnibox::GroupConfig_SideType_SECONDARY);
    sections.push_back(
        std::make_unique<DesktopSecondaryNTPZpsSection>(group_configs));
    auto out_matches = Section::GroupMatches(std::move(sections), matches);
    VerifyMatches(out_matches, expected_relevances);
  };
  {
    SCOPED_TRACE("Given no matches, should return no matches.");
    test({}, {});
  }
  {
    SCOPED_TRACE("Matches should be added up to their group limit.");
    // Groups do not enforce a minimum number of matches shown. They do enforce
    // a maximum number of matches shown for that Group.
    test({CreateMatch(100, omnibox::GROUP_TRENDS),
          CreateMatch(99, omnibox::GROUP_TRENDS),
          CreateMatch(98, omnibox::GROUP_TRENDS),
          CreateMatch(97, omnibox::GROUP_TRENDS),
          CreateMatch(96, omnibox::GROUP_TRENDS)},
         {100, 99, 98, 97});
  }
  {
    SCOPED_TRACE("Matches should be added up to the section limit.");
    // Sections do not enforce a minimum number of matches shown. They do
    // enforce a maximum number of matches shown.
    test({CreateMatch(100, omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS),
          CreateMatch(99, omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS),
          // `GROUP_PREVIOUS_SEARCH_RELATED` matches will be displayed
          // simultaneously as `GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS`
          // matches. Showing both may occur but is not likely to happen.
          CreateMatch(200, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
          CreateMatch(201, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
          CreateMatch(202, omnibox::GROUP_PREVIOUS_SEARCH_RELATED)},
         {100, 99, 200, 201});
  }
  {
    SCOPED_TRACE(
        "Matches added up to their group limit, but not section limit.");
    test({CreateMatch(100, omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS),
          CreateMatch(99, omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS),
          CreateMatch(98, omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS),
          CreateMatch(97, omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS)},
         {100, 99, 98});
  }
  {
    SCOPED_TRACE(
        "Given no matches that can be added to this section because of their "
        "GroupId, should return no matches.");
    test({CreateMatch(100, omnibox::GROUP_TRENDS_ENTITY_CHIPS),
          CreateMatch(99, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
          CreateMatch(98, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST)},
         {});
  }
  // Test groups and limits when RealboxContextualAndTrendingSuggestions feature
  // is disabled.
  scoped_config.Reset();
  scoped_config.Get().enabled = false;
  {
    SCOPED_TRACE(
        "Matches should be added up to their group limit. "
        "(RealboxContextualAndTrendingSuggestions feature disabled)");
    test({CreateMatch(100, omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS),
          CreateMatch(99, omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS),
          CreateMatch(98, omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS),
          CreateMatch(97, omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS)},
         {100, 99, 98});
  }
  {
    SCOPED_TRACE(
        "Given no matches that can be added to this section because of their "
        "Group limit, should return no matches. "
        "(RealboxContextualAndTrendingSuggestions feature disabled)");
    test({CreateMatch(100, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
          CreateMatch(99, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
          CreateMatch(98, omnibox::GROUP_TRENDS)},
         {});
  }
}

// Tests the behavior when DesktopNTPZpsSection and
// DesktopSecondaryNTPZpsSection are both created.
TEST(AutocompleteGrouperSectionsTest,
     DesktopNTPZpsSectionAndDesktopSecondaryNTPZpsSection) {
  // Explicitly enable RealboxContextualAndTrendingSuggestions feature and set
  // params.
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::RealboxContextualAndTrendingSuggestions>
      scoped_config;
  scoped_config.Get().enabled = true;
  scoped_config.Get().total_limit = 4;
  scoped_config.Get().contextual_suggestions_limit = 4;
  scoped_config.Get().trending_suggestions_limit = 4;
  auto test = [](ACMatches matches, std::vector<int> expected_relevances,
                 bool trends_has_default_side_type = true) {
    PSections sections;
    const auto group1 = omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST;
    const auto group2 = omnibox::GROUP_TRENDS;
    const auto group3 = omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS;
    const auto group4 = omnibox::GROUP_PREVIOUS_SEARCH_RELATED;
    omnibox::GroupConfigMap group_configs;
    group_configs[group1];
    group_configs[group2];
    if (!trends_has_default_side_type) {
      group_configs[group2].set_side_type(
          omnibox::GroupConfig_SideType_SECONDARY);
    }
    group_configs[group3].set_side_type(
        omnibox::GroupConfig_SideType_SECONDARY);
    group_configs[group4].set_side_type(
        omnibox::GroupConfig_SideType_SECONDARY);
    sections.push_back(
        std::make_unique<DesktopNTPZpsSection>(group_configs, 8u, false));
    sections.push_back(
        std::make_unique<DesktopSecondaryNTPZpsSection>(group_configs));
    auto out_matches = Section::GroupMatches(std::move(sections), matches);
    VerifyMatches(out_matches, expected_relevances);
  };
  {
    SCOPED_TRACE(
        "Given 8 psuggest matches, and trending matches with a secondary side "
        "type, display psuggest matches on LHS and trending on the RHS.");
    test(
        {
            CreateMatch(100, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(99, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(98, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(97, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(96, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(95, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(94, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(93, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(91, omnibox::GROUP_TRENDS),
            CreateMatch(90, omnibox::GROUP_TRENDS),
            CreateMatch(89, omnibox::GROUP_TRENDS),
            CreateMatch(88, omnibox::GROUP_TRENDS),
        },
        {100, 99, 98, 97, 96, 95, 94, 93, 91, 90, 89, 88}, false);
  }
  {
    SCOPED_TRACE(
        "Given psuggests and trending suggestions with a default side type, "
        "display psuggest and trending on the LHS and entity chip suggestions "
        "on the RHS.");
    test(
        {
            CreateMatch(200, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(199, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(198, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(197, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(100, omnibox::GROUP_TRENDS),
            CreateMatch(99, omnibox::GROUP_TRENDS),
            CreateMatch(98, omnibox::GROUP_TRENDS),
            CreateMatch(97, omnibox::GROUP_TRENDS),
            CreateMatch(96, omnibox::GROUP_TRENDS),
            CreateMatch(92,
                        omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS),
            CreateMatch(91,
                        omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS),
            CreateMatch(90,
                        omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS),
        },
        {200, 199, 198, 197, 100, 99, 98, 97, 92, 91, 90});
  }
  // Test groups and limits when RealboxContextualAndTrendingSuggestions feature
  // is disabled.
  scoped_config.Reset();
  scoped_config.Get().enabled = false;
  {
    SCOPED_TRACE(
        "Given 8 psuggest matches, and trending matches with a secondary side "
        "type, but RealboxContextualAndTrendingSuggestions"
        "feature disabled, do not show trending on the RHS.");
    test(
        {
            CreateMatch(100, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(99, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(98, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(97, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(96, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(95, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(94, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(93, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(92, omnibox::GROUP_TRENDS),
            CreateMatch(91, omnibox::GROUP_TRENDS),
            CreateMatch(90, omnibox::GROUP_TRENDS),
            CreateMatch(89, omnibox::GROUP_TRENDS),
        },
        {100, 99, 98, 97, 96, 95, 94, 93}, false);
  }
}

#if !(BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS))
// Tests the groups, limits, and rules for the Desktop SRP section with URL
// suggestions enabled.
TEST(AutocompleteGrouperSectionsTest, DesktopSRPZpsSectionWithUrls) {
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::OmniboxZpsSuggestionLimit>
      scoped_config;
  scoped_config.Get().enabled = true;
  scoped_config.Get().max_search_suggestions = 4;
  scoped_config.Get().max_url_suggestions = 4;
  auto test = [](size_t action_count, ACMatches matches,
                 std::vector<int> expected_relevances) {
    PSections sections;
    omnibox::GroupConfigMap group_configs;
    group_configs[omnibox::GROUP_MOST_VISITED];
    group_configs[omnibox::GROUP_PREVIOUS_SEARCH_RELATED];
    // Max 8 suggestions, with an upper limit of 4 search suggestions.
    sections.push_back(std::make_unique<DesktopSRPZpsSection>(
        group_configs, 8u + action_count, 4u, 4u, action_count));
    auto out_matches = Section::GroupMatches(std::move(sections), matches);
    VerifyMatches(out_matches, expected_relevances);
  };
  {
    SCOPED_TRACE(
        "Given 12 srp zps matches, the group should respect the search "
        "suggestion limit");
    test(0,
         {
             CreateMatch(100, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
             CreateMatch(99, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
             CreateMatch(98, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
             CreateMatch(97, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
             CreateMatch(96, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
             CreateMatch(95, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
             CreateMatch(94, omnibox::GROUP_MOST_VISITED),
             CreateMatch(93, omnibox::GROUP_MOST_VISITED),
             CreateMatch(92, omnibox::GROUP_MOST_VISITED),
             CreateMatch(91, omnibox::GROUP_MOST_VISITED),
             CreateMatch(89, omnibox::GROUP_MOST_VISITED),
             CreateMatch(88, omnibox::GROUP_MOST_VISITED),
         },
         {100, 99, 98, 97, 94, 93, 92, 91});
  }
  {
    SCOPED_TRACE(
        "Given 12 srp zps matches, the group should respect the url suggestion "
        "limit");
    test(0,
         {
             CreateMatch(100, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
             CreateMatch(99, omnibox::GROUP_MOST_VISITED),
             CreateMatch(98, omnibox::GROUP_MOST_VISITED),
             CreateMatch(97, omnibox::GROUP_MOST_VISITED),
             CreateMatch(96, omnibox::GROUP_MOST_VISITED),
             CreateMatch(95, omnibox::GROUP_MOST_VISITED),
             CreateMatch(94, omnibox::GROUP_MOST_VISITED),
             CreateMatch(93, omnibox::GROUP_MOST_VISITED),
             CreateMatch(92, omnibox::GROUP_MOST_VISITED),
             CreateMatch(91, omnibox::GROUP_MOST_VISITED),
             CreateMatch(90, omnibox::GROUP_MOST_VISITED),
             CreateMatch(89, omnibox::GROUP_MOST_VISITED),
         },
         {100, 99, 98, 97, 96});
  }
  {
    SCOPED_TRACE("Contextual search action is excluded when disabled.");
    test(0,
         {
             CreateMatch(100, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
             CreateMatch(99, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
             CreateMatch(98, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
             CreateMatch(97, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
             CreateMatch(96, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
             CreateMatch(95, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
             CreateMatch(94, omnibox::GROUP_MOST_VISITED),
             CreateMatch(93, omnibox::GROUP_MOST_VISITED),
             CreateMatch(92, omnibox::GROUP_MOST_VISITED),
             CreateMatch(91, omnibox::GROUP_MOST_VISITED),
             CreateMatch(89, omnibox::GROUP_MOST_VISITED),
             CreateMatch(88, omnibox::GROUP_MOST_VISITED),
             CreateMatch(87, omnibox::GROUP_CONTEXTUAL_SEARCH_ACTION),
         },
         {100, 99, 98, 97, 94, 93, 92, 91});
  }
  {
    SCOPED_TRACE("Contextual search action is included when enabled.");
    test(1,
         {
             CreateMatch(100, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
             CreateMatch(99, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
             CreateMatch(98, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
             CreateMatch(97, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
             CreateMatch(96, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
             CreateMatch(95, omnibox::GROUP_PREVIOUS_SEARCH_RELATED),
             CreateMatch(94, omnibox::GROUP_MOST_VISITED),
             CreateMatch(93, omnibox::GROUP_MOST_VISITED),
             CreateMatch(92, omnibox::GROUP_MOST_VISITED),
             CreateMatch(91, omnibox::GROUP_MOST_VISITED),
             CreateMatch(89, omnibox::GROUP_MOST_VISITED),
             CreateMatch(88, omnibox::GROUP_MOST_VISITED),
             CreateMatch(87, omnibox::GROUP_CONTEXTUAL_SEARCH_ACTION),
         },
         {100, 99, 98, 97, 94, 93, 92, 91, 87});
  }
}

// Tests the groups, limits, and rules for the Desktop Web section with URL
// suggestions enabled.
TEST(AutocompleteGrouperSectionsTest, DesktopWebZpsSectionWithUrls) {
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::OmniboxUrlSuggestionsOnFocus>
      scoped_config;
  scoped_config.Get().enabled = true;
  auto test = [](ACMatches matches, std::vector<int> expected_relevances,
                 bool trends_has_default_side_type = true) {
    PSections sections;
    omnibox::GroupConfigMap group_configs;
    group_configs[omnibox::GROUP_MOST_VISITED];
    group_configs[omnibox::GROUP_PREVIOUS_SEARCH_RELATED];
    group_configs[omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST];
    // Max 4 url suggestions.
    sections.push_back(
        std::make_unique<DesktopWebURLZpsSection>(group_configs, 4u));
    // Max 4 suggestions, with an upper limit of 4 contextual search
    // suggestions and no contextual actions.
    sections.push_back(std::make_unique<DesktopWebSearchZpsSection>(
        group_configs, /*limit=*/4u, /*contextual_action_limit=*/0u,
        /*contextual_search_limit=*/4u));
    auto out_matches = Section::GroupMatches(std::move(sections), matches);
    VerifyMatches(out_matches, expected_relevances);
  };
  {
    SCOPED_TRACE(
        "Given 12 web zps matches, the group should respect the url "
        "limit as well as show them first in the suggestion list.");
    test(
        {
            CreateMatch(100, omnibox::GROUP_MOST_VISITED),
            CreateMatch(99, omnibox::GROUP_MOST_VISITED),
            CreateMatch(98, omnibox::GROUP_MOST_VISITED),
            CreateMatch(97, omnibox::GROUP_MOST_VISITED),
            CreateMatch(96, omnibox::GROUP_MOST_VISITED),
            CreateMatch(95, omnibox::GROUP_MOST_VISITED),
            CreateMatch(94, omnibox::GROUP_VISITED_DOC_RELATED),
            CreateMatch(93, omnibox::GROUP_VISITED_DOC_RELATED),
            CreateMatch(92, omnibox::GROUP_VISITED_DOC_RELATED),
            CreateMatch(91, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(89, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
        },
        {100, 99, 98, 97, 94, 93, 92, 91});
  }
  {
    SCOPED_TRACE(
        "Given 12 web zps matches, if there aren't enough search suggestions, "
        "url suggestions should not take their place. Instead less "
        "overall suggestions should be shown.");
    test(
        {
            CreateMatch(100, omnibox::GROUP_VISITED_DOC_RELATED),
            CreateMatch(99, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(98, omnibox::GROUP_MOST_VISITED),
            CreateMatch(97, omnibox::GROUP_MOST_VISITED),
            CreateMatch(96, omnibox::GROUP_MOST_VISITED),
            CreateMatch(95, omnibox::GROUP_MOST_VISITED),
            CreateMatch(94, omnibox::GROUP_MOST_VISITED),
            CreateMatch(93, omnibox::GROUP_MOST_VISITED),
            CreateMatch(91, omnibox::GROUP_MOST_VISITED),
            CreateMatch(90, omnibox::GROUP_MOST_VISITED),
            CreateMatch(89, omnibox::GROUP_MOST_VISITED),
            CreateMatch(88, omnibox::GROUP_MOST_VISITED),
        },
        {98, 97, 96, 95, 100, 99});
  }
}

TEST(AutocompleteGrouperSectionsTest, DesktopWebZpsWithActionsSection) {
  auto test = [](ACMatches matches, std::vector<int> expected_relevances) {
    PSections sections;
    omnibox::GroupConfigMap group_configs;
    // Max 3 suggestions, with an upper limit of 3 url suggestions.
    sections.push_back(
        std::make_unique<DesktopWebURLZpsSection>(group_configs, 3u));
    // Max 3 suggestions, with an upper limit of 3 contextual search
    // suggestions and one contextual action.
    sections.push_back(std::make_unique<DesktopWebSearchZpsSection>(
        group_configs, /*limit=*/3u, /*contextual_action_limit=*/1u,
        /*contextual_search_limit=*/3u));
    auto out_matches = Section::GroupMatches(std::move(sections), matches);
    VerifyMatches(out_matches, expected_relevances);
  };
  {
    SCOPED_TRACE("ZPS action matches group before contextual search matches");
    test(
        {
            CreateMatch(300, omnibox::GROUP_CONTEXTUAL_SEARCH_ACTION),
            CreateMatch(299, omnibox::GROUP_CONTEXTUAL_SEARCH_ACTION),
            CreateMatch(200, omnibox::GROUP_CONTEXTUAL_SEARCH),
            CreateMatch(199, omnibox::GROUP_CONTEXTUAL_SEARCH),
            CreateMatch(100, omnibox::GROUP_MOST_VISITED),
            CreateMatch(99, omnibox::GROUP_MOST_VISITED),
            CreateMatch(98, omnibox::GROUP_MOST_VISITED),
            CreateMatch(97, omnibox::GROUP_MOST_VISITED),
            CreateMatch(96, omnibox::GROUP_MOST_VISITED),
            CreateMatch(95, omnibox::GROUP_MOST_VISITED),
            CreateMatch(94, omnibox::GROUP_VISITED_DOC_RELATED),
            CreateMatch(93, omnibox::GROUP_VISITED_DOC_RELATED),
            CreateMatch(92, omnibox::GROUP_VISITED_DOC_RELATED),
            CreateMatch(91, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(90, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(89, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
        },
        // 3 URLs, 1 action, 2 contextual searches.
        {100, 99, 98, 300, 200, 199});
  }
}

TEST(AutocompleteGrouperSectionsTest, DesktopWebZpsNoContextualSuggestions) {
  auto test = [](ACMatches matches, std::vector<int> expected_relevances) {
    PSections sections;
    omnibox::GroupConfigMap group_configs;
    sections.push_back(
        std::make_unique<DesktopWebURLZpsSection>(group_configs, 3u));
    sections.push_back(std::make_unique<DesktopWebSearchZpsSection>(
        group_configs, /*limit=*/4u, /*contextual_action_limit=*/1u,
        /*contextual_search_limit=*/0u));
    auto out_matches = Section::GroupMatches(std::move(sections), matches);
    VerifyMatches(out_matches, expected_relevances);
  };
  {
    SCOPED_TRACE("ZPS contextual search matches limit 0");
    test(
        {
            CreateMatch(99, omnibox::GROUP_MOST_VISITED),
            CreateMatch(98, omnibox::GROUP_VISITED_DOC_RELATED),
            CreateMatch(97, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(96, omnibox::GROUP_CONTEXTUAL_SEARCH_ACTION),
            CreateMatch(95, omnibox::GROUP_CONTEXTUAL_SEARCH),
            CreateMatch(94, omnibox::GROUP_CONTEXTUAL_SEARCH_ACTION),
            CreateMatch(93, omnibox::GROUP_CONTEXTUAL_SEARCH),
        },
        // URLs, then searches, then one action, stable sorted.
        // No contextual search matches due to above configuration.
        {99, 98, 97, 96});
  }
}

TEST(AutocompleteGrouperSectionsTest, DesktopWebZpsContextualSuggestionsOnly) {
  auto test = [](ACMatches matches, std::vector<int> expected_relevances) {
    PSections sections;
    omnibox::GroupConfigMap group_configs;
    sections.push_back(
        std::make_unique<DesktopWebSearchZpsContextualOnlySection>(
            group_configs, /*contextual_action_limit=*/1u,
            /*contextual_search_limit=*/3u));
    auto out_matches = Section::GroupMatches(std::move(sections), matches);
    VerifyMatches(out_matches, expected_relevances);
  };
  {
    SCOPED_TRACE("ZPS contextual search matches only");
    test(
        {
            CreateMatch(99, omnibox::GROUP_MOST_VISITED),
            CreateMatch(98, omnibox::GROUP_VISITED_DOC_RELATED),
            CreateMatch(97, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST),
            CreateMatch(96, omnibox::GROUP_CONTEXTUAL_SEARCH_ACTION),
            CreateMatch(95, omnibox::GROUP_CONTEXTUAL_SEARCH),
            CreateMatch(94, omnibox::GROUP_CONTEXTUAL_SEARCH_ACTION),
            CreateMatch(93, omnibox::GROUP_CONTEXTUAL_SEARCH),
        },
        // Nothing but contextual action and contextual search matches.
        {96, 95, 93});
  }
}
#endif  // !(BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS))

// Test that (on Android) sections are grouped by Search vs URL.
#if BUILDFLAG(IS_ANDROID)
TEST(AutocompleteGrouperSectionsTest,
     AndroidNonZPSSection_groupsBySearchVsUrl) {
  auto test = [](bool show_only_search_suggestions, ACMatches matches,
                 std::vector<int> expected_relevances) {
    PSections sections;
    omnibox::GroupConfigMap group_configs;
    sections.push_back(std::make_unique<AndroidNonZPSSection>(
        show_only_search_suggestions, group_configs));
    auto out_matches = Section::GroupMatches(std::move(sections), matches);
    VerifyMatches(out_matches, expected_relevances);
  };

  auto make_search = [](int score) {
    auto match = CreateMatch(score, omnibox::GROUP_SEARCH);
    match.type = AutocompleteMatchType::SEARCH_HISTORY;
    return match;
  };

  auto make_url = [](int score) {
    auto match = CreateMatch(score, omnibox::GROUP_OTHER_NAVS);
    match.type = AutocompleteMatchType::NAVSUGGEST;
    return match;
  };

  constexpr bool kSearchesOnly = true;
  constexpr bool kSearchesAndUrls = false;

  {
    SCOPED_TRACE("No matches = no crashes.");
    test(kSearchesAndUrls, {}, {});
    test(kSearchesOnly, {}, {});
  }
  {
    SCOPED_TRACE("Grouping top section only w/ Search.");
    test(kSearchesAndUrls, {make_search(100)}, {100});
    test(kSearchesOnly, {make_search(100)}, {100});
  }
  {
    SCOPED_TRACE("Grouping top section only w/ URL.");
    test(kSearchesAndUrls, {make_url(100)}, {100});
    // Top URL is allowed.
    test(kSearchesOnly, {make_url(100)}, {100});
  }
  {
    SCOPED_TRACE("Grouping top section only w/ multiple URLs.");
    ACMatches matches{
        make_url(20),
        make_url(19),
        make_url(18),
    };
    test(kSearchesAndUrls, matches, {20, 19, 18});
    // Only top URL is allowed.
    test(kSearchesOnly, matches, {20});
  }
  {
    SCOPED_TRACE("Grouping top two sections.");
    ACMatches matches{
        make_url(20),    make_url(19),   make_url(18),
        make_search(10), make_search(9),
    };

    test(kSearchesAndUrls, matches,
         // 20     -- default match.
         // 10, 9  -- top searches.
         // 19, 18 -- top URLs.
         {20, 10, 9, 19, 18});

    test(kSearchesOnly, matches,
         // 20     -- default match (url).
         // 10, 9  -- top searches.
         {20, 10, 9});
  }
  {
    SCOPED_TRACE("Grouping all sections.");
    ACMatches matches{
        make_url(20),
        // top adaptive group
        make_url(19),
        make_url(18),
        make_search(10),
        make_search(9),
        make_url(17),
        // bottom adaptive group
        make_url(16),
        make_search(8),
        make_search(7),
        make_url(15),
        make_url(14),
        make_search(6),
        make_search(5),
        make_url(13),
        make_url(12),
    };

    test(kSearchesAndUrls, matches,
         {
             20,                             // the default match
             10, 9, 19, 18, 17,              // the top adaptive group
             8, 7, 6, 5, 16, 15, 14, 13, 12  // the bottom adaptive group.
         });

    test(kSearchesOnly, matches,
         {
             20,                 // Default match is URL
             10, 9, 8, 7, 6, 5,  // top adaptive group.
         });
  }
}

TEST(AutocompleteGrouperSectionsTest,
     AndroidNonZPSSection_richCardInFirstPosition) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kOmniboxAnswerActions,
      {{OmniboxFieldTrial::kAnswerActionsShowRichCard.name, "true"}});
  auto test = [](ACMatches matches, std::vector<int> expected_relevances) {
    PSections sections;
    omnibox::GroupConfigMap group_configs;
    AndroidNonZPSSection::set_num_visible_matches(5);
    sections.push_back(
        std::make_unique<AndroidNonZPSSection>(false, group_configs));
    auto out_matches = Section::GroupMatches(std::move(sections), matches);
    VerifyMatches(out_matches, expected_relevances);
  };

  auto make_search = [](int score) {
    auto match = CreateMatch(score, omnibox::GROUP_SEARCH);
    match.type = AutocompleteMatchType::SEARCH_HISTORY;
    return match;
  };

  auto make_url = [](int score) {
    auto match = CreateMatch(score, omnibox::GROUP_OTHER_NAVS);
    match.type = AutocompleteMatchType::NAVSUGGEST;
    return match;
  };

  auto make_rich_card = [](int score) {
    auto match = CreateMatch(score, omnibox::GROUP_MOBILE_RICH_ANSWER);
    match.type = AutocompleteMatchType::SEARCH_HISTORY;
    return match;
  };

  {
    SCOPED_TRACE("No matches, no crashes.");
    test({}, {});
  }

  SCOPED_TRACE("Card in first position");
  test(
      {
          make_rich_card(20),
          make_url(19),
          make_url(18),
          make_search(10),
          make_search(9),
      },
      // 20     -- rich answer card
      // 10, 9  -- top searches.
      // 19, 18 -- top URLs.
      {20, 10, 9, 19, 18});
}

TEST(AutocompleteGrouperSectionsTest,
     AndroidNonZPSSection_richCardAboveKeyboard) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kOmniboxAnswerActions,
      {{OmniboxFieldTrial::kAnswerActionsShowRichCard.name, "true"},
       {OmniboxFieldTrial::kAnswerActionsShowAboveKeyboard.name, "true"},
       {OmniboxFieldTrial::kAnswerActionsShowIfUrlsPresent.name, "true"}});
  auto test = [](ACMatches matches, std::vector<int> expected_relevances) {
    PSections sections;
    omnibox::GroupConfigMap group_configs;
    AndroidNonZPSSection::set_num_visible_matches(5);
    sections.push_back(
        std::make_unique<AndroidNonZPSSection>(false, group_configs));
    auto out_matches = Section::GroupMatches(std::move(sections), matches);
    VerifyMatches(out_matches, expected_relevances);
  };

  auto make_search = [](int score) {
    auto match = CreateMatch(score, omnibox::GROUP_SEARCH);
    match.type = AutocompleteMatchType::SEARCH_HISTORY;
    return match;
  };

  auto make_url = [](int score) {
    auto match = CreateMatch(score, omnibox::GROUP_OTHER_NAVS);
    match.type = AutocompleteMatchType::NAVSUGGEST;
    return match;
  };

  auto make_rich_card = [](int score) {
    auto match = CreateMatch(score, omnibox::GROUP_MOBILE_RICH_ANSWER);
    match.type = AutocompleteMatchType::SEARCH_HISTORY;
    omnibox::RichAnswerTemplate answer_template;
    match.answer_template = answer_template;
    return match;
  };

  {
    SCOPED_TRACE("No matches, no crashes.");
    test({}, {});
  }

  SCOPED_TRACE("Card in last position of visible matches");
  test(
      {
          make_url(19),
          make_url(18),
          make_rich_card(20),
          make_search(10),
          make_search(9),
          make_search(8),
          make_search(7),
      },
      // 19     -- default match url
      // 10, 9  -- top searches.
      // 18 -- remaining URL.
      // 20 -- rich answer card
      // 8, 7 -- below the fold matches
      {19, 10, 9, 18, 20, 8, 7});
}

TEST(AutocompleteGrouperSectionsTest,
     AndroidNonZPSSection_hideCardWhenUrlsPresent) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kOmniboxAnswerActions,
      {{OmniboxFieldTrial::kAnswerActionsShowRichCard.name, "true"},
       {OmniboxFieldTrial::kAnswerActionsShowAboveKeyboard.name, "true"},
       {OmniboxFieldTrial::kAnswerActionsShowIfUrlsPresent.name, "false"}});

  auto test = [](ACMatches matches, std::vector<int> expected_relevances) {
    PSections sections;
    omnibox::GroupConfigMap group_configs;
    AndroidNonZPSSection::set_num_visible_matches(5);
    sections.push_back(
        std::make_unique<AndroidNonZPSSection>(false, group_configs));
    auto out_matches = Section::GroupMatches(std::move(sections), matches);
    VerifyMatches(out_matches, expected_relevances);
  };

  auto make_search = [](int score) {
    auto match = CreateMatch(score, omnibox::GROUP_SEARCH);
    match.type = AutocompleteMatchType::SEARCH_HISTORY;
    return match;
  };

  auto make_url = [](int score) {
    auto match = CreateMatch(score, omnibox::GROUP_OTHER_NAVS);
    match.type = AutocompleteMatchType::NAVSUGGEST;
    return match;
  };

  auto make_rich_card = [](int score) {
    auto match = CreateMatch(score, omnibox::GROUP_MOBILE_RICH_ANSWER);
    match.type = AutocompleteMatchType::SEARCH_HISTORY;
    omnibox::RichAnswerTemplate answer_template;
    match.answer_template = answer_template;
    return match;
  };

  {
    SCOPED_TRACE("No matches, no crashes.");
    test({}, {});
  }

  SCOPED_TRACE("Card in last position of visible matches");
  test(
      {
          make_url(19),
          make_url(18),
          make_rich_card(20),
          make_search(10),
          make_search(9),
          make_search(8),
          make_search(7),
      },
      // 19     -- default match url
      // 20, 10 -- top searches.
      // Answer(20) counts as a plain search due to presence of urls.
      // 18 -- remaining URL.
      // 9, 8, 7 -- below the fold matches
      {19, 20, 10, 18, 9, 8, 7});
}

#endif
