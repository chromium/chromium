// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_controller.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/omnibox/browser/actions/contextual_search_action.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_enums.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_test_util.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/fake_autocomplete_controller.h"
#include "components/omnibox/browser/fake_autocomplete_provider.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/fake_tab_matcher.h"
#include "components/omnibox/browser/keyword_provider.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/enterprise/enterprise_search_manager.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/answer_type.pb.h"
#include "third_party/omnibox_proto/rich_answer_template.pb.h"

using ::testing::ElementsAre;
using ::testing::Pair;
using ::testing::WhenSorted;

class AutocompleteControllerTest : public testing::Test {
 public:
  AutocompleteControllerTest() : controller_(&task_environment_) {}

  void SetUp() override {
    EnterpriseSearchManager::RegisterProfilePrefs(pref_service()->registry());
  }

  void SetAutocompleteMatches(const std::vector<AutocompleteMatch>& matches) {
    controller_.internal_result_.ClearMatches();
    controller_.internal_result_.AppendMatches(matches);
  }

  void UpdateSearchboxStats() {
    controller_.UpdateSearchboxStats(&controller_.internal_result_);
  }

  void UpdateShownInSession() {
    controller_.UpdateShownInSession(&controller_.internal_result_);
  }

  void MaybeRemoveCompanyEntityImages() {
    controller_.MaybeRemoveCompanyEntityImages(&controller_.internal_result_);
  }

  bool ImageURLAndImageDominantColorIsEmpty(size_t index) {
    return controller_.internal_result_.match_at(index)->image_url.is_empty() &&
           controller_.internal_result_.match_at(index)
               ->image_dominant_color.empty();
  }

  FakeAutocompleteProviderClient* provider_client() {
    return static_cast<FakeAutocompleteProviderClient*>(
        controller_.autocomplete_provider_client());
  }

  sync_preferences::TestingPrefServiceSyncable* pref_service() {
    return static_cast<sync_preferences::TestingPrefServiceSyncable*>(
        provider_client()->GetPrefs());
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeAutocompleteController controller_;
};

TEST_F(AutocompleteControllerTest, UpdateShownInSessionOmitAsyncMatches) {
  std::vector<AutocompleteMatch> matches;

  AutocompleteInput input(u"abc", 3u, metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_omit_asynchronous_matches(true);
  controller_.input_ = input;

  matches.push_back(CreateSearchMatch(u"abc"));
  SetAutocompleteMatches(matches);

  UpdateShownInSession();

  for (size_t i = 0; i < controller_.internal_result_.size(); i++) {
    const auto* match = controller_.internal_result_.match_at(i);
    ASSERT_FALSE(match->session);
  }
}

TEST_F(AutocompleteControllerTest, UpdateShownInSessionTypedThenZeroPrefix) {
  std::vector<AutocompleteMatch> matches;

  AutocompleteInput typed_input(u"abc", 3u, metrics::OmniboxEventProto::OTHER,
                                TestSchemeClassifier());
  controller_.input_ = typed_input;

  matches.push_back(CreateSearchMatch(u"abc"));
  SetAutocompleteMatches(matches);

  UpdateSearchboxStats();
  UpdateShownInSession();

  for (size_t i = 0; i < controller_.internal_result_.size(); i++) {
    const auto* match = controller_.internal_result_.match_at(i);

    ASSERT_FALSE(match->session->zero_prefix_suggestions_shown_in_session);
    ASSERT_FALSE(
        match->session->zero_prefix_search_suggestions_shown_in_session);
    ASSERT_FALSE(match->session->zero_prefix_url_suggestions_shown_in_session);

    ASSERT_TRUE(match->session->typed_suggestions_shown_in_session);
    ASSERT_TRUE(match->session->typed_search_suggestions_shown_in_session);
    ASSERT_FALSE(match->session->typed_url_suggestions_shown_in_session);
  }

  matches.push_back(
      CreateHistoryURLMatch(/*destination_url=*/"https://www.abc.com/"));
  SetAutocompleteMatches(matches);

  UpdateSearchboxStats();
  UpdateShownInSession();

  for (size_t i = 0; i < controller_.internal_result_.size(); i++) {
    const auto* match = controller_.internal_result_.match_at(i);

    ASSERT_FALSE(match->session->zero_prefix_suggestions_shown_in_session);
    ASSERT_FALSE(
        match->session->zero_prefix_search_suggestions_shown_in_session);
    ASSERT_FALSE(match->session->zero_prefix_url_suggestions_shown_in_session);

    ASSERT_TRUE(match->session->typed_suggestions_shown_in_session);
    ASSERT_TRUE(match->session->typed_search_suggestions_shown_in_session);
    ASSERT_TRUE(match->session->typed_url_suggestions_shown_in_session);
  }

  matches.clear();

  AutocompleteInput zero_prefix_input(
      u"", 0u, metrics::OmniboxEventProto::OTHER, TestSchemeClassifier());
  zero_prefix_input.set_focus_type(
      metrics::OmniboxFocusType::INTERACTION_FOCUS);
  controller_.input_ = zero_prefix_input;

  matches.push_back(CreateZeroPrefixSearchMatch(u"abc"));
  SetAutocompleteMatches(matches);

  UpdateSearchboxStats();
  UpdateShownInSession();

  for (size_t i = 0; i < controller_.internal_result_.size(); i++) {
    const auto* match = controller_.internal_result_.match_at(i);

    ASSERT_TRUE(match->session->zero_prefix_suggestions_shown_in_session);
    ASSERT_TRUE(
        match->session->zero_prefix_search_suggestions_shown_in_session);
    ASSERT_FALSE(match->session->zero_prefix_url_suggestions_shown_in_session);

    ASSERT_TRUE(match->session->typed_suggestions_shown_in_session);
    ASSERT_TRUE(match->session->typed_search_suggestions_shown_in_session);
    ASSERT_TRUE(match->session->typed_url_suggestions_shown_in_session);
  }

  matches.push_back(CreateHistoryURLMatch(
      /*destination_url=*/"https://www.abc.com/", /*is_zero_prefix=*/true));
  SetAutocompleteMatches(matches);

  UpdateSearchboxStats();
  UpdateShownInSession();

  for (size_t i = 0; i < controller_.internal_result_.size(); i++) {
    const auto* match = controller_.internal_result_.match_at(i);

    ASSERT_TRUE(match->session->zero_prefix_suggestions_shown_in_session);
    ASSERT_TRUE(
        match->session->zero_prefix_search_suggestions_shown_in_session);
    ASSERT_TRUE(match->session->zero_prefix_url_suggestions_shown_in_session);

    ASSERT_TRUE(match->session->typed_suggestions_shown_in_session);
    ASSERT_TRUE(match->session->typed_search_suggestions_shown_in_session);
    ASSERT_TRUE(match->session->typed_url_suggestions_shown_in_session);
  }
}

TEST_F(AutocompleteControllerTest, UpdateShownInSessionZeroPrefixThenTyped) {
  std::vector<AutocompleteMatch> matches;

  AutocompleteInput zero_prefix_input(
      u"", 0u, metrics::OmniboxEventProto::OTHER, TestSchemeClassifier());
  zero_prefix_input.set_focus_type(
      metrics::OmniboxFocusType::INTERACTION_FOCUS);
  controller_.input_ = zero_prefix_input;

  matches.push_back(CreateZeroPrefixSearchMatch(u"abc"));
  SetAutocompleteMatches(matches);

  UpdateSearchboxStats();
  UpdateShownInSession();

  for (size_t i = 0; i < controller_.internal_result_.size(); i++) {
    const auto* match = controller_.internal_result_.match_at(i);

    ASSERT_TRUE(match->session->zero_prefix_suggestions_shown_in_session);
    ASSERT_TRUE(
        match->session->zero_prefix_search_suggestions_shown_in_session);
    ASSERT_FALSE(match->session->zero_prefix_url_suggestions_shown_in_session);

    ASSERT_FALSE(match->session->typed_suggestions_shown_in_session);
    ASSERT_FALSE(match->session->typed_search_suggestions_shown_in_session);
    ASSERT_FALSE(match->session->typed_url_suggestions_shown_in_session);
  }

  matches.push_back(CreateHistoryURLMatch(
      /*destination_url=*/"https://www.abc.com/", /*is_zero_prefix=*/true));
  SetAutocompleteMatches(matches);

  UpdateSearchboxStats();
  UpdateShownInSession();

  for (size_t i = 0; i < controller_.internal_result_.size(); i++) {
    const auto* match = controller_.internal_result_.match_at(i);

    ASSERT_TRUE(match->session->zero_prefix_suggestions_shown_in_session);
    ASSERT_TRUE(
        match->session->zero_prefix_search_suggestions_shown_in_session);
    ASSERT_TRUE(match->session->zero_prefix_url_suggestions_shown_in_session);

    ASSERT_FALSE(match->session->typed_suggestions_shown_in_session);
    ASSERT_FALSE(match->session->typed_search_suggestions_shown_in_session);
    ASSERT_FALSE(match->session->typed_url_suggestions_shown_in_session);
  }

  matches.clear();

  AutocompleteInput typed_input(u"abc", 3u, metrics::OmniboxEventProto::OTHER,
                                TestSchemeClassifier());
  controller_.input_ = typed_input;

  matches.push_back(CreateSearchMatch(u"abc"));
  SetAutocompleteMatches(matches);

  UpdateSearchboxStats();
  UpdateShownInSession();

  for (size_t i = 0; i < controller_.internal_result_.size(); i++) {
    const auto* match = controller_.internal_result_.match_at(i);

    ASSERT_TRUE(match->session->zero_prefix_suggestions_shown_in_session);
    ASSERT_TRUE(
        match->session->zero_prefix_search_suggestions_shown_in_session);
    ASSERT_TRUE(match->session->zero_prefix_url_suggestions_shown_in_session);

    ASSERT_TRUE(match->session->typed_suggestions_shown_in_session);
    ASSERT_TRUE(match->session->typed_search_suggestions_shown_in_session);
    ASSERT_FALSE(match->session->typed_url_suggestions_shown_in_session);
  }

  matches.push_back(
      CreateHistoryURLMatch(/*destination_url=*/"https://www.abc.com/"));
  SetAutocompleteMatches(matches);

  UpdateSearchboxStats();
  UpdateShownInSession();

  for (size_t i = 0; i < controller_.internal_result_.size(); i++) {
    const auto* match = controller_.internal_result_.match_at(i);

    ASSERT_TRUE(match->session->zero_prefix_suggestions_shown_in_session);
    ASSERT_TRUE(
        match->session->zero_prefix_search_suggestions_shown_in_session);
    ASSERT_TRUE(match->session->zero_prefix_url_suggestions_shown_in_session);

    ASSERT_TRUE(match->session->typed_suggestions_shown_in_session);
    ASSERT_TRUE(match->session->typed_search_suggestions_shown_in_session);
    ASSERT_TRUE(match->session->typed_url_suggestions_shown_in_session);
  }
}

TEST_F(AutocompleteControllerTest, RemoveCompanyEntityImage) {
  base::HistogramTester histogram_tester;
  std::vector<AutocompleteMatch> matches;
  // To ablate entity image the historical match must be the first and the
  // company entity can be in any other slot.
  matches.push_back(
      CreateHistoryURLMatch(/*destination_url=*/"https://www.wellsfargo.com/"));
  matches.push_back(CreateSearchMatch());
  matches.push_back(
      CreateCompanyEntityMatch(/*website_uri=*/"https://www.wellsfargo.com/"));

  SetAutocompleteMatches(matches);
  ASSERT_FALSE(ImageURLAndImageDominantColorIsEmpty(/*index=*/2));

  MaybeRemoveCompanyEntityImages();
  ASSERT_TRUE(ImageURLAndImageDominantColorIsEmpty(/*index=*/2));
  histogram_tester.ExpectBucketCount("Omnibox.CompanyEntityImageAblated", true,
                                     1);
}

TEST_F(AutocompleteControllerTest, CompanyEntityImageNotRemoved) {
  // History match is not the first suggestion. Entity's image should not be
  // removed.
  {
    base::HistogramTester histogram_tester;
    std::vector<AutocompleteMatch> matches;
    matches.push_back(CreateCompanyEntityMatch(
        /*website_uri=*/"https://www.wellsfargo.com/"));
    matches.push_back(CreateHistoryURLMatch(
        /*destination_url=*/"https://www.wellsfargo.com/"));
    matches.push_back(CreateSearchMatch());

    SetAutocompleteMatches(matches);
    ASSERT_FALSE(ImageURLAndImageDominantColorIsEmpty(/*index=*/0));

    MaybeRemoveCompanyEntityImages();
    // The entity's image_url should remain as is.
    ASSERT_FALSE(ImageURLAndImageDominantColorIsEmpty(/*index=*/0));
    histogram_tester.ExpectBucketCount("Omnibox.CompanyEntityImageAblated",
                                       false, 1);
  }

  // History match is the first suggestion, but there isn't a matching company
  // entity.
  {
    base::HistogramTester histogram_tester;
    std::vector<AutocompleteMatch> matches;
    matches.push_back(CreateHistoryURLMatch(
        /*destination_url=*/"https://www.wellsfargo.com/"));
    matches.push_back(
        CreateCompanyEntityMatch(/*website_uri=*/"https://www.weather.com/"));

    SetAutocompleteMatches(matches);
    ASSERT_FALSE(ImageURLAndImageDominantColorIsEmpty(/*index=*/1));

    MaybeRemoveCompanyEntityImages();
    // The entity's image_url should remain as is.
    ASSERT_FALSE(ImageURLAndImageDominantColorIsEmpty(/*index=*/1));
    histogram_tester.ExpectBucketCount("Omnibox.CompanyEntityImageAblated",
                                       false, 1);
  }
  // There is a company entity, but no history match.
  {
    base::HistogramTester histogram_tester;
    std::vector<AutocompleteMatch> matches;
    matches.push_back(CreateSearchMatch());
    matches.push_back(
        CreateCompanyEntityMatch(/*website_uri=*/"https://www.weather.com/"));

    SetAutocompleteMatches(matches);
    ASSERT_FALSE(ImageURLAndImageDominantColorIsEmpty(/*index=*/1));

    MaybeRemoveCompanyEntityImages();
    // The entity's image_url should remain as is.
    ASSERT_FALSE(ImageURLAndImageDominantColorIsEmpty(/*index=*/1));
    histogram_tester.ExpectBucketCount("Omnibox.CompanyEntityImageAblated",
                                       false, 1);
  }
}

// Desktop has some special handling for bare '@' inputs.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(AutocompleteControllerTest, FilterMatchesForInstantKeywordWithBareAt) {
  SetAutocompleteMatches({
      CreateSearchMatch(u"@"),
      CreateCompanyEntityMatch("https://example.com"),
      CreateHistoryURLMatch("https://example.com"),
      CreateStarterPackMatch(u"@bookmarks"),
      CreateStarterPackMatch(u"@history"),
      CreateStarterPackMatch(u"@tabs"),
      CreateFeaturedEnterpriseSearch(u"@work"),
  });

  AutocompleteInput input(u"@", 1u, metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  controller_.MaybeCleanSuggestionsForKeywordMode(
      input, &controller_.internal_result_);

  EXPECT_EQ(controller_.internal_result_.size(), 5u);
  EXPECT_TRUE(std::all_of(
      controller_.internal_result_.begin(), controller_.internal_result_.end(),
      [](const auto& match) {
        return match.type == AutocompleteMatchType::STARTER_PACK ||
               match.type ==
                   AutocompleteMatchType::FEATURED_ENTERPRISE_SEARCH ||
               match.contents == u"@";
      }));
}
#endif

TEST_F(AutocompleteControllerTest, UpdateResult_SyncAnd2Async) {
  auto sync_match = CreateSearchMatch("sync", true, 1300);
  auto async_match1 = CreateSearchMatch("async_1", true, 1200);
  auto async_match2 = CreateSearchMatch("async_2", true, 1250);
  {
    SCOPED_TRACE("Sync pass.");
    EXPECT_THAT(controller_.SimulateAutocompletePass(true, false, {sync_match}),
                testing::ElementsAreArray({
                    "sync",
                }));
  }
  {
    SCOPED_TRACE("1st async pass.");
    EXPECT_THAT(controller_.SimulateAutocompletePass(
                    false, false, {sync_match, async_match1}),
                testing::ElementsAreArray({
                    "sync",
                    "async_1",
                }));
  }
  {
    SCOPED_TRACE(
        "Last async pass. Verify the correct matches are shown ranked by "
        "relevance.");
    EXPECT_THAT(controller_.SimulateAutocompletePass(
                    false, true, {async_match1, async_match2}),
                testing::ElementsAreArray({
                    "async_2",
                    "async_1",
                }));
  }
}

TEST_F(AutocompleteControllerTest, UpdateResult_TransferringOldMatches) {
  auto pass1_match1 = CreateSearchMatch("pass1_match1", true, 1300);
  auto pass1_match2 = CreateSearchMatch("pass1_match2", true, 1200);
  auto pass1_match3 = CreateSearchMatch("pass1_match3", true, 1100);
  auto pass2_match1 = CreateSearchMatch("pass2_match1", true, 1000);
  auto pass3_match2 = CreateSearchMatch("pass3_match2", true, 900);
  auto pass3_match3 = CreateSearchMatch("pass3_match3", true, 800);
  auto pass3_match4 = CreateSearchMatch("pass3_match4", true, 700);
  auto pass4_match1 = CreateSearchMatch("pass4_match1", true, 600);
  auto pass5_match1 = CreateSearchMatch("pass5_match1", true, 500);
  EXPECT_THAT(controller_.SimulateAutocompletePass(
                  true, false, {pass1_match1, pass1_match2, pass1_match3}),
              testing::ElementsAreArray({
                  "pass1_match1",
                  "pass1_match2",
                  "pass1_match3",
              }));
  // # of matches decreased from 3 to 2. So 1 match should be transferred. The
  // lowest ranked match should be transferred. It should keep its score and be
  // ranked above the new non-transferred match.
  EXPECT_THAT(controller_.SimulateAutocompletePass(
                  false, false, {pass1_match1, pass2_match1}),
              testing::ElementsAreArray({
                  "pass1_match1",
                  "pass1_match3",
                  "pass2_match1",
              }));
  // # of matches remained 3. So no matches should be transferred.
  EXPECT_THAT(controller_.SimulateAutocompletePass(
                  false, false, {pass3_match2, pass3_match3, pass3_match4}),
              testing::ElementsAreArray({
                  "pass3_match2",
                  "pass3_match3",
                  "pass3_match4",
              }));
  // Transferred matches should not be allowed to be default.
  EXPECT_THAT(
      controller_.SimulateAutocompletePass(false, false, {pass4_match1}),
      testing::ElementsAreArray({
          "pass4_match1",
          "pass3_match3",
          "pass3_match4",
      }));
  // Lowest ranked match should be transferred. But old matches still present
  // shouldn't count, and the next lowest match should be transferred.
  // Transferred match scores should be capped to the new default, therefore,
  // the transferred `pass3_match3` should be demoted to last even though it
  // originally outscored `pass3_match4`.
  EXPECT_THAT(controller_.SimulateAutocompletePass(
                  false, false, {pass4_match1, pass3_match4}),
              testing::ElementsAreArray({
                  "pass4_match1",
                  "pass3_match4",
                  "pass3_match3",
              }));
  // Sync updates should also transfer old matches. Lowest ranked, not
  // necessarily lowest scored, match should be transferred.
  EXPECT_THAT(controller_.SimulateAutocompletePass(true, false, {pass5_match1}),
              testing::ElementsAreArray({
                  "pass5_match1",
                  "pass3_match3",
                  "pass3_match4",
              }));
  // Expire updates should not transfer old matches.
  EXPECT_THAT(controller_.SimulateExpirePass(), testing::ElementsAreArray({
                                                    "pass5_match1",
                                                }));
  // Async updates after the expire update should transfer matches.
  EXPECT_THAT(controller_.SimulateAutocompletePass(false, false, {}),
              testing::ElementsAreArray({
                  "pass5_match1",
              }));
  // The last async pass shouldn't transfer matches.
  EXPECT_THAT(controller_.SimulateAutocompletePass(true, true, {}),
              testing::ElementsAreArray<std::string>({}));
}

TEST_F(AutocompleteControllerTest, UpdateResult_PreservingDefault) {
  auto match1 = CreateSearchMatch("match1", true, 100);
  auto match2 = CreateSearchMatch("match2", true, 200);
  auto match3 = CreateSearchMatch("match3", true, 300);
  auto match4 = CreateSearchMatch("match4", true, 400);
  auto match5 = CreateSearchMatch("match5", true, 500);
  auto match6 = CreateSearchMatch("match6", true, 600);
  auto match7 = CreateSearchMatch("match7", true, 700);
  auto match8 = CreateSearchMatch("match8", true, 800);
  auto match9 = CreateSearchMatch("match9", true, 900);
  {
    SCOPED_TRACE("Load a default suggestion.");
    EXPECT_THAT(controller_.SimulateAutocompletePass(true, true, {match1}),
                testing::ElementsAreArray({
                    "match1",
                }));
  }
  {
    SCOPED_TRACE("Don't preserve default on sync pass with short inputs.");
    EXPECT_THAT(controller_.SimulateAutocompletePass(
                    true, false, {match1, match2},
                    FakeAutocompleteController::CreateInput(u"x")),
                testing::ElementsAreArray({
                    "match2",
                    "match1",
                }));
  }
  {
    SCOPED_TRACE("Preserve default on async pass with short inputs.");
    EXPECT_THAT(
        controller_.SimulateAutocompletePass(false, false, {match2, match3}),
        testing::ElementsAreArray({
            "match2",
            "match3",
        }));
    // Preserve default on last async pass with short inputs. Preserve default
  }
  {
    SCOPED_TRACE("across multiple passes.");
    EXPECT_THAT(
        controller_.SimulateAutocompletePass(false, true, {match2, match4}),
        testing::ElementsAreArray({
            "match2",
            "match4",
        }));
    // Preserve default on sync pass with long inputs. Preserve default across
  }
  {
    SCOPED_TRACE("multiple inputs.");
    EXPECT_THAT(
        controller_.SimulateAutocompletePass(true, false, {match2, match5}),
        testing::ElementsAreArray({
            "match2",
            "match5",
        }));
  }
  {
    SCOPED_TRACE("Preserve default on async pass with long inputs.");
    EXPECT_THAT(
        controller_.SimulateAutocompletePass(false, false, {match2, match6}),
        testing::ElementsAreArray({
            "match2",
            "match6",
        }));
  }
  {
    SCOPED_TRACE("Don't preserve default if it's transferred.");
    EXPECT_THAT(
        controller_.SimulateAutocompletePass(false, false, {match7, match8}),
        testing::ElementsAreArray({
            "match8",
            "match7",
        }));
  }
  {
    SCOPED_TRACE("Preserve default on last async pass with long inputs.");
    EXPECT_THAT(
        controller_.SimulateAutocompletePass(false, true, {match8, match9}),
        testing::ElementsAreArray({
            "match8",
            "match9",
        }));
  }
}

TEST_F(AutocompleteControllerTest, UpdateResult_Ranking) {
  // Higher scored suggestions are ranked higher.
  // Clear results between each test to avoid default preserving applying.
  EXPECT_THAT(controller_.SimulateCleanAutocompletePass({
                  CreateSearchMatch("500", true, 500),
                  CreateSearchMatch("800", true, 800),
              }),
              testing::ElementsAreArray({
                  "800",
                  "500",
              }));

  // Default suggestion must be allowed to be default.
  EXPECT_THAT(controller_.SimulateCleanAutocompletePass({
                  CreateSearchMatch("500", true, 500),
                  CreateSearchMatch("800", false, 800),
              }),
              testing::ElementsAreArray({
                  "500",
                  "800",
              }));

// Android and iOS don't use the same grouping logic as desktop
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  // Searches should be grouped above non-shortcut-boosted URLs.
  EXPECT_THAT(controller_.SimulateCleanAutocompletePass({
                  CreateSearchMatch("search900", true, 900),
                  CreateHistoryUrlMlScoredMatch("history800", true, 800, 1),
                  CreateSearchMatch("search700", true, 700),
                  CreateHistoryUrlMlScoredMatch("history600", true, 600, 1),
                  CreateSearchMatch("search500", true, 500),
                  CreateHistoryUrlMlScoredMatch("history400", true, 400, 1),
              }),
              testing::ElementsAreArray({
                  "search900",
                  "search700",
                  "search500",
                  "history800",
                  "history600",
                  "history400",
              }));

  // Default can be a non-search if it's scored higher than all the searches.
  EXPECT_THAT(controller_.SimulateCleanAutocompletePass({
                  CreateSearchMatch("search900", true, 900),
                  CreateHistoryUrlMlScoredMatch("history800", true, 800, 1),
                  CreateSearchMatch("search700", true, 700),
                  CreateHistoryUrlMlScoredMatch("history600", true, 600, 1),
                  CreateSearchMatch("search500", true, 500),
                  CreateHistoryUrlMlScoredMatch("history400", true, 400, 1),
                  CreateHistoryUrlMlScoredMatch("history1000", true, 1000, 1),
              }),
              testing::ElementsAreArray({
                  "history1000",
                  "search900",
                  "search700",
                  "search500",
                  "history800",
                  "history600",
                  "history400",
              }));

  // Shortcut boosting is re-distributed when ML Scoring is enabled.  That is
  // tested in the `MlRanking` test below.
  OmniboxFieldTrial::ScopedMLConfigForTesting scoped_config;
  if (!scoped_config.GetMLConfig().ml_url_scoring) {
    // Shortcut boosted suggestions should be ranked above searches, even if
    // they're scored lower.
    EXPECT_THAT(controller_.SimulateCleanAutocompletePass({
                    CreateHistoryUrlMlScoredMatch("history800", true, 800, 1),
                    CreateHistoryUrlMlScoredMatch("history850", true, 850, 1),
                    CreateSearchMatch("search700", true, 700),
                    CreateSearchMatch("search750", true, 750),
                    CreateBoostedShortcutMatch("shortcut600", 600, 1),
                    CreateBoostedShortcutMatch("shortcut650", 650, 1),
                }),
                testing::ElementsAreArray({
                    "history850",
                    "shortcut650",
                    "shortcut600",
                    "search750",
                    "search700",
                    "history800",
                }));
  }

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
}

TEST_F(AutocompleteControllerTest, UpdateResult_ZPSEnabledAndShownInSession) {
  // Populate TemplateURLService with a keyword.
  TemplateURLData turl_data;
  turl_data.SetShortName(u"Keyword");
  turl_data.SetKeyword(u"keyword");
  turl_data.SetURL("https://google.com/search?q={searchTerms}");
  controller_.template_url_service_->Add(
      std::make_unique<TemplateURL>(turl_data));

  // Create a zero-suggest input.
  auto zps_input = FakeAutocompleteController::CreateInput(u"");
  zps_input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);

  {
    SCOPED_TRACE("Zero-prefix suggestions are offered synchronously");
    EXPECT_THAT(controller_.SimulateAutocompletePass(
                    /*sync=*/true, /*done=*/false,
                    {
                        CreatePersonalizedZeroPrefixMatch("zps_1", 1450),
                        CreatePersonalizedZeroPrefixMatch("zps_2", 1449),
                    },
                    zps_input),
                testing::ElementsAreArray({
                    "zps_1",
                    "zps_2",
                }));
    // Whether zero-suggest was enabled and the number of zero-prefix
    // suggestions shown in the session are updated in the internal result set.
    EXPECT_TRUE(controller_.internal_result_.zero_prefix_enabled_in_session());
    EXPECT_EQ(controller_.internal_result_
                  .num_zero_prefix_suggestions_shown_in_session(),
              2u);
    // Published result set does not get the session data.
    EXPECT_FALSE(
        controller_.published_result_.zero_prefix_enabled_in_session());
    EXPECT_EQ(controller_.published_result_
                  .num_zero_prefix_suggestions_shown_in_session(),
              0u);
    // Published matches contain the relevant session data in searchboxstats.
    EXPECT_TRUE(controller_.published_result_.match_at(0)
                    ->search_terms_args->searchbox_stats.zero_prefix_enabled());
    EXPECT_EQ(controller_.published_result_.match_at(0)
                  ->search_terms_args->searchbox_stats
                  .num_zero_prefix_suggestions_shown(),
              2u);
  }
  {
    SCOPED_TRACE("More zero-prefix suggestions are offered asynchronously");
    EXPECT_THAT(controller_.SimulateAutocompletePass(
                    /*sync=*/false, /*done=*/false,
                    {
                        CreatePersonalizedZeroPrefixMatch("zps_1", 1450),
                        CreatePersonalizedZeroPrefixMatch("zps_2", 1449),
                        CreatePersonalizedZeroPrefixMatch("zps_3", 1448),
                        CreatePersonalizedZeroPrefixMatch("zps_4", 1447),
                    },
                    zps_input),
                testing::ElementsAreArray({
                    "zps_1",
                    "zps_2",
                    "zps_3",
                    "zps_4",
                }));
    // If zero-prefix suggestions are offered multiple times in the session, the
    // most recent count is logged.
    EXPECT_TRUE(controller_.internal_result_.zero_prefix_enabled_in_session());
    EXPECT_EQ(controller_.internal_result_
                  .num_zero_prefix_suggestions_shown_in_session(),
              4u);
    // Published result set does not get the session data.
    EXPECT_FALSE(
        controller_.published_result_.zero_prefix_enabled_in_session());
    EXPECT_EQ(controller_.published_result_
                  .num_zero_prefix_suggestions_shown_in_session(),
              0u);
    // Published matches contain the relevant session data in searchboxstats.
    EXPECT_TRUE(controller_.published_result_.match_at(0)
                    ->search_terms_args->searchbox_stats.zero_prefix_enabled());
    EXPECT_EQ(controller_.published_result_.match_at(0)
                  ->search_terms_args->searchbox_stats
                  .num_zero_prefix_suggestions_shown(),
              4u);
  }
  {
    SCOPED_TRACE("Stop with clear_result=false is called due to user idleness");
    controller_.Stop(AutocompleteStopReason::kInteraction);
    // Stop with clear_result=false does not clear the internal result set and
    // does not notify `OnResultChanged()`.
    EXPECT_FALSE(controller_.internal_result_.empty());
    EXPECT_FALSE(controller_.published_result_.empty());
    // Whether zero-suggest was enabled and the number of zero-prefix
    // suggestions shown in the session are unchanged in the internal result
    // set.
    EXPECT_TRUE(controller_.internal_result_.zero_prefix_enabled_in_session());
    EXPECT_EQ(controller_.internal_result_
                  .num_zero_prefix_suggestions_shown_in_session(),
              4u);
    // Published result set does not get the session data.
    EXPECT_FALSE(
        controller_.published_result_.zero_prefix_enabled_in_session());
    EXPECT_EQ(controller_.published_result_
                  .num_zero_prefix_suggestions_shown_in_session(),
              0u);
    // Published matches contain the relevant session data in searchboxstats.
    EXPECT_TRUE(controller_.published_result_.match_at(0)
                    ->search_terms_args->searchbox_stats.zero_prefix_enabled());
    EXPECT_EQ(controller_.published_result_.match_at(0)
                  ->search_terms_args->searchbox_stats
                  .num_zero_prefix_suggestions_shown(),
              4u);
  }
  {
    SCOPED_TRACE("Prefix suggestions are offered synchronously");
    EXPECT_THAT(controller_.SimulateAutocompletePass(
                    /*sync=*/true, /*done=*/true,
                    {
                        CreateSearchMatch("search_1", true, 900),
                    }),
                testing::ElementsAreArray({
                    "search_1",
                }));
    // Whether zero-suggest was enabled and the number of zero-prefix
    // suggestions shown in the session are unchanged in the internal result
    // set.
    EXPECT_TRUE(controller_.internal_result_.zero_prefix_enabled_in_session());
    EXPECT_EQ(controller_.internal_result_
                  .num_zero_prefix_suggestions_shown_in_session(),
              4u);
    // Published result set does not get the session data.
    EXPECT_FALSE(
        controller_.published_result_.zero_prefix_enabled_in_session());
    EXPECT_EQ(controller_.published_result_
                  .num_zero_prefix_suggestions_shown_in_session(),
              0u);
    // Published matches contain the relevant session data in searchboxstats.
    EXPECT_TRUE(controller_.published_result_.match_at(0)
                    ->search_terms_args->searchbox_stats.zero_prefix_enabled());
    EXPECT_EQ(controller_.published_result_.match_at(0)
                  ->search_terms_args->searchbox_stats
                  .num_zero_prefix_suggestions_shown(),
              4u);
  }
  {
    SCOPED_TRACE("Stop with clear_result=true is called due to popup closing");
    controller_.Stop(AutocompleteStopReason::kClobbered);
    // Stop with clear_result=true clears the internal result set and notifies
    // `OnResultChanged()`.
    EXPECT_TRUE(controller_.internal_result_.empty());
    EXPECT_TRUE(controller_.published_result_.empty());
    // Whether zero-suggest was enabled and the number of zero-prefix
    // suggestions shown in the session are reset in the internal result set.
    EXPECT_FALSE(controller_.internal_result_.zero_prefix_enabled_in_session());
    EXPECT_EQ(controller_.internal_result_
                  .num_zero_prefix_suggestions_shown_in_session(),
              0u);
    // Published result set does not get the session data.
    EXPECT_FALSE(
        controller_.published_result_.zero_prefix_enabled_in_session());
    EXPECT_EQ(controller_.published_result_
                  .num_zero_prefix_suggestions_shown_in_session(),
              0u);
  }
}

// Android and iOS aren't ready for ML and won't pass this test because they
// have their own grouping code.
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB) && !BUILDFLAG(IS_ANDROID) && \
    !BUILDFLAG(IS_IOS)
TEST_F(AutocompleteControllerTest, MlRanking) {
  OmniboxFieldTrial::ScopedMLConfigForTesting scoped_ml_config;
  scoped_ml_config.GetMLConfig().ml_url_scoring = true;
  scoped_ml_config.GetMLConfig().url_scoring_model = true;

  EXPECT_THAT(controller_.SimulateCleanAutocompletePass({}),
              testing::ElementsAre());

  // Even if ML ranks a URL 0, it should still use traditional scores.
  EXPECT_THAT(controller_.SimulateCleanAutocompletePass({
                  CreateHistoryUrlMlScoredMatch("history", true, 1400, 0),
                  CreateSearchMatch("search", true, 1300),
              }),
              testing::ElementsAreArray({
                  "history",
                  "search",
              }));

  // Simple case of redistributing ranking among only URLs.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateHistoryUrlMlScoredMatch("history 1350 .5", true, 1350, .5),
          CreateSearchMatch("search 1400", false, 1400),
          CreateSearchMatch("search 800", true, 800),
          CreateSearchMatch("search 600", false, 600),
          CreateHistoryUrlMlScoredMatch("history 1200 .9", true, 1200, .9),
          CreateHistoryUrlMlScoredMatch("history 1100 .1", false, 1100, .1),
          CreateHistoryUrlMlScoredMatch("history 500 .2", true, 500, .2),
      }),
      testing::ElementsAreArray({
          "history 1200 .9",
          "search 1400",
          "search 800",
          "search 600",
          "history 1350 .5",
          "history 500 .2",
          "history 1100 .1",
      }));

  // When multiple URL suggestions have been assigned the same score by the ML
  // model, those suggestions which were top-ranked according to legacy scoring
  // should continue to be top-ranked once ML scoring has run.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateHistoryUrlMlScoredMatch("history A 1350 .2", true, 1350, .2),
          CreateHistoryUrlMlScoredMatch("history B 1200 .2", true, 1200, .2),
          CreateHistoryUrlMlScoredMatch("history C 1100 .2", false, 1100, .2),
          CreateHistoryUrlMlScoredMatch("history D 300 .2", true, 300, .2),
          CreateHistoryUrlMlScoredMatch("history E 200 .2", true, 200, .2),
          CreateHistoryUrlMlScoredMatch("history F 100 .2", true, 100, .2),
      }),
      testing::ElementsAreArray({
          "history A 1350 .2",
          "history B 1200 .2",
          "history C 1100 .2",
          "history D 300 .2",
          "history E 200 .2",
          "history F 100 .2",
      }));

  // Can change the default suggestion from 1 history to another.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateHistoryUrlMlScoredMatch("history 1400 .5", true, 1400, .5),
          CreateSearchMatch("search", true, 1300),
          CreateHistoryUrlMlScoredMatch("history 1200 .9", true, 1200, .9),
      }),
      testing::ElementsAreArray({
          "history 1200 .9",
          "search",
          "history 1400 .5",
      }));

  // Can change the default from search to history.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateSearchMatch("search 1300", true, 1300),
          CreateHistoryUrlMlScoredMatch("history 1400 .5", false, 1400, .5),
          CreateHistoryUrlMlScoredMatch("history 1200 .9", true, 1200, .9),
      }),
      testing::ElementsAreArray({
          "history 1200 .9",
          "search 1300",
          "history 1400 .5",
      }));

  // Can change the default from history to search.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateHistoryUrlMlScoredMatch("history 1400 .5", true, 1400, .5),
          CreateSearchMatch("search 1300", true, 1300),
          CreateHistoryUrlMlScoredMatch("history 1200 .9", false, 1200, .9),
      }),
      testing::ElementsAreArray({
          "search 1300",
          "history 1200 .9",
          "history 1400 .5",
      }));

  // Can redistribute shortcut boosting to non-shortcuts.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateSearchMatch("search 1300", true, 1300),
          CreateBoostedShortcutMatch("shortcut 1000 .1", 1000, .1),
          CreateSearchMatch("search 1200", true, 1200),
          CreateHistoryUrlMlScoredMatch("history 1400 .9", false, 1400, .9),
          CreateHistoryUrlMlScoredMatch("history 1100 .5", true, 1100, .5),
      }),
      testing::ElementsAreArray({
          "search 1300",
          "history 1400 .9",
          "search 1200",
          "history 1100 .5",
          "shortcut 1000 .1",
      }));

  // Can 'consume' shortcut boosting by assigning it to a match that's becoming
  // default anyways.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateSearchMatch("search 1300", true, 1300),
          CreateBoostedShortcutMatch("shortcut 1000 .1", 1000, .1),
          CreateSearchMatch("search 1200", true, 1200),
          CreateHistoryUrlMlScoredMatch("history 1400 .5", false, 1400, .5),
          CreateHistoryUrlMlScoredMatch("history 1100 .9", true, 1100, .9),
      }),
      testing::ElementsAreArray({
          "history 1100 .9",
          "search 1300",
          "search 1200",
          "history 1400 .5",
          "shortcut 1000 .1",
      }));

  // Can increase the number of URLs above searches.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateSearchMatch("search 1300", true, 1300),
          CreateBoostedShortcutMatch("shortcut 1000 .7", 1000, .7),
          CreateSearchMatch("search 1200", true, 1200),
          CreateHistoryUrlMlScoredMatch("history 1400 .5", false, 1400, .5),
          CreateHistoryUrlMlScoredMatch("history 1350 .2", false, 1350, .2),
          CreateHistoryUrlMlScoredMatch("history 1100 .8", true, 1100, .8),
          CreateHistoryUrlMlScoredMatch("history 1050 .9", false, 1050, .9),
      }),
      testing::ElementsAreArray({
          "history 1100 .8",
          "history 1050 .9",
          "search 1300",
          "search 1200",
          "shortcut 1000 .7",
          "history 1400 .5",
          "history 1350 .2",
      }));

  // Can increase the number of URLs above searches even when the default was a
  // URL.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateBoostedShortcutMatch("shortcut 1450 .7", 1450, .7),
          CreateSearchMatch("search 1200", true, 1200),
          CreateHistoryUrlMlScoredMatch("history 1400 .9", false, 1400, .9),
      }),
      testing::ElementsAreArray({
          "shortcut 1450 .7",
          "history 1400 .9",
          "search 1200",
      }));

  // Can decrease the number of URLs above searches.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateHistoryUrlMlScoredMatch("history 1400 .5", true, 1400, .5),
          CreateBoostedShortcutMatch("shortcut 1000 .1", 1000, .1),
          CreateSearchMatch("search 1300", true, 1300),
          CreateSearchMatch("search 1200", true, 1200),
          CreateHistoryUrlMlScoredMatch("history 1100 .9", true, 1100, .9),
      }),
      testing::ElementsAreArray({
          "history 1100 .9",
          "search 1300",
          "search 1200",
          "history 1400 .5",
          "shortcut 1000 .1",
      }));

  // When transferring matches, culls the lowest ML ranked matches, rather than
  // the lowest traditional ranked matches.
  controller_.internal_result_.ClearMatches();
  EXPECT_THAT(
      controller_.SimulateAutocompletePass(
          true, false,
          {
              CreateSearchMatch("search 1270", true, 1270),
              CreateSearchMatch("search 1260", true, 1260),
              CreateSearchMatch("search 1250", true, 1250),
              CreateSearchMatch("search 1240", true, 1240),
              CreateSearchMatch("search 1230", true, 1230),
              CreateSearchMatch("search 1220", true, 1220),
              CreateSearchMatch("search 1210", true, 1210),
              CreateHistoryUrlMlScoredMatch("history 1100 .5", true, 1100, .5),
              CreateHistoryUrlMlScoredMatch("history 1000 .9", true, 1000, .9),
          }),
      testing::ElementsAreArray({
          "search 1270",
          "search 1260",
          "search 1250",
          "search 1240",
          "search 1230",
          "search 1220",
          "search 1210",
          "history 1000 .9",
      }));

  // When not transferring matches, like above, culls the lowest ML ranked
  // matches, rather than the lowest traditional ranked matches.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateSearchMatch("search 1270", true, 1270),
          CreateSearchMatch("search 1260", true, 1260),
          CreateSearchMatch("search 1250", true, 1250),
          CreateSearchMatch("search 1240", true, 1240),
          CreateSearchMatch("search 1230", true, 1230),
          CreateSearchMatch("search 1220", true, 1220),
          CreateSearchMatch("search 1210", true, 1210),
          CreateHistoryUrlMlScoredMatch("history 1100 .5", true, 1100, .5),
          CreateHistoryUrlMlScoredMatch("history 1000 .9", true, 1000, .9),
      }),
      testing::ElementsAreArray({
          "search 1270",
          "search 1260",
          "search 1250",
          "search 1240",
          "search 1230",
          "search 1220",
          "search 1210",
          "history 1000 .9",
      }));
}

TEST_F(AutocompleteControllerTest, MlRanking_ApplyPiecewiseScoringTransform) {
  const std::vector<std::pair<double, int>> break_points = {
      {0, 500}, {0.25, 1000}, {0.75, 1300}, {1, 1500}};

  float ml_score = 0;
  EXPECT_EQ(AutocompleteController::ApplyPiecewiseScoringTransform(
                ml_score, break_points),
            500);

  ml_score = 0.186;
  EXPECT_EQ(AutocompleteController::ApplyPiecewiseScoringTransform(
                ml_score, break_points),
            872);

  ml_score = 0.25;
  EXPECT_EQ(AutocompleteController::ApplyPiecewiseScoringTransform(
                ml_score, break_points),
            1000);

  ml_score = 0.473;
  EXPECT_EQ(AutocompleteController::ApplyPiecewiseScoringTransform(
                ml_score, break_points),
            1133);

  ml_score = 0.75;
  EXPECT_EQ(AutocompleteController::ApplyPiecewiseScoringTransform(
                ml_score, break_points),
            1300);

  ml_score = 0.914;
  EXPECT_EQ(AutocompleteController::ApplyPiecewiseScoringTransform(
                ml_score, break_points),
            1431);

  ml_score = 1;
  EXPECT_EQ(AutocompleteController::ApplyPiecewiseScoringTransform(
                ml_score, break_points),
            1500);
}

TEST_F(AutocompleteControllerTest, MlRanking_PiecewiseMappedSearchBlending) {
  OmniboxFieldTrial::ScopedMLConfigForTesting scoped_ml_config;
  scoped_ml_config.GetMLConfig().ml_url_scoring = true;
  scoped_ml_config.GetMLConfig().url_scoring_model = true;
  scoped_ml_config.GetMLConfig().piecewise_mapped_search_blending = true;

  scoped_ml_config.GetMLConfig().piecewise_mapped_search_blending_break_points =
      "0,500;0.25,1000;0.75,1300;1,1500";
  scoped_ml_config.GetMLConfig()
      .piecewise_mapped_search_blending_grouping_threshold = 1100;
  scoped_ml_config.GetMLConfig()
      .piecewise_mapped_search_blending_relevance_bias = 0;

  EXPECT_THAT(controller_.SimulateCleanAutocompletePass({}),
              testing::ElementsAre());

  // If a (remote) document suggestion has a traditional score of zero, then the
  // final relevance score should remain zero (instead of using the piecewise ML
  // score mapping function to overwrite the relevance score). This will result
  // in the document suggestion getting culled from the final list of
  // suggestions.
  const auto type = AutocompleteMatchType::DOCUMENT_SUGGESTION;
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          // Final score: 1000
          CreateMlScoredMatch("document 1400 0.25", type, false, 1400, 0.25),
          // Final score: 0 (!= 1500)
          CreateMlScoredMatch("document 0 0.95", type, false, 0, 1),
          // Final score: 1300
          CreateMlScoredMatch("document 1200 0.75", type, false, 1200, 0.75),
      }),
      testing::ElementsAreArray({
          "document 1200 0.75",
          "document 1400 0.25",
      }));

  scoped_ml_config.GetMLConfig().enable_ml_scoring_for_searches = true;
  // Calculator and Answer suggestions should not be ML scored at this time,
  // since the ML model doesn't assign accurate scores to such suggestions
  // (due to the fact that they have a low click-through rate).
  std::string answer_json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text\", \"tt\": 8 }] } }, "
      "  { \"il\": { \"t\": [{ \"t\": \"sunny with a chance of hail\", "
      "\"tt\": "
      "5 }] } }] }";
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          // Final score: 1100 (!= 1300)
          CreateAnswerMlScoredMatch("answer 1100 0.75",
                                    omnibox::ANSWER_TYPE_WEATHER, answer_json,
                                    false, 1100, 0.75),
          // Final score: 1000 (!= 1500)
          CreateMlScoredMatch("calculator 1000 0.95",
                              AutocompleteMatchType::CALCULATOR, false, 1000,
                              1),
          // Final score: 1431
          CreateHistoryUrlMlScoredMatch("history 500 0.914", true, 500, 0.914),
      }),
      testing::ElementsAreArray({
          "history 500 0.914",
          "answer 1100 0.75",
          "calculator 1000 0.95",
      }));
  scoped_ml_config.GetMLConfig().enable_ml_scoring_for_searches = false;

  // Simple case of ranking with piecewise score mapping. The ML
  // scores used here are the same as those specified in the
  // `MlRanking_ApplyPiecewiseScoringTransform` test for the sake of simplicity.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          // Final score: 1133
          CreateHistoryUrlMlScoredMatch("history 1350 .473", true, 1350, .473),
          // Final score: 1431
          CreateHistoryUrlMlScoredMatch("history 1200 .914", true, 1200, .914),
          // Final score: 872
          CreateHistoryUrlMlScoredMatch("history 1100 .186", false, 1100, .186),
          // Final score: 1000
          CreateHistoryUrlMlScoredMatch("history 500 .25", true, 500, .25),
      }),
      testing::ElementsAreArray({
          "history 1200 .914",
          "history 1350 .473",
          "history 500 .25",
          "history 1100 .186",
      }));

  scoped_refptr<FakeAutocompleteProvider> shortcut_provider =
      new FakeAutocompleteProvider(AutocompleteProvider::Type::TYPE_SHORTCUTS);

  auto shortcut_match = CreateMlScoredMatch(
      "shortcut 600 0.75", AutocompleteMatchType::HISTORY_URL, true, 600, 0.75);
  shortcut_match.provider = shortcut_provider.get();

  // Non-boosted shortcut suggestions should be ranked BELOW searches.
  shortcut_match.scoring_signals->set_visit_count(0);
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          // Final score: 1431
          CreateHistoryUrlMlScoredMatch("history 1200 .914", true, 1200, .914),
          // Final score: 700
          CreateSearchMatch("search 700", true, 700),
          // Final score: 1300
          shortcut_match,
      }),
      testing::ElementsAreArray({
          "history 1200 .914",
          "search 700",
          "shortcut 600 0.75",
      }));

  // Boosted shortcut suggestions should be ranked ABOVE searches.
  shortcut_match.scoring_signals->set_visit_count(5);
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          // Final score: 1431
          CreateHistoryUrlMlScoredMatch("history 1200 .914", true, 1200, .914),
          // Final score: 700
          CreateSearchMatch("search 700", true, 700),
          // Final score: 1300
          shortcut_match,
      }),
      testing::ElementsAreArray({
          "history 1200 .914",
          "shortcut 600 0.75",
          "search 700",
      }));

  // ...unless their final relevance score (obtained via piecewise ML scoring)
  // is below the "grouping threshold".
  shortcut_match = CreateMlScoredMatch(
      "shortcut 600 0.25", AutocompleteMatchType::HISTORY_URL, true, 600, 0.25);
  shortcut_match.provider = shortcut_provider.get();
  shortcut_match.scoring_signals->set_visit_count(5);
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          // Final score: 1431
          CreateHistoryUrlMlScoredMatch("history 1200 .914", true, 1200, .914),
          // Final score: 700
          CreateSearchMatch("search 700", true, 700),
          // Final score: 1000
          shortcut_match,
      }),
      testing::ElementsAreArray({
          "history 1200 .914",
          "search 700",
          "shortcut 600 0.25",
      }));

  // In general, URL suggestions are NOT "shortcut boosted" above searches even
  // when they're scored higher via ML scoring.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          // Final score: 1431
          CreateHistoryUrlMlScoredMatch("history 1200 .914", true, 1200, .914),
          // Final score: 700
          CreateSearchMatch("search 700", true, 700),
          // Final score: 1300
          CreateHistoryUrlMlScoredMatch("history 1100 .75", true, 1100, .75),
      }),
      testing::ElementsAreArray({
          "history 1200 .914",
          "search 700",
          "history 1100 .75",
      }));

  // When multiple URL suggestions have been assigned the same score by the ML
  // model, those suggestions which were top-ranked according to legacy scoring
  // should continue to be top-ranked once ML scoring has run.
  EXPECT_THAT(
      // Each of the below URL suggestions are assigned an initial relevance
      // score of 1300. After initial assignment,
      // score adjustment logic is applied in order to generate the final
      // relevance scores (which are guaranteed to be distinct).
      controller_.SimulateCleanAutocompletePass({
          // Final score: 1299
          CreateHistoryUrlMlScoredMatch("history B 1200 .75", true, 1200, .75),
          // Final score: 1296
          CreateHistoryUrlMlScoredMatch("history E 200 .75", true, 200, .75),
          // Final score: 1300
          CreateHistoryUrlMlScoredMatch("history A 1350 .75", true, 1350, .75),
          // Final score: 1297
          CreateHistoryUrlMlScoredMatch("history D 300 .75", true, 300, .75),
          // Final score: 1298
          CreateHistoryUrlMlScoredMatch("history C 1100 .75", false, 1100, .75),
          // Final score: 1295
          CreateHistoryUrlMlScoredMatch("history F 100 .75", true, 100, .75),
      }),
      testing::ElementsAreArray({
          "history A 1350 .75",
          "history B 1200 .75",
          "history C 1100 .75",
          "history D 300 .75",
          "history E 200 .75",
          "history F 100 .75",
      }));

  // Can change the default suggestion from 1 history to another.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          // Final score: 1000
          CreateHistoryUrlMlScoredMatch("history 1400 .25", true, 1400, .25),
          CreateSearchMatch("search", true, 1100),
          // Final score: 1300
          CreateHistoryUrlMlScoredMatch("history 1200 .75", true, 1200, .75),
      }),
      testing::ElementsAreArray({
          "history 1200 .75",
          "search",
          "history 1400 .25",
      }));

  // Can change the default from search to history.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateSearchMatch("search 1200", true, 1200),
          // Final score: 1000
          CreateHistoryUrlMlScoredMatch("history 1400 .25", false, 1400, .25),
          // Final score: 1300
          CreateHistoryUrlMlScoredMatch("history 1100 .75", true, 1100, .75),
      }),
      testing::ElementsAreArray({
          "history 1100 .75",
          "search 1200",
          "history 1400 .25",
      }));

  // Can change the default from history to search.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          // Final score: 1000
          CreateHistoryUrlMlScoredMatch("history 1400 .25", true, 1400, .25),
          CreateSearchMatch("search 1300", true, 1300),
          // Final score: 872
          CreateHistoryUrlMlScoredMatch("history 1200 .186", false, 1200, .186),
      }),
      testing::ElementsAreArray({
          "search 1300",
          "history 1400 .25",
          "history 1200 .186",
      }));

  // When transferring matches, culls the lowest ML ranked matches, rather than
  // the lowest traditional ranked matches.
  controller_.internal_result_.Reset();
  EXPECT_THAT(
      controller_.SimulateAutocompletePass(
          true, false,
          {
              CreateSearchMatch("search 1270", true, 1270),
              CreateSearchMatch("search 1260", true, 1260),
              CreateSearchMatch("search 1250", true, 1250),
              CreateSearchMatch("search 1240", true, 1240),
              CreateSearchMatch("search 1230", true, 1230),
              CreateSearchMatch("search 1220", true, 1220),
              CreateSearchMatch("search 1210", true, 1210),
              CreateHistoryUrlMlScoredMatch("history 1100 .1", true, 1100, .1),
              CreateHistoryUrlMlScoredMatch("history 1000 .2", true, 1000, .2),
          }),
      testing::ElementsAreArray({
          "search 1270",
          "search 1260",
          "search 1250",
          "search 1240",
          "search 1230",
          "search 1220",
          "search 1210",
          "history 1000 .2",
      }));

  // When not transferring matches, like above, culls the lowest ML ranked
  // matches, rather than the lowest traditional ranked matches.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateSearchMatch("search 1270", true, 1270),
          CreateSearchMatch("search 1260", true, 1260),
          CreateSearchMatch("search 1250", true, 1250),
          CreateSearchMatch("search 1240", true, 1240),
          CreateSearchMatch("search 1230", true, 1230),
          CreateSearchMatch("search 1220", true, 1220),
          CreateSearchMatch("search 1210", true, 1210),
          CreateHistoryUrlMlScoredMatch("history 1100 .1", true, 1100, .1),
          CreateHistoryUrlMlScoredMatch("history 1000 .2", true, 1000, .2),
      }),
      testing::ElementsAreArray({
          "search 1270",
          "search 1260",
          "search 1250",
          "search 1240",
          "search 1230",
          "search 1220",
          "search 1210",
          "history 1000 .2",
      }));
}

TEST_F(AutocompleteControllerTest, MlRanking_MappedSearchBlending) {
  OmniboxFieldTrial::ScopedMLConfigForTesting scoped_ml_config;
  scoped_ml_config.GetMLConfig().ml_url_scoring = true;
  scoped_ml_config.GetMLConfig().url_scoring_model = true;
  scoped_ml_config.GetMLConfig().mapped_search_blending = true;

  scoped_ml_config.GetMLConfig().mapped_search_blending_min = 600;
  scoped_ml_config.GetMLConfig().mapped_search_blending_max = 2800;
  scoped_ml_config.GetMLConfig().mapped_search_blending_grouping_threshold =
      1400;

  EXPECT_THAT(controller_.SimulateCleanAutocompletePass({}),
              testing::ElementsAre());

  // If ML ranks a URL 0, then the final relevance score should be set to the
  // value of `mapped_search_blending_min` (since ML scores are mapped using the
  // formula "final_score = min + ml_score * (max - min))".
  EXPECT_THAT(controller_.SimulateCleanAutocompletePass({
                  CreateHistoryUrlMlScoredMatch("history", true, 1400, 0),
                  CreateSearchMatch("search", true, 1300),
              }),
              testing::ElementsAreArray({
                  "search",
                  "history",
              }));

  // If a (remote) document suggestion has a traditional score of zero, then the
  // final relevance score should remain zero (instead of using the formula
  // "final_score = min + ml_score * (max - min)" to overwrite the score). This
  // will result in the document suggestion getting culled from the final list
  // of suggestions.
  const auto type = AutocompleteMatchType::DOCUMENT_SUGGESTION;
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          // Final score: 1150 (== 600 + 0.25 * (2800 - 600))
          CreateMlScoredMatch("document 1400 0.25", type, false, 1400, 0.25),
          // Final score: 0 (!= 600 + 0.95 * (2800 - 600))
          CreateMlScoredMatch("document 0 0.95", type, false, 0, 0.95),
          // Final score: 2250 (== 600 + 0.75 * (2800 - 600))
          CreateMlScoredMatch("document 1200 0.75", type, false, 1200, 0.75),
      }),
      testing::ElementsAreArray({
          "document 1200 0.75",
          "document 1400 0.25",
      }));

  // Simple case of ranking with linear score mapping.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          // Final score: 1700 (== 600 + 0.5 * (2800 - 600))
          CreateHistoryUrlMlScoredMatch("history 1350 .5", true, 1350, .5),
          // Final score: 2580 (== 600 + 0.9 * (2800 - 600))
          CreateHistoryUrlMlScoredMatch("history 1200 .9", true, 1200, .9),
          // Final score: 820 (== 600 + 0.1 * (2800 - 600))
          CreateHistoryUrlMlScoredMatch("history 1100 .1", false, 1100, .1),
          // Final score: 1040 (== 600 + 0.2 * (2800 - 600))
          CreateHistoryUrlMlScoredMatch("history 500 .2", true, 500, .2),
      }),
      testing::ElementsAreArray({
          "history 1200 .9",
          "history 1350 .5",
          "history 500 .2",
          "history 1100 .1",
      }));

  // Verify that URLs are grouped above searches if their final score is
  // greater than `grouping_threshold` (i.e. "shortcut boosting").
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          // Final score: 1700 (== 600 + 0.5 * (2800 - 600))
          CreateHistoryUrlMlScoredMatch("history 1350 .5", true, 1350, .5),
          CreateSearchMatch("search 1400", false, 1400),
          CreateSearchMatch("search 800", true, 800),
          CreateSearchMatch("search 600", false, 600),
          // Final score: 2580 (== 600 + 0.9 * (2800 - 600))
          CreateHistoryUrlMlScoredMatch("history 1200 .9", true, 1200, .9),
          // Final score: 820 (== 600 + 0.1 * (2800 - 600))
          CreateHistoryUrlMlScoredMatch("history 1100 .1", false, 1100, .1),
          // Final score: 1040 (== 600 + 0.2 * (2800 - 600))
          CreateHistoryUrlMlScoredMatch("history 500 .2", true, 500, .2),
      }),
      testing::ElementsAreArray({
          "history 1200 .9",
          "history 1350 .5",
          "search 1400",
          "search 800",
          "search 600",
          "history 500 .2",
          "history 1100 .1",
      }));

  // When multiple URL suggestions have been assigned the same score by the ML
  // model, those suggestions which were top-ranked according to legacy scoring
  // should continue to be top-ranked once ML scoring has run.
  EXPECT_THAT(
      // Each of the below URL suggestions are assigned an initial relevance
      // score of 1040 (== 600 + 0.2 * (2800 - 600)). After initial assignment,
      // score adjustment logic is applied in order to generate the final
      // relevance scores (which are guaranteed to be distinct).
      controller_.SimulateCleanAutocompletePass({
          // Final score: 1039
          CreateHistoryUrlMlScoredMatch("history B 1200 .2", true, 1200, .2),
          // Final score: 1036
          CreateHistoryUrlMlScoredMatch("history E 200 .2", true, 200, .2),
          // Final score: 1040
          CreateHistoryUrlMlScoredMatch("history A 1350 .2", true, 1350, .2),
          // Final score: 1037
          CreateHistoryUrlMlScoredMatch("history D 300 .2", true, 300, .2),
          // Final score: 1038
          CreateHistoryUrlMlScoredMatch("history C 1100 .2", false, 1100, .2),
          // Final score: 1035
          CreateHistoryUrlMlScoredMatch("history F 100 .2", true, 100, .2),
      }),
      testing::ElementsAreArray({
          "history A 1350 .2",
          "history B 1200 .2",
          "history C 1100 .2",
          "history D 300 .2",
          "history E 200 .2",
          "history F 100 .2",
      }));

  // Can change the default suggestion from 1 history to another.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          // Final score: 1040 (== 600 + 0.2 * (2800 - 600))
          CreateHistoryUrlMlScoredMatch("history 1400 .2", true, 1400, .2),
          CreateSearchMatch("search", true, 1100),
          // Final score: 1260 (== 600 + 0.3 * (2800 - 600))
          CreateHistoryUrlMlScoredMatch("history 1200 .3", true, 1200, .3),
      }),
      testing::ElementsAreArray({
          "history 1200 .3",
          "search",
          "history 1400 .2",
      }));

  // Can change the default from search to history.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateSearchMatch("search 1200", true, 1200),
          // Final score: 1040 (== 600 + 0.2 * (2800 - 600))
          CreateHistoryUrlMlScoredMatch("history 1400 .2", false, 1400, .2),
          // Final score: 1260 (== 600 + 0.3 * (2800 - 600))
          CreateHistoryUrlMlScoredMatch("history 1100 .3", true, 1100, .3),
      }),
      testing::ElementsAreArray({
          "history 1100 .3",
          "search 1200",
          "history 1400 .2",
      }));

  // Can change the default from history to search.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          // Final score: 1040 (== 600 + 0.2 * (2800 - 600))
          CreateHistoryUrlMlScoredMatch("history 1400 .2", true, 1400, .2),
          CreateSearchMatch("search 1300", true, 1300),
          // Final score: 820 (== 600 + 0.1 * (2800 - 600))
          CreateHistoryUrlMlScoredMatch("history 1200 .1", false, 1200, .1),
      }),
      testing::ElementsAreArray({
          "search 1300",
          "history 1400 .2",
          "history 1200 .1",
      }));

  // When transferring matches, culls the lowest ML ranked matches, rather than
  // the lowest traditional ranked matches.
  controller_.internal_result_.Reset();
  EXPECT_THAT(
      controller_.SimulateAutocompletePass(
          true, false,
          {
              CreateSearchMatch("search 1270", true, 1270),
              CreateSearchMatch("search 1260", true, 1260),
              CreateSearchMatch("search 1250", true, 1250),
              CreateSearchMatch("search 1240", true, 1240),
              CreateSearchMatch("search 1230", true, 1230),
              CreateSearchMatch("search 1220", true, 1220),
              CreateSearchMatch("search 1210", true, 1210),
              CreateHistoryUrlMlScoredMatch("history 1100 .1", true, 1100, .1),
              CreateHistoryUrlMlScoredMatch("history 1000 .2", true, 1000, .2),
          }),
      testing::ElementsAreArray({
          "search 1270",
          "search 1260",
          "search 1250",
          "search 1240",
          "search 1230",
          "search 1220",
          "search 1210",
          "history 1000 .2",
      }));

  // When not transferring matches, like above, culls the lowest ML ranked
  // matches, rather than the lowest traditional ranked matches.
  EXPECT_THAT(
      controller_.SimulateCleanAutocompletePass({
          CreateSearchMatch("search 1270", true, 1270),
          CreateSearchMatch("search 1260", true, 1260),
          CreateSearchMatch("search 1250", true, 1250),
          CreateSearchMatch("search 1240", true, 1240),
          CreateSearchMatch("search 1230", true, 1230),
          CreateSearchMatch("search 1220", true, 1220),
          CreateSearchMatch("search 1210", true, 1210),
          CreateHistoryUrlMlScoredMatch("history 1100 .1", true, 1100, .1),
          CreateHistoryUrlMlScoredMatch("history 1000 .2", true, 1000, .2),
      }),
      testing::ElementsAreArray({
          "search 1270",
          "search 1260",
          "search 1250",
          "search 1240",
          "search 1230",
          "search 1220",
          "search 1210",
          "history 1000 .2",
      }));
}

TEST_F(AutocompleteControllerTest, UpdateResult_MLRanking_PreserveDefault) {
  OmniboxFieldTrial::ScopedMLConfigForTesting scoped_ml_config;
  scoped_ml_config.GetMLConfig().ml_url_scoring = true;
  scoped_ml_config.GetMLConfig().url_scoring_model = true;
  scoped_ml_config.GetMLConfig().mapped_search_blending = true;

  scoped_ml_config.GetMLConfig().mapped_search_blending_min = 600;
  scoped_ml_config.GetMLConfig().mapped_search_blending_max = 2800;
  scoped_ml_config.GetMLConfig().mapped_search_blending_grouping_threshold =
      1400;

  // ML ranking should preserve search defaults.
  // In other words, if the autocomplete controller starts off by listing
  // "search 1300" as the (top-ranked) default suggestion, then "search 1300"
  // should remain the default suggestion (even though "history 1400" has been
  // added to the list of suggestions).
  EXPECT_THAT(controller_.SimulateAutocompletePass(
                  true, true,
                  {
                      CreateSearchMatch("search 1300", true, 1300),
                  }),
              testing::ElementsAreArray({
                  "search 1300",
              }));
  EXPECT_THAT(
      controller_.SimulateAutocompletePass(
          true, true,
          {
              CreateHistoryUrlMlScoredMatch("history 1400", true, 1400, 1),
              CreateSearchMatch("search 1300", true, 1300),
          }),
      testing::ElementsAreArray({
          "search 1300",
          "history 1400",
      }));

  // ML ranking should preserve non-search defaults.
  // In other words, if the autocomplete controller starts off by listing
  // "history 1300" as the (top-ranked) default suggestion, then "history 1300"
  // should remain the default suggestion (even though "history 1500" and
  // "search 1400" have been added to the list of suggestions).
  EXPECT_THAT(
      controller_.SimulateAutocompletePass(
          true, true,
          {
              CreateHistoryUrlMlScoredMatch("history 1300", true, 1300, 1),
          }),
      testing::ElementsAreArray({
          "history 1300",
      }));
  EXPECT_THAT(
      controller_.SimulateAutocompletePass(
          true, true,
          {
              CreateHistoryUrlMlScoredMatch("history 1500", true, 1500, 1),
              CreateSearchMatch("search 1400", true, 1400),
              CreateHistoryUrlMlScoredMatch("history 1300", true, 1300, .1),
          }),
      testing::ElementsAreArray({
          "history 1300",
          "history 1500",
          "search 1400",
      }));
}

TEST_F(AutocompleteControllerTest, UpdateResult_MLRanking_AllMatches) {
  OmniboxFieldTrial::ScopedMLConfigForTesting scoped_ml_config;
  scoped_ml_config.GetMLConfig().ml_url_scoring = true;
  scoped_ml_config.GetMLConfig().url_scoring_model = true;
  scoped_ml_config.GetMLConfig().mapped_search_blending = true;

  scoped_ml_config.GetMLConfig().mapped_search_blending_min = 600;
  scoped_ml_config.GetMLConfig().mapped_search_blending_max = 2800;
  scoped_ml_config.GetMLConfig().mapped_search_blending_grouping_threshold =
      1400;

  EXPECT_THAT(
      controller_.SimulateAutocompletePass(
          true, false,
          {
              CreateHistoryUrlMlScoredMatch("history 100", true, 100, 1),
              CreateHistoryUrlMlScoredMatch("history 200", true, 200, 1),
              CreateHistoryUrlMlScoredMatch("history 300", true, 300, 1),
              CreateHistoryUrlMlScoredMatch("history 400", true, 400, 1),
              CreateHistoryUrlMlScoredMatch("history 500", true, 500, 1),
              CreateHistoryUrlMlScoredMatch("history 600", true, 600, 1),
              CreateHistoryUrlMlScoredMatch("history 700", true, 700, 1),
              CreateHistoryUrlMlScoredMatch("history 800", true, 800, 1),
              CreateHistoryUrlMlScoredMatch("history 900", true, 900, 1),
              CreateHistoryUrlMlScoredMatch("history 1000", true, 1000, 1),
          }),
      testing::ElementsAreArray({
          "history 1000",
          "history 900",
          "history 800",
          "history 700",
          "history 600",
          "history 500",
          "history 400",
          "history 300",
      }));

  EXPECT_THAT(
      controller_.SimulateAutocompletePass(
          true, false,
          {
              CreateHistoryUrlMlScoredMatch("history 100 .9", true, 100, .9),
              CreateHistoryUrlMlScoredMatch("history 200 .8", true, 200, .8),
              CreateHistoryUrlMlScoredMatch("history 300 .7", true, 300, .7),
              CreateHistoryUrlMlScoredMatch("history 400 .6", true, 400, .6),
              CreateHistoryUrlMlScoredMatch("history 500 .5", true, 500, .5),
              CreateHistoryUrlMlScoredMatch("history 600 .4", true, 600, .4),
              CreateHistoryUrlMlScoredMatch("history 700 .3", true, 700, .3),
              CreateHistoryUrlMlScoredMatch("history 800 .2", true, 800, .2),
              CreateHistoryUrlMlScoredMatch("history 900 .1", true, 900, .1),
              CreateHistoryUrlMlScoredMatch("history 1000 0", true, 1000, 0),
          }),
      testing::ElementsAreArray({
          "history 100 .9",
          "history 200 .8",
          "history 300 .7",
          "history 400 .6",
          "history 500 .5",
          "history 600 .4",
          "history 700 .3",
          "history 800 .2",
      }));
}
#endif  //  BUILDFLAG(BUILD_WITH_TFLITE_LIB) && !BUILDFLAG(IS_ANDROID) &&
        //  !BUILDFLAG(IS_IOS)

TEST_F(AutocompleteControllerTest, UpdateResult_NotifyingAndTimers) {
  {
    SCOPED_TRACE("Expect immediate notification after sync pass.");
    controller_.GetFakeProvider().done_ = false;
    controller_.Start(FakeAutocompleteController::CreateInput(u"test"));
    controller_.ExpectOnResultChanged(
        0, AutocompleteController::UpdateType::kSyncPass);
  }
  {
    SCOPED_TRACE("Expect debounced notification after async pass.");
    controller_.GetFakeProvider().done_ = false;
    controller_.OnProviderUpdate(true, &controller_.GetFakeProvider());
    controller_.ExpectOnResultChanged(
        200, AutocompleteController::UpdateType::kAsyncPass);
  }
  {
    SCOPED_TRACE("Expect debouncing to reset after each async passes.");
    controller_.GetFakeProvider().done_ = false;
    controller_.OnProviderUpdate(true, &controller_.GetFakeProvider());
    task_environment_.FastForwardBy(base::Milliseconds(150));
    controller_.OnProviderUpdate(true, &controller_.GetFakeProvider());
    task_environment_.FastForwardBy(base::Milliseconds(150));
    controller_.OnProviderUpdate(true, &controller_.GetFakeProvider());
    controller_.ExpectOnResultChanged(
        200, AutocompleteController::UpdateType::kAsyncPass);
  }
  {
    SCOPED_TRACE("Expect delayed notification after expiration.");
    controller_.UpdateResult(AutocompleteController::UpdateType::kExpirePass);
    controller_.ExpectOnResultChanged(
        200, AutocompleteController::UpdateType::kExpirePass);
  }
  {
    SCOPED_TRACE("Expect immediate notification after the last async pass.");
    controller_.GetFakeProvider().done_ = true;
    controller_.OnProviderUpdate(true, &controller_.GetFakeProvider());
    controller_.ExpectOnResultChanged(
        0, AutocompleteController::UpdateType::kLastAsyncPass);
  }
  {
    SCOPED_TRACE("Expect no stop update after the last async pass.");
    controller_.ExpectNoNotificationOrStop();
  }
  {
    SCOPED_TRACE(
        "Expect immediate notification after the last async pass, even if a "
        "debounced notification is pending.");
    controller_.GetFakeProvider().done_ = false;
    controller_.Start(FakeAutocompleteController::CreateInput(u"test"));
    controller_.ExpectOnResultChanged(
        0, AutocompleteController::UpdateType::kSyncPass);

    controller_.OnProviderUpdate(true, &controller_.GetFakeProvider());

    controller_.GetFakeProvider().done_ = true;
    task_environment_.FastForwardBy(base::Milliseconds(10));
    controller_.OnProviderUpdate(true, &controller_.GetFakeProvider());
    controller_.ExpectOnResultChanged(
        0, AutocompleteController::UpdateType::kLastAsyncPass);
  }
  {
    SCOPED_TRACE("Expect no stop update after the last async pass (2).");
    controller_.ExpectNoNotificationOrStop();
  }
  {
    SCOPED_TRACE("Expect no stop update after a sync only pass.");
    controller_.GetFakeProvider().done_ = true;
    controller_.Start(FakeAutocompleteController::CreateInput(u"test"));
    controller_.ExpectOnResultChanged(
        0, AutocompleteController::UpdateType::kSyncPassOnly);
    controller_.ExpectNoNotificationOrStop();
  }
  {
    SCOPED_TRACE(
        "Expect a stop update if the async passes takes too long. Expect no "
        "notification.");
    controller_.GetFakeProvider().done_ = false;
    controller_.Start(FakeAutocompleteController::CreateInput(u"test"));
    controller_.ExpectOnResultChanged(
        0, AutocompleteController::UpdateType::kSyncPass);
    controller_.ExpectStopAfter(1500);
  }
  {
    SCOPED_TRACE(
        "Expect a stop update to flush any pending notification for completed "
        "non-final async passes.");
    controller_.GetFakeProvider().done_ = false;
    controller_.Start(FakeAutocompleteController::CreateInput(u"test"));
    controller_.ExpectOnResultChanged(
        0, AutocompleteController::UpdateType::kSyncPass);
    for (size_t i = 0; i < 9; ++i) {
      task_environment_.FastForwardBy(base::Milliseconds(150));
      controller_.OnProviderUpdate(true, &controller_.GetFakeProvider());
    }
    controller_.ExpectStopAfter(150);
    controller_.ExpectNoNotificationOrStop();
  }
  {
    SCOPED_TRACE("Expect debounced expire notification.");
    controller_.GetFakeProvider().done_ = false;
    AutocompleteMatch transferred_match{
        nullptr, 1000, false, AutocompleteMatchType::URL_WHAT_YOU_TYPED};
    transferred_match.from_previous = true;
    controller_.GetFakeProvider().matches_ = {transferred_match};
    controller_.Start(FakeAutocompleteController::CreateInput(u"test"));
    controller_.ExpectOnResultChanged(
        0, AutocompleteController::UpdateType::kSyncPass);
    // Expire timer is 500ms. Debounce delay is 200ms.
    controller_.ExpectOnResultChanged(
        700, AutocompleteController::UpdateType::kExpirePass);
    controller_.ExpectStopAfter(800);
    controller_.ExpectNoNotificationOrStop();
  }
}

TEST_F(AutocompleteControllerTest, ExplicitStop) {
  // Besides the `Stop()` fired by the timer, which is tested in
  // `UpdateResult_NotifyingAndTimers`, there's also user triggered `Stop()`s
  // tests here.

  auto matches = {CreateSearchMatch("search", true, 900)};

  {
    SCOPED_TRACE(
        "Stop with `kInteraction` and no pending changes should not notify "
        "`OnResultChanged()` - there's no change to notify of.");
    controller_.SimulateAutocompletePass(true, false, matches);
    controller_.Stop(AutocompleteStopReason::kInteraction);
    controller_.ExpectStopAfter(0, true);
    EXPECT_FALSE(controller_.published_result_.empty());
    controller_.ExpectNoNotificationOrStop();
  }
  {
    SCOPED_TRACE(
        "Stop with `kInteraction` and pending changes should not notify "
        "`OnResultChanged()` - the last pending change should be "
        "abandoned to avoid changes as the user's e.g. down arrowing.");
    controller_.SimulateAutocompletePass(true, false, matches);
    controller_.SimulateAutocompletePass(false, false, matches);
    controller_.Stop(AutocompleteStopReason::kInteraction);
    EXPECT_FALSE(controller_.published_result_.empty());
    controller_.ExpectStopAfter(0, true);
    controller_.ExpectNoNotificationOrStop();
  }
  {
    SCOPED_TRACE(
        "Stop with `kClobbered` and no pending notifications should notify "
        "`OnResultChanged()` - observers should know the results were "
        "cleared.");
    controller_.SimulateAutocompletePass(true, false, matches);
    controller_.observer_->last_default_match_changed = true;
    controller_.Stop(AutocompleteStopReason::kClobbered);
    EXPECT_TRUE(controller_.published_result_.empty());
    controller_.ExpectOnResultChanged(
        0, AutocompleteController::UpdateType::kStop);
    EXPECT_FALSE(controller_.observer_->last_default_match_changed);
    controller_.ExpectNoNotificationOrStop();
  }
  {
    SCOPED_TRACE(
        "Stop with `kClobbered` and pending notifications should notify "
        "`OnResultChanged()` - observers should know the results were cleared."
        "cleared.");
    controller_.SimulateAutocompletePass(true, false, matches);
    controller_.SimulateAutocompletePass(false, false, matches);
    controller_.observer_->last_default_match_changed = true;
    controller_.Stop(AutocompleteStopReason::kClobbered);
    EXPECT_TRUE(controller_.published_result_.empty());
    controller_.ExpectOnResultChanged(
        0, AutocompleteController::UpdateType::kStop);
    EXPECT_FALSE(controller_.observer_->last_default_match_changed);
    controller_.ExpectNoNotificationOrStop();
  }
}

TEST_F(AutocompleteControllerTest, UpdateResult_ForceAllowedToBeDefault) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(omnibox::kGroupingFrameworkForNonZPS);

  auto set_feature = [](bool enabled) {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatureState(
        omnibox_feature_configs::ForceAllowedToBeDefault::
            kForceAllowedToBeDefault,
        enabled);
    return omnibox_feature_configs::ScopedConfigForTesting<
        omnibox_feature_configs::ForceAllowedToBeDefault>();
  };

  {
    // When disabled, a not-defaultable history match should not be default.
    SCOPED_TRACE("Disabled");
    auto disabled_config = set_feature(false);
    EXPECT_THAT(controller_.SimulateCleanAutocompletePass({
                    CreateSearchMatch("search", true, 200),
                    CreateHistoryUrlMlScoredMatch("history", false, 1400, .5),
                }),
                testing::ElementsAreArray({
                    "search",
                    "history",
                }));
  }
  {
    // An initially not-defaultable history match can be made defaultable.
    SCOPED_TRACE("Enabled");
    auto enabled_config = set_feature(true);
    EXPECT_THAT(controller_.SimulateCleanAutocompletePass({
                    CreateSearchMatch("search", true, 200),
                    CreateHistoryUrlMlScoredMatch("history", false, 1400, .5),
                }),
                testing::ElementsAreArray({
                    "history",
                    "search",
                }));
  }
  {
    // Initially defaultable matches should not be made non-defaultable even if
    // they don't qualify for forcing defaultable.
    SCOPED_TRACE("Enabled defaultable");
    auto enabled_config = set_feature(true);
    EXPECT_THAT(controller_.SimulateCleanAutocompletePass({
                    CreateSearchMatch("search", true, 200),
                    CreateHistoryUrlMlScoredMatch("history", true, 300, .5),
                }),
                testing::ElementsAreArray({
                    "history",
                    "search",
                }));
  }
  {
    // Keyword matches shouldn't be made defaultable.
    SCOPED_TRACE("Enabled keyword");
    auto enabled_config = set_feature(true);
    EXPECT_THAT(controller_.SimulateCleanAutocompletePass({
                    CreateSearchMatch("search", true, 200),
                    CreateKeywordHintMatch("keyword", 1000),
                }),
                testing::ElementsAreArray({
                    "search",
                    "keyword",
                }));
  }
  {
    // Should not force default when `prevent_inline_autocomplete_` is true.
    SCOPED_TRACE("Enabled prevent inline autocomplete");
    auto enabled_config = set_feature(true);
    controller_.internal_result_.ClearMatches();
    EXPECT_THAT(
        controller_.SimulateAutocompletePass(
            true, true,
            {
                CreateSearchMatch("search", true, 200),
                CreateAutocompleteMatch("history",
                                        AutocompleteMatchType::HISTORY_CLUSTER,
                                        false, false, 1000, std::nullopt),
            },
            FakeAutocompleteController::CreateInput(u"test", false, true)),
        testing::ElementsAreArray({
            "search",
            "history",
        }));
  }
}

// Feature not enabled on Android and iOS.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(AutocompleteControllerTest, UpdateResult_ContextualSuggestionsAndLens) {
  // Enable contextual suggestions.
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::ContextualSearch>
      contextual_search_config;
  contextual_search_config.Get().contextual_zps_limit = 3;
  contextual_search_config.Get().show_open_lens_action = true;
  contextual_search_config.Get().use_apc_paywall_signal = true;
  contextual_search_config.Get().show_suggestions_on_no_apc = true;

  // Populate TemplateURLService with a keyword.
  TemplateURLData turl_data;
  turl_data.SetShortName(u"Keyword");
  turl_data.SetKeyword(u"keyword");
  turl_data.SetURL("https://google.com/search?q={searchTerms}");
  controller_.template_url_service_->Add(
      std::make_unique<TemplateURL>(turl_data));

  // Create a zero-suggest input.
  AutocompleteInput zps_input(u"", 0u, metrics::OmniboxEventProto::OTHER,
                              TestSchemeClassifier());
  zps_input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);

  std::vector<AutocompleteMatch> provider_matches = {
      CreatePersonalizedZeroPrefixMatch("zps_base", 1450),
      CreateContextualSearchMatch(u"zps_contextual 1"),
      CreateContextualSearchMatch(u"zps_contextual 2"),
      CreateLensActionMatch(u"lens")};

  // Helper to check results
  auto check_results = [&](bool expect_contextual, bool expect_lens) {
    bool actual_contextual = false;
    bool actual_lens = false;
    for (const auto& match : controller_.published_result_) {
      if (match.subtypes.count(omnibox::SUBTYPE_CONTEXTUAL_SEARCH)) {
        actual_contextual = true;
      }
      if (match.takeover_action &&
          match.takeover_action->ActionId() ==
              OmniboxActionId::CONTEXTUAL_SEARCH_OPEN_LENS) {
        actual_lens = true;
      }
    }
    EXPECT_EQ(actual_contextual, expect_contextual);
    EXPECT_EQ(actual_lens, expect_lens);
  };

  // Lens is active. No contextual suggestions nor Lens entrypoint.
  {
    SCOPED_TRACE("Lens is active");
    EXPECT_CALL(*provider_client(), AreLensEntrypointsVisible())
        .WillRepeatedly(testing::Return(false));
    EXPECT_CALL(*provider_client(), IsPagePaywalled())
        .WillRepeatedly(testing::Return(false));

    controller_.SimulateAutocompletePass(/*sync=*/true, /*done=*/true,
                                         provider_matches, zps_input);
    check_results(/*expect_contextual=*/false, /*expect_lens=*/false);
  }

  // Lens is inactive. Contextual suggestions and Lens entrypoint.
  {
    SCOPED_TRACE("Lens is inactive");
    EXPECT_CALL(*provider_client(), AreLensEntrypointsVisible())
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(*provider_client(), IsPagePaywalled())
        .WillRepeatedly(testing::Return(false));

    controller_.SimulateAutocompletePass(/*sync=*/true, /*done=*/true,
                                         provider_matches, zps_input);
    check_results(/*expect_contextual=*/true, /*expect_lens=*/true);
  }

  // Page is paywalled. No contextual suggestions but has Lens entrypoint.
  {
    SCOPED_TRACE("Page is paywalled");
    EXPECT_CALL(*provider_client(), AreLensEntrypointsVisible())
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(*provider_client(), IsPagePaywalled())
        .WillRepeatedly(testing::Return(true));

    controller_.SimulateAutocompletePass(/*sync=*/true, /*done=*/true,
                                         provider_matches, zps_input);
    check_results(/*expect_contextual=*/false, /*expect_lens=*/true);
  }

  // Paywall is unknown. Contextual suggestions and Lens entrypoint.
  {
    SCOPED_TRACE("Paywall status is unknown");
    EXPECT_CALL(*provider_client(), AreLensEntrypointsVisible())
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(*provider_client(), IsPagePaywalled())
        .WillRepeatedly(testing::Return(std::nullopt));

    controller_.SimulateAutocompletePass(/*sync=*/true, /*done=*/true,
                                         provider_matches, zps_input);
    check_results(/*expect_contextual=*/true, /*expect_lens=*/true);
  }
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

TEST_F(AutocompleteControllerTest, ExtraHeaders) {
  // Populate TemplateURLService with a keyword.
  {
    TemplateURLData turl_data;
    turl_data.SetShortName(u"Keyword");
    turl_data.SetKeyword(u"keyword");
    turl_data.SetURL("https://google.com/search?q={searchTerms}");
    controller_.template_url_service_->Add(
        std::make_unique<TemplateURL>(turl_data));
  }

  // Populate template URL service with starter pack entries.
  for (auto& turl_data :
       template_url_starter_pack_data::GetStarterPackEngines()) {
    controller_.template_url_service_->Add(
        std::make_unique<TemplateURL>(std::move(*turl_data)));
  }

  const std::string expected_gemini_url =
      "https://gemini.google.com/prompt?"
      "utm_source=chrome_omnibox&utm_medium=owned&utm_campaign=gemini_shortcut";

  {
    SCOPED_TRACE("@gemini starter pack match gets an extra header.");
    auto match = CreateStarterPackMatch(u"@gemini");
    // search_terms_args need to have been set.
    match.search_terms_args =
        std::make_unique<TemplateURLRef::SearchTermsArgs>(u"search term");

    controller_.SetMatchDestinationURL(&match);
    EXPECT_THAT(
        match.extra_headers,
        WhenSorted(ElementsAre(Pair("X-Omnibox-Gemini", "search%20term"))));
    EXPECT_EQ(match.destination_url, expected_gemini_url);
  }
  {
    SCOPED_TRACE("@gemini starter pack match with url override");

    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        omnibox::kStarterPackExpansion,
        {{"StarterPackGeminiUrlOverride", "https://example.com/"}});

    auto match = CreateStarterPackMatch(u"@gemini");
    // search_terms_args need to have been set.
    match.search_terms_args =
        std::make_unique<TemplateURLRef::SearchTermsArgs>(u"search term?");

    controller_.SetMatchDestinationURL(&match);
    EXPECT_THAT(
        match.extra_headers,
        WhenSorted(ElementsAre(Pair("X-Omnibox-Gemini", "search%20term%3F"))));
    EXPECT_EQ(match.destination_url, "https://example.com/");
  }
  {
    SCOPED_TRACE("@gemini starter pack with invalid non-encoded input");
    auto match = CreateStarterPackMatch(u"@gemini");
    // search_terms_args need to have been set.
    match.search_terms_args =
        std::make_unique<TemplateURLRef::SearchTermsArgs>(u"search term\n");

    controller_.SetMatchDestinationURL(&match);
    EXPECT_THAT(
        match.extra_headers,
        WhenSorted(ElementsAre(Pair("X-Omnibox-Gemini", "search%20term%0A"))));
    EXPECT_EQ(match.destination_url, expected_gemini_url);
  }
  {
    SCOPED_TRACE("@gemini starter pack with url in the input");
    auto match = CreateStarterPackMatch(u"@gemini");
    // search_terms_args need to have been set.
    match.search_terms_args = std::make_unique<TemplateURLRef::SearchTermsArgs>(
        u"what is http://example.com for?");

    controller_.SetMatchDestinationURL(&match);
    EXPECT_THAT(match.extra_headers,
                WhenSorted(ElementsAre(
                    Pair("X-Omnibox-Gemini",
                         "what%20is%20http%3A%2F%2Fexample.com%20for%3F"))));
    EXPECT_EQ(match.destination_url, expected_gemini_url);
  }
  {
    SCOPED_TRACE("@gemini starter pack with non-ascii input");
    auto match = CreateStarterPackMatch(u"@gemini");
    // search_terms_args need to have been set.
    match.search_terms_args =
        std::make_unique<TemplateURLRef::SearchTermsArgs>(u"こんにちは\n");

    controller_.SetMatchDestinationURL(&match);
    EXPECT_THAT(match.extra_headers,
                WhenSorted(ElementsAre(
                    Pair("X-Omnibox-Gemini",
                         "%E3%81%93%E3%82%93%E3%81%AB%E3%81%A1%E3%81%AF%0A"))));
    EXPECT_EQ(match.destination_url, expected_gemini_url);
  }
  {
    SCOPED_TRACE("@bookmarks starter pack match does not get an extra header.");
    auto match = CreateStarterPackMatch(u"@bookmarks");
    // search_terms_args need to have been set.
    match.search_terms_args =
        std::make_unique<TemplateURLRef::SearchTermsArgs>(u"search term");

    controller_.SetMatchDestinationURL(&match);
    EXPECT_TRUE(match.extra_headers.empty());
    EXPECT_EQ(match.destination_url, "chrome://bookmarks/?q=search+term");
  }
  {
    SCOPED_TRACE("search match does not get an extra header.");
    auto match = CreateSearchMatch("search term", true, 1300);

    controller_.SetMatchDestinationURL(&match);
    EXPECT_TRUE(match.extra_headers.empty());
    EXPECT_EQ(match.destination_url, "https://google.com/search?q=search+term");
  }
}

TEST_F(AutocompleteControllerTest, ShouldRunProvider_StarterPack) {
  std::set<AutocompleteProvider::Type> expected_provider_types;
  AutocompleteInput input(u"a", 1u, metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  controller_.input_ = input;

  // Populate template URL service with starter pack entries.
  std::vector<std::unique_ptr<TemplateURLData>> turls =
      template_url_starter_pack_data::GetStarterPackEngines();
  for (auto& turl : turls) {
    controller_.template_url_service_->Add(
        std::make_unique<TemplateURL>(std::move(*turl)));
  }

  // Not in keyword mode, run all providers except open tab provider.
  for (auto& provider : controller_.providers()) {
    EXPECT_EQ(controller_.ShouldRunProvider(provider.get()),
              provider->type() != AutocompleteProvider::TYPE_OPEN_TAB)
        << "Provider Type: "
        << AutocompleteProvider::TypeToString(provider->type());
  }

  // Enter keyword mode.
  controller_.input_.set_keyword_mode_entry_method(
      metrics::OmniboxEventProto_KeywordModeEntryMethod_TAB);

  // In @tabs, run search, keyword, and open tab provider only.
  controller_.input_.UpdateText(u"@tabs", 0, {});
  expected_provider_types = {AutocompleteProvider::TYPE_KEYWORD,
                             AutocompleteProvider::TYPE_SEARCH,
                             AutocompleteProvider::TYPE_OPEN_TAB};
  for (auto& provider : controller_.providers()) {
    EXPECT_EQ(controller_.ShouldRunProvider(provider.get()),
              expected_provider_types.contains(provider->type()))
        << "Provider Type: "
        << AutocompleteProvider::TypeToString(provider->type());
  }

  // In @bookmarks, run search, keyword, and bookmarks only.
  controller_.input_.UpdateText(u"@bookmarks", 0, {});
  expected_provider_types = {AutocompleteProvider::TYPE_KEYWORD,
                             AutocompleteProvider::TYPE_SEARCH,
                             AutocompleteProvider::TYPE_BOOKMARK};
  for (auto& provider : controller_.providers()) {
    EXPECT_EQ(controller_.ShouldRunProvider(provider.get()),
              expected_provider_types.contains(provider->type()))
        << "Provider Type: "
        << AutocompleteProvider::TypeToString(provider->type());
  }

  // In @history, run search, keyword, and history providers only.
  controller_.input_.UpdateText(u"@history", 0, {});
  expected_provider_types = {AutocompleteProvider::TYPE_KEYWORD,
                             AutocompleteProvider::TYPE_SEARCH,
                             AutocompleteProvider::TYPE_HISTORY_QUICK,
                             AutocompleteProvider::TYPE_HISTORY_URL};
  for (auto& provider : controller_.providers()) {
    EXPECT_EQ(controller_.ShouldRunProvider(provider.get()),
              expected_provider_types.contains(provider->type()))
        << "Provider Type: "
        << AutocompleteProvider::TypeToString(provider->type());
  }
}

TEST_F(AutocompleteControllerTest,
       ShouldRunProvider_LimitKeywordModeSuggestions) {
  std::set<AutocompleteProvider::Type> excluded_provider_types;
  AutocompleteInput input(u"a", 1u, metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  controller_.input_ = input;

  // Populate template URL service with an entry for drive.google.com (to test
  // document provider) and a generic keyword entry.
  TemplateURLData drive_turl_data;
  drive_turl_data.SetShortName(u"Google Drive");
  drive_turl_data.SetKeyword(u"drive.google.com");
  drive_turl_data.SetURL("https://drive.google.com/search?q={searchTerms}");
  controller_.template_url_service_->Add(
      std::make_unique<TemplateURL>(drive_turl_data));
  TemplateURLData turl_data;
  turl_data.SetShortName(u"Test Keyword");
  turl_data.SetKeyword(u"keyword");
  turl_data.SetURL("https://google.com/search?q={searchTerms}");
  controller_.template_url_service_->Add(
      std::make_unique<TemplateURL>(turl_data));

  // Not in keyword mode, run all providers except open tab provider.
  excluded_provider_types = {AutocompleteProvider::TYPE_OPEN_TAB};
  for (auto& provider : controller_.providers()) {
    EXPECT_NE(controller_.ShouldRunProvider(provider.get()),
              excluded_provider_types.contains(provider->type()))
        << "Provider Type: "
        << AutocompleteProvider::TypeToString(provider->type());
  }

  // In keyword mode, all limit provider params on by default, limit document
  // and history cluster suggestions as well.
  controller_.input_.UpdateText(u"keyword", 0, {});
  controller_.input_.set_keyword_mode_entry_method(
      metrics::OmniboxEventProto_KeywordModeEntryMethod_TAB);
  excluded_provider_types = {
      AutocompleteProvider::TYPE_OPEN_TAB,
      AutocompleteProvider::TYPE_HISTORY_CLUSTER_PROVIDER,
      AutocompleteProvider::TYPE_DOCUMENT,
      AutocompleteProvider::TYPE_ON_DEVICE_HEAD};
  for (auto& provider : controller_.providers()) {
    EXPECT_NE(controller_.ShouldRunProvider(provider.get()),
              excluded_provider_types.contains(provider->type()))
        << "Provider Type: "
        << AutocompleteProvider::TypeToString(provider->type());
  }

  // For drive.google.com, run document provider.
  controller_.input_.UpdateText(u"drive.google.com", 0, {});
  excluded_provider_types = {
      AutocompleteProvider::TYPE_OPEN_TAB,
      AutocompleteProvider::TYPE_HISTORY_CLUSTER_PROVIDER,
      AutocompleteProvider::TYPE_ON_DEVICE_HEAD};
  for (auto& provider : controller_.providers()) {
    EXPECT_NE(controller_.ShouldRunProvider(provider.get()),
              excluded_provider_types.contains(provider->type()))
        << "Provider Type: "
        << AutocompleteProvider::TypeToString(provider->type());
  }
}

TEST_F(AutocompleteControllerTest, ShouldRunProvider_LensSearchbox) {
  // Run all providers except open tab provider.
  std::set<AutocompleteProvider::Type> excluded_provider_types = {
      AutocompleteProvider::TYPE_OPEN_TAB};
  controller_.input_ = AutocompleteInput(
      u"a", 1u, metrics::OmniboxEventProto::OTHER, TestSchemeClassifier());
  for (auto& provider : controller_.providers()) {
    EXPECT_NE(controller_.ShouldRunProvider(provider.get()),
              excluded_provider_types.contains(provider->type()))
        << "Provider Type: "
        << AutocompleteProvider::TypeToString(provider->type());
  }

  // For Lens searchboxes, run search provider only.
  std::set<AutocompleteProvider::Type> expected_provider_types = {
      AutocompleteProvider::TYPE_SEARCH};

  controller_.input_ = AutocompleteInput(
      u"a", 1u, metrics::OmniboxEventProto::CONTEXTUAL_SEARCHBOX,
      TestSchemeClassifier());
  for (auto& provider : controller_.providers()) {
    EXPECT_EQ(controller_.ShouldRunProvider(provider.get()),
              expected_provider_types.contains(provider->type()))
        << "Provider Type: "
        << AutocompleteProvider::TypeToString(provider->type());
  }

  controller_.input_ = AutocompleteInput(
      u"a", 1u, metrics::OmniboxEventProto::SEARCH_SIDE_PANEL_SEARCHBOX,
      TestSchemeClassifier());
  for (auto& provider : controller_.providers()) {
    EXPECT_EQ(controller_.ShouldRunProvider(provider.get()),
              expected_provider_types.contains(provider->type()))
        << "Provider Type: "
        << AutocompleteProvider::TypeToString(provider->type());
  }

  controller_.input_ = AutocompleteInput(
      u"a", 1u, metrics::OmniboxEventProto::LENS_SIDE_PANEL_SEARCHBOX,
      TestSchemeClassifier());
  for (auto& provider : controller_.providers()) {
    EXPECT_EQ(controller_.ShouldRunProvider(provider.get()),
              expected_provider_types.contains(provider->type()))
        << "Provider Type: "
        << AutocompleteProvider::TypeToString(provider->type());
  }
}

// The EnterpriseSearchAggregatorProvider is only run on desktop.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
TEST_F(AutocompleteControllerTest,
       ShouldRunProvider_EnterpriseSearchAggregator) {
  // Populate template URL service.
  auto add_template_url = [&](const std::string& name,
                              TemplateURLData::PolicyOrigin policy_origin,
                              bool featured) {
    TemplateURLData data;
    data.SetShortName(base::UTF8ToUTF16(name));
    data.SetKeyword(base::UTF8ToUTF16(name));
    data.SetURL("https://" + name + ".com/q={searchTerms}");
    data.policy_origin = policy_origin;
    data.featured_by_policy = featured;
    controller_.template_url_service_->Add(std::make_unique<TemplateURL>(data));
  };

  add_template_url("site_search_not_featured",
                   TemplateURLData::PolicyOrigin::kSiteSearch, false);
  add_template_url("site_search_featured",
                   TemplateURLData::PolicyOrigin::kSiteSearch, true);
  add_template_url("aggregator_not_featured",
                   TemplateURLData::PolicyOrigin::kSearchAggregator, false);
  add_template_url("aggregator_featured",
                   TemplateURLData::PolicyOrigin::kSearchAggregator, true);

  // Setup the providers.
  auto aggregator_provider = base::MakeRefCounted<FakeAutocompleteProvider>(
      AutocompleteProvider::Type::TYPE_ENTERPRISE_SEARCH_AGGREGATOR);
  controller_.providers_.push_back(aggregator_provider);
  auto document_provider = base::MakeRefCounted<FakeAutocompleteProvider>(
      AutocompleteProvider::Type::TYPE_DOCUMENT);
  controller_.providers_.push_back(document_provider);

  // In unscoped mode (not keyword mode), aggregator is run when
  // `require_shortcut` policy field is false, and is not run when
  // `require_shortcut` policy field is true. When it is run, the document
  // provider should not be run and vice versa.
  controller_.input_ = AutocompleteInput(
      u"query", 1u, metrics::OmniboxEventProto::OTHER, TestSchemeClassifier());
  EXPECT_TRUE(controller_.ShouldRunProvider(aggregator_provider.get()));
  EXPECT_FALSE(controller_.ShouldRunProvider(document_provider.get()));

  pref_service()->SetManagedPref(
      EnterpriseSearchManager::
          kEnterpriseSearchAggregatorSettingsRequireShortcutPrefName,
      base::Value(true));
  EXPECT_FALSE(controller_.ShouldRunProvider(aggregator_provider.get()));
  EXPECT_TRUE(controller_.ShouldRunProvider(document_provider.get()));

  // If the feature param `disable_drive` is false, then the document provider
  // should run regardless of whether the aggregator provider is ran.
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::SearchAggregatorProvider>
      scoped_config;
  scoped_config.Get().disable_drive = false;
  pref_service()->SetManagedPref(
      EnterpriseSearchManager::
          kEnterpriseSearchAggregatorSettingsRequireShortcutPrefName,
      base::Value(false));
  EXPECT_TRUE(controller_.ShouldRunProvider(aggregator_provider.get()));
  EXPECT_TRUE(controller_.ShouldRunProvider(document_provider.get()));

  pref_service()->SetManagedPref(
      EnterpriseSearchManager::
          kEnterpriseSearchAggregatorSettingsRequireShortcutPrefName,
      base::Value(true));
  EXPECT_FALSE(controller_.ShouldRunProvider(aggregator_provider.get()));
  EXPECT_TRUE(controller_.ShouldRunProvider(document_provider.get()));

  // Enter keyword mode.
  controller_.input_.set_keyword_mode_entry_method(
      metrics::OmniboxEventProto_KeywordModeEntryMethod_TAB);

  // Aggregator not ran when in site search mode, regardless of
  // `enterprise_search_aggregator_settings.require_shortcut` pref value.
  controller_.input_.UpdateText(u"site_search_not_featured", 0, {});
  EXPECT_FALSE(controller_.ShouldRunProvider(aggregator_provider.get()));
  controller_.input_.UpdateText(u"site_search_featured", 0, {});
  EXPECT_FALSE(controller_.ShouldRunProvider(aggregator_provider.get()));

  pref_service()->SetManagedPref(
      EnterpriseSearchManager::
          kEnterpriseSearchAggregatorSettingsRequireShortcutPrefName,
      base::Value(false));
  controller_.input_.UpdateText(u"site_search_not_featured", 0, {});
  EXPECT_FALSE(controller_.ShouldRunProvider(aggregator_provider.get()));
  controller_.input_.UpdateText(u"site_search_featured", 0, {});
  EXPECT_FALSE(controller_.ShouldRunProvider(aggregator_provider.get()));

  // Only search, keyword, and aggregator providers ran when in aggregator mode.
  std::set<AutocompleteProvider::Type> expected_provider_types = {
      AutocompleteProvider::TYPE_SEARCH, AutocompleteProvider::TYPE_KEYWORD,
      AutocompleteProvider::Type::TYPE_ENTERPRISE_SEARCH_AGGREGATOR};
  controller_.input_.UpdateText(u"aggregator_not_featured", 0, {});
  for (auto& provider : controller_.providers()) {
    EXPECT_EQ(controller_.ShouldRunProvider(provider.get()),
              expected_provider_types.contains(provider->type()))
        << "Provider Type: "
        << AutocompleteProvider::TypeToString(provider->type());
  }
  controller_.input_.UpdateText(u"aggregator_featured", 0, {});
  for (auto& provider : controller_.providers()) {
    EXPECT_EQ(controller_.ShouldRunProvider(provider.get()),
              expected_provider_types.contains(provider->type()))
        << "Provider Type: "
        << AutocompleteProvider::TypeToString(provider->type());
  }
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
       // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
TEST_F(AutocompleteControllerTest, ShouldRunProvider_AndroidHubSearch) {
  // Include bookmarks and history as default providers for hub search.
  std::set<AutocompleteProvider::Type> expected_provider_types = {
      AutocompleteProvider::TYPE_SEARCH, AutocompleteProvider::TYPE_OPEN_TAB,
      AutocompleteProvider::TYPE_BOOKMARK,
      AutocompleteProvider::TYPE_HISTORY_QUICK};

  controller_.input_ =
      AutocompleteInput(u"a", 1u, metrics::OmniboxEventProto::ANDROID_HUB,
                        TestSchemeClassifier());
  for (auto& provider : controller_.providers()) {
    EXPECT_EQ(controller_.ShouldRunProvider(provider.get()),
              expected_provider_types.contains(provider->type()))
        << "Provider Type: "
        << AutocompleteProvider::TypeToString(provider->type());
  }
}
#endif

// Android and iOS have different handling for pedals.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(AutocompleteControllerTest, NoActionsAttachedToLensSearchboxMatches) {
  std::unordered_map<OmniboxPedalId, scoped_refptr<OmniboxPedal>> pedals;
  const auto add = [&](OmniboxPedal* pedal) {
    pedals.insert(
        std::make_pair(pedal->PedalId(), base::WrapRefCounted(pedal)));
  };
  add(new TestOmniboxPedalClearBrowsingData());
  provider_client()->set_pedal_provider(std::make_unique<OmniboxPedalProvider>(
      *provider_client(), std::move(pedals)));
  EXPECT_NE(nullptr, provider_client()->GetPedalProvider());

  // Create input with lens searchbox page classification.
  controller_.input_ = AutocompleteInput(
      u"Clear History", metrics::OmniboxEventProto::LENS_SIDE_PANEL_SEARCHBOX,
      TestSchemeClassifier());

  SetAutocompleteMatches(
      {CreateSearchMatch(u"Clear History"), CreateSearchMatch(u"search 1"),
       CreateSearchMatch(u"search 2"),
       CreateHistoryURLMatch(
           /*destination_url=*/"http://this-site-matches.com")});

  static_cast<FakeTabMatcher&>(
      const_cast<TabMatcher&>(provider_client()->GetTabMatcher()))
      .set_url_substring_match("matches");

  controller_.AttachActions();

  // For a Lens Searchbox, AttachActions should not attach a pedal to the
  // first match, and therefore it won't get split out into a separate pedal
  // match. It also shouldn't attach a switch to this tab action to the last
  // match.
  EXPECT_EQ(nullptr, controller_.internal_result_.match_at(1)->takeover_action);
  EXPECT_FALSE(
      controller_.internal_result_.match_at(3)->has_tab_match.value_or(false));

  controller_.input_ =
      AutocompleteInput(u"Clear History", metrics::OmniboxEventProto::OTHER,
                        TestSchemeClassifier());

  SetAutocompleteMatches(
      {CreateSearchMatch(u"Clear History"),
       CreateHistoryURLMatch(
           /*destination_url=*/"http://this-site-matches.com"),
       CreateSearchMatch(u"search 1"), CreateSearchMatch(u"search 2")});

  controller_.AttachActions();

  // For any other page classification, AttachActions should attach a pedal
  // and a switch to this tab action to the relevant matches.
  EXPECT_EQ(
      OmniboxActionId::PEDAL,
      controller_.internal_result_.match_at(1)->takeover_action->ActionId());
  EXPECT_TRUE(
      controller_.internal_result_.match_at(2)->has_tab_match.value_or(false));
}
#endif

// Android and iOS have different handling for pedals.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(AutocompleteControllerTest, NoActionsAttachedToNtpComposeboxMatches) {
  // Create input with lens searchbox page classification.
  controller_.input_ = AutocompleteInput(
      u"Clear History", metrics::OmniboxEventProto::NTP_COMPOSEBOX,
      TestSchemeClassifier());

  SetAutocompleteMatches(
      {CreateSearchMatch(u"search 2"),
       CreateHistoryURLMatch(
           /*destination_url=*/"http://this-site-matches.com")});

  static_cast<FakeTabMatcher&>(
      const_cast<TabMatcher&>(provider_client()->GetTabMatcher()))
      .set_url_substring_match("matches");

  controller_.AttachActions();

  // For a Lens Searchbox, AttachActions shouldn't attach a switch to this tab
  // action to the last match.
  EXPECT_FALSE(
      controller_.internal_result_.match_at(1)->has_tab_match.value_or(false));

  controller_.input_ =
      AutocompleteInput(u"Clear History", metrics::OmniboxEventProto::OTHER,
                        TestSchemeClassifier());

  SetAutocompleteMatches(
      {CreateSearchMatch(u"Clear History"),
       CreateHistoryURLMatch(
           /*destination_url=*/"http://this-site-matches.com")});

  controller_.AttachActions();

  // For any other page classification, AttachActions should attach a switch
  // to this tab action to the relevant matches.
  EXPECT_TRUE(
      controller_.internal_result_.match_at(1)->has_tab_match.value_or(false));
}
#endif

// Feature not enabled on Android and iOS.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(AutocompleteControllerTest,
       ContextualSearchActionAttachedPageKeywordMode) {
  // Create a pedal provider to ensure that the contextual search action takes
  // precedence over the pedal.
  std::unordered_map<OmniboxPedalId, scoped_refptr<OmniboxPedal>> pedals;
  const auto add = [&](OmniboxPedal* pedal) {
    pedals.insert(
        std::make_pair(pedal->PedalId(), base::WrapRefCounted(pedal)));
  };
  add(new TestOmniboxPedalClearBrowsingData());
  provider_client()->set_pedal_provider(std::make_unique<OmniboxPedalProvider>(
      *provider_client(), std::move(pedals)));
  EXPECT_NE(nullptr, provider_client()->GetPedalProvider());

  // Populate template URL service with starter pack entries.
  for (auto& turl_data :
       template_url_starter_pack_data::GetStarterPackEngines()) {
    controller_.template_url_service_->Add(
        std::make_unique<TemplateURL>(std::move(*turl_data)));
  }

  // Create input with lens searchbox page classification.
  controller_.input_ =
      AutocompleteInput(u"@page Summar", metrics::OmniboxEventProto::OTHER,
                        TestSchemeClassifier());
  controller_.input_.set_keyword_mode_entry_method(
      metrics::OmniboxEventProto::SPACE_AT_END);

  SetAutocompleteMatches({CreateContextualSearchMatch(u"Summary"),
                          CreateContextualSearchMatch(u"Summarize this page")});

  static_cast<FakeTabMatcher&>(
      const_cast<TabMatcher&>(provider_client()->GetTabMatcher()))
      .set_url_substring_match("matches");

  controller_.AttachActions();

  // The takeover action should be for the contextual search action, not pedals.
  ASSERT_TRUE(controller_.internal_result_.match_at(0)->takeover_action);
  EXPECT_EQ(
      OmniboxActionId::CONTEXTUAL_SEARCH_FULFILLMENT,
      controller_.internal_result_.match_at(0)->takeover_action->ActionId());
  ASSERT_TRUE(controller_.internal_result_.match_at(1)->takeover_action);
  EXPECT_EQ(
      OmniboxActionId::CONTEXTUAL_SEARCH_FULFILLMENT,
      controller_.internal_result_.match_at(1)->takeover_action->ActionId());
}

TEST_F(AutocompleteControllerTest, ContextualQueryAppendsSearchboxStats) {
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::ContextualSearch>
      contextual_search_config;
  contextual_search_config.Get().contextual_zero_suggest_lens_fulfillment =
      true;
  TemplateURLData turl_data;
  turl_data.SetShortName(u"Contextual");
  turl_data.SetKeyword(u"contextual");
  turl_data.SetURL(
      "https://google.com/search?q={searchTerms}/{google:assistedQueryStats}");
  controller_.template_url_service_->Add(
      std::make_unique<TemplateURL>(turl_data));

  // Create input with lens searchbox page classification.
  controller_.input_ = AutocompleteInput(u"", metrics::OmniboxEventProto::OTHER,
                                         TestSchemeClassifier());
  controller_.input_.set_focus_type(
      metrics::OmniboxFocusType::INTERACTION_FOCUS);

  SetAutocompleteMatches(
      {CreateZeroSuggestContextualSearchMatch(u"Summary"),
       CreateZeroSuggestContextualSearchMatch(u"Summarize this page")});

  controller_.AttachActions();
  UpdateSearchboxStats();

  // The takeover action should be for the contextual search action, not pedals.
  ASSERT_TRUE(controller_.internal_result_.match_at(0)->takeover_action);
  auto* contextual_takover_action_0 =
      ContextualSearchFulfillmentAction::FromAction(
          controller_.internal_result_.match_at(0)->takeover_action.get());
  EXPECT_EQ(OmniboxActionId::CONTEXTUAL_SEARCH_FULFILLMENT,
            contextual_takover_action_0->ActionId());
  EXPECT_TRUE(base::Contains(
      contextual_takover_action_0->get_fulfillment_url_for_testing().spec(),
      "gs_lcrp="));
  ASSERT_TRUE(controller_.internal_result_.match_at(1)->takeover_action);
  auto* contextual_takover_action_1 =
      ContextualSearchFulfillmentAction::FromAction(
          controller_.internal_result_.match_at(1)->takeover_action.get());
  EXPECT_EQ(OmniboxActionId::CONTEXTUAL_SEARCH_FULFILLMENT,
            contextual_takover_action_1->ActionId());
  EXPECT_TRUE(base::Contains(
      contextual_takover_action_1->get_fulfillment_url_for_testing().spec(),
      "gs_lcrp="));
}

TEST_F(AutocompleteControllerTest,
       ContextualSearchActionAttachedInZeroSuggest) {
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::ContextualSearch>
      contextual_search_config;
  contextual_search_config.Get().contextual_zero_suggest_lens_fulfillment =
      true;

  EXPECT_CALL(*provider_client(), IsLensEnabled())
      .WillRepeatedly(testing::Return(true));

  // Create a pedal provider to ensure that the contextual search action takes
  // precedence over the pedal.
  std::unordered_map<OmniboxPedalId, scoped_refptr<OmniboxPedal>> pedals;
  const auto add = [&](OmniboxPedal* pedal) {
    pedals.insert(
        std::make_pair(pedal->PedalId(), base::WrapRefCounted(pedal)));
  };
  add(new TestOmniboxPedalClearBrowsingData());
  provider_client()->set_pedal_provider(std::make_unique<OmniboxPedalProvider>(
      *provider_client(), std::move(pedals)));
  EXPECT_NE(nullptr, provider_client()->GetPedalProvider());

  // Create input for zero suggest.
  controller_.input_ = AutocompleteInput(u"", metrics::OmniboxEventProto::OTHER,
                                         TestSchemeClassifier());
  controller_.input_.set_focus_type(
      metrics::OmniboxFocusType::INTERACTION_FOCUS);

  // Create ZPS matches.
  auto contextual_search_match_1 =
      CreatePersonalizedZeroPrefixMatch("contextual search match 1", 1450);
  contextual_search_match_1.subtypes.insert(omnibox::SUBTYPE_CONTEXTUAL_SEARCH);
  auto contextual_search_match_2 =
      CreatePersonalizedZeroPrefixMatch("contextual search match 2", 1450);
  contextual_search_match_2.subtypes.insert(omnibox::SUBTYPE_CONTEXTUAL_SEARCH);

  SetAutocompleteMatches(
      {CreatePersonalizedZeroPrefixMatch("normal zps match 1", 1200),
       contextual_search_match_1, contextual_search_match_2,
       CreatePersonalizedZeroPrefixMatch("noormal zps match 1", 1550)});

  static_cast<FakeTabMatcher&>(
      const_cast<TabMatcher&>(provider_client()->GetTabMatcher()))
      .set_url_substring_match("matches");

  controller_.AttachActions();

  // The takeover action should be for the contextual suggestions, but not
  // others.
  EXPECT_FALSE(controller_.internal_result_.match_at(0)->takeover_action);
  EXPECT_FALSE(controller_.internal_result_.match_at(3)->takeover_action);

  ASSERT_TRUE(controller_.internal_result_.match_at(1)->takeover_action);
  EXPECT_EQ(
      OmniboxActionId::CONTEXTUAL_SEARCH_FULFILLMENT,
      controller_.internal_result_.match_at(1)->takeover_action->ActionId());
  ASSERT_TRUE(controller_.internal_result_.match_at(2)->takeover_action);
  EXPECT_EQ(
      OmniboxActionId::CONTEXTUAL_SEARCH_FULFILLMENT,
      controller_.internal_result_.match_at(2)->takeover_action->ActionId());
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

TEST_F(AutocompleteControllerTest, UpdateAssociatedKeywords) {
  controller_.keyword_provider_ =
      new KeywordProvider(provider_client(), nullptr);
  controller_.providers_.push_back(controller_.keyword_provider_.get());

  auto add_keyword = [&](std::u16string keyword, bool is_starter_pack = false,
                         bool is_featured_enterprise_search = false) {
    TemplateURLData turl_data;
    turl_data.SetShortName(u"name");
    turl_data.SetURL("https://google.com/search?q={searchTerms}");
    turl_data.is_active = TemplateURLData::ActiveStatus::kTrue;
    turl_data.SetKeyword(keyword);
    if (is_starter_pack) {
      turl_data.starter_pack_id = 1;
    } else if (is_featured_enterprise_search) {
      turl_data.featured_by_policy = true;
    }
    controller_.template_url_service_->Add(
        std::make_unique<TemplateURL>(turl_data));
  };

  struct MatchData {
    std::u16string fill_into_edit;
    AutocompleteMatchType::Type type;
  };

  auto test = [&](const std::u16string input_text,
                  const std::u16string input_keyword,
                  std::vector<MatchData> match_datas,
                  bool is_zero_suggest = false) {
    controller_.input_ = FakeAutocompleteController::CreateInput(input_text);
    if (is_zero_suggest) {
      controller_.input_.set_focus_type(
          metrics::OmniboxFocusType::INTERACTION_FOCUS);
    }
    AutocompleteResult result;
    for (const auto& match_data : match_datas) {
      AutocompleteMatch match;
      match.fill_into_edit = match_data.fill_into_edit;
      match.type = match_data.type;
      result.AppendMatches({match});
    }
    if (!input_keyword.empty()) {
      result.match_at(0)->keyword = input_keyword;
      result.match_at(0)->transition = ui::PAGE_TRANSITION_KEYWORD;
    }
    controller_.UpdateAssociatedKeywords(&result);

    std::vector<std::u16string> attached_keywords;
    for (const auto& match : result) {
      attached_keywords.push_back(match.associated_keyword);
    }
    return attached_keywords;
  };

  add_keyword(u"keyword_0");
  add_keyword(u"keyword_1");
  add_keyword(u"keyword_starter_pack", true);
  add_keyword(u"keyword_featured_enterprise_search", false, true);

  // When the input text's 1st word matches a keyword, the keyword hint is added
  // to the 1st match regardless of which match is similar to the keyword. Only
  // 1 keyword is added even if there's another match matching the keyword.
  EXPECT_THAT(test(u"keyword_0", u"", {{u"bing.com"}, {u"keyword_0"}}),
              testing::ElementsAreArray({u"keyword_0", u""}));
  EXPECT_THAT(
      test(u"keyword_0 more words", u"", {{u"bing.com"}, {u"keyword_0"}}),
      testing::ElementsAreArray({u"keyword_0", u""}));

  // Only 1 keyword is added even if there're 2 non-exact matches matching the
  // keyword.
  EXPECT_THAT(
      test(u"input", u"", {{u"bing.com"}, {u"keyword_0"}, {u"keyword_0"}}),
      testing::ElementsAreArray({u"", u"keyword_0", u""}));

  // When the user is in a keyword mode, don't show keyword hints for that
  // keyword, but do still show other keywords.
  EXPECT_THAT(test(u"keyword_0", u"keyword_0",
                   {{u"keyword_0"}, {u"keyword_0"}, {u"keyword_1"}}),
              testing::ElementsAreArray({u"", u"", u"keyword_1"}));

  // Starter pack and featured enterprise matches should always have keywords,
  // regardless of the input or the match position.
  EXPECT_THAT(test(u"input", u"",
                   {{u"keyword_starter_pack",
                     AutocompleteMatchType::Type::STARTER_PACK},
                    {u"keyword_featured_enterprise_search",
                     AutocompleteMatchType::Type::FEATURED_ENTERPRISE_SEARCH},
                    {u"keyword_0"},
                    {u"keywo"}}),
              testing::ElementsAreArray({u"keyword_starter_pack",
                                         u"keyword_featured_enterprise_search",
                                         u"keyword_0", u""}));

  // Normal matches should not have keyword hints for starter pack or featured
  // enterprise keywords.
  EXPECT_THAT(test(u"input", u"",
                   {{u"keyword_starter_pack"},
                    {u"keyword_featured_enterprise_search"}}),
              testing::ElementsAreArray({u"", u""}));

  // Normal matches should not have keyword hints for starter pack or featured
  // enterprise keywords, even if the input is an exact keyword match.
  EXPECT_THAT(test(u"keyword_starter_pack", u"", {{u"keyword_starter_pack"}}),
              testing::ElementsAreArray({u""}));
  EXPECT_THAT(test(u"keyword_featured_enterprise_search", u"",
                   {{u"keyword_featured_enterprise_search"}}),
              testing::ElementsAreArray({u""}));

  // Keywords are added if the 1st word of the match text matches, even if the
  // match text has more non-matching words after. Keywords are not added if the
  // 1st word of the match text is a prefix of or prefixed by the keyword.
  EXPECT_THAT(
      test(u"input", u"",
           {{u"keywo"}, {u"keyword_0_underscore"}, {u"keyword_0 space"}}),
      testing::ElementsAreArray({u"", u"", u"keyword_0"}));

  EXPECT_THAT(test(u"", u"",
                   {{u"keywo", AutocompleteMatchType::Type::NAVSUGGEST},
                    {u"keyword_0_underscore"},
                    {u"keyword_0 space"}},
                   /*is_zero_suggest=*/true),
              testing::ElementsAreArray({u"", u"", u""}));
}

// Helper function to create a basic AutocompleteMatch for testing default match
// changes.
AutocompleteMatch CreateDefaultMatch(std::u16string fill_into_edit,
                                     GURL icon_url,
                                     std::u16string associated_keyword,
                                     std::u16string keyword) {
  AutocompleteMatch match;
  match.fill_into_edit = fill_into_edit;
  match.icon_url = icon_url;
  match.associated_keyword = associated_keyword;
  match.keyword = keyword;

  // Set other fields to make it a plausible default match
  match.relevance = 1000;
  match.allowed_to_be_default_match = true;
  match.destination_url =
      GURL("https://foo.com/" + base::UTF16ToUTF8(match.fill_into_edit));

  return match;
}

TEST_F(AutocompleteControllerTest, CheckWhetherDefaultMatchChanged) {
  // Helper lambda to set the internal default match
  auto set_current_default = [&](std::optional<AutocompleteMatch> match) {
    controller_.internal_result_.ClearMatches();  // Clear previous matches
    if (match) {
      controller_.internal_result_.AppendMatches({*match});
    }
  };

  // Helper lambda to call the private method under test
  auto check_change =
      [&](std::optional<AutocompleteMatch> last_default_match,
          const std::u16string& last_default_associated_keyword) {
        // Reset timestamp before check
        controller_.last_time_default_match_changed_ = base::TimeTicks();
        bool changed = controller_.CheckWhetherDefaultMatchChanged(
            last_default_match, last_default_associated_keyword);
        // Check if timestamp was updated only if a change was detected
        if (changed) {
          EXPECT_NE(controller_.last_time_default_match_changed_,
                    base::TimeTicks());
        } else {
          EXPECT_EQ(controller_.last_time_default_match_changed_,
                    base::TimeTicks());
        }
        return changed;
      };

  {
    // No change: Both null
    set_current_default(std::nullopt);
    EXPECT_FALSE(check_change(std::nullopt, u""));
  }
  {
    // No change: Both exist and are identical
    auto match = CreateDefaultMatch(u"test1", GURL("https://www.foo.com/icon1"),
                                    u"assoc1", u"key1");
    set_current_default(match);
    EXPECT_FALSE(check_change(match, u"assoc1"));
  }
  {
    // No change: Irrelevant fields differ (e.g., relevance)
    auto match = CreateDefaultMatch(u"test1", GURL("https://www.foo.com/icon1"),
                                    u"assoc1", u"key1");
    auto match_different_relevance = match;
    match_different_relevance.relevance = 900;
    set_current_default(match);
    EXPECT_FALSE(check_change(match_different_relevance, u"assoc1"));
  }
  {
    // Change: Existence (last had value, current doesn't)
    auto match = CreateDefaultMatch(u"test1", GURL("https://www.foo.com/icon1"),
                                    u"assoc1", u"key1");
    set_current_default(std::nullopt);
    EXPECT_TRUE(check_change(match, u"assoc1"));
  }
  {
    // Change: Existence (last didn't have value, current does)
    auto match = CreateDefaultMatch(u"test1", GURL("https://www.foo.com/icon1"),
                                    u"assoc1", u"key1");
    set_current_default(match);
    EXPECT_TRUE(check_change(std::nullopt, u""));
  }
  {
    // Change: fill_into_edit differs
    auto match = CreateDefaultMatch(u"test1", GURL("https://www.foo.com/icon1"),
                                    u"assoc1", u"key1");
    auto match_different_fill_into_edit = CreateDefaultMatch(
        u"test2", GURL("https://www.foo.com/icon1"), u"assoc1", u"key1");
    set_current_default(match_different_fill_into_edit);
    EXPECT_TRUE(check_change(match, u"assoc1"));
  }
  {
    // Change: icon_url differs
    auto match = CreateDefaultMatch(u"test1", GURL("https://www.foo.com/icon1"),
                                    u"assoc1", u"key1");
    auto match_different_icon_url = CreateDefaultMatch(
        u"test1", GURL("https://www.foo.com/icon2"), u"assoc1", u"key1");
    set_current_default(match_different_icon_url);
    EXPECT_TRUE(check_change(match, u"assoc1"));
  }
  {
    // Change: associated_keyword existence differs (last had, current doesn't)
    auto match = CreateDefaultMatch(u"test1", GURL("https://www.foo.com/icon1"),
                                    u"assoc1", u"key1");
    auto match_no_associated_keyword = CreateDefaultMatch(
        u"test1", GURL("https://www.foo.com/icon1"), u"", u"key1");
    set_current_default(match_no_associated_keyword);
    EXPECT_TRUE(check_change(match, u"assoc1"));
  }
  {
    // Change: associated_keyword existence differs (last didn't, current does)
    auto match = CreateDefaultMatch(u"test1", GURL("https://www.foo.com/icon1"),
                                    u"assoc1", u"key1");
    auto match_no_associated_keyword = CreateDefaultMatch(
        u"test1", GURL("https://www.foo.com/icon1"), u"", u"key1");
    set_current_default(match);
    EXPECT_TRUE(
        check_change(match_no_associated_keyword, u""));  // double check this
  }
  {
    // Change: associated_keyword differs
    auto match = CreateDefaultMatch(u"test1", GURL("https://www.foo.com/icon1"),
                                    u"assoc1", u"key1");
    auto match_different_associated_keyword = CreateDefaultMatch(
        u"test1", GURL("https://www.foo.com/icon1"), u"assoc2", u"key1");
    set_current_default(match_different_associated_keyword);
    EXPECT_TRUE(check_change(match, u"assoc1"));
  }
  {
    // No change: associated_keyword same
    auto match = CreateDefaultMatch(u"test1", GURL("https://www.foo.com/icon1"),
                                    u"assoc1", u"key1");
    set_current_default(match);
    EXPECT_FALSE(check_change(match, u"assoc1"));
  }
  {
    // Change: keyword differs
    auto match = CreateDefaultMatch(u"test1", GURL("https://www.foo.com/icon1"),
                                    u"assoc1", u"key1");
    auto match_different_keyword = CreateDefaultMatch(
        u"test1", GURL("https://www.foo.com/icon1"), u"assoc1", u"key2");
    set_current_default(match_different_keyword);
    EXPECT_TRUE(check_change(match, u"assoc1"));
  }
  {
    // No change: keyword same
    auto match = CreateDefaultMatch(u"test1", GURL("https://www.foo.com/icon1"),
                                    u"assoc1", u"key1");
    set_current_default(match);
    EXPECT_FALSE(check_change(match, u"assoc1"));
  }
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(AutocompleteControllerTest,
       AttachContextualSearchOpenLensActionToMatches) {
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::ContextualSearch>
      contextual_search_config;
  contextual_search_config.Get().contextual_zero_suggest_lens_fulfillment =
      true;
  contextual_search_config.Get().suggestions_fulfilled_by_lens_supported = true;

  // Create a zero-suggest input.
  controller_.input_ = AutocompleteInput(u"", metrics::OmniboxEventProto::OTHER,
                                         TestSchemeClassifier());
  controller_.input_.set_focus_type(
      metrics::OmniboxFocusType::INTERACTION_FOCUS);

  ACMatches matches;

  // Match 1: Contextual search suggestion with Lens action.
  AutocompleteMatch match1;
  match1.subtypes.insert(omnibox::SuggestSubtype::SUBTYPE_CONTEXTUAL_SEARCH);
  match1.suggest_template = omnibox::SuggestTemplateInfo();
  auto* action1 = match1.suggest_template->add_action_suggestions();
  action1->set_action_type(
      omnibox::SuggestTemplateInfo_TemplateAction_ActionType_CHROME_LENS);
  matches.push_back(match1);

  // Match 2: Contextual search suggestion without Lens action.
  AutocompleteMatch match2;
  match2.subtypes.insert(omnibox::SuggestSubtype::SUBTYPE_CONTEXTUAL_SEARCH);
  matches.push_back(match2);

  // Match 3: Non-contextual search suggestion with Lens action.
  AutocompleteMatch match3;
  match3.suggest_template = omnibox::SuggestTemplateInfo();
  auto* action3 = match3.suggest_template->add_action_suggestions();
  action3->set_action_type(
      omnibox::SuggestTemplateInfo_TemplateAction_ActionType_CHROME_LENS);
  matches.push_back(match3);

  // Match 4: Non-contextual search suggestion without Lens action.
  AutocompleteMatch match4;
  matches.push_back(match4);

  SetAutocompleteMatches(matches);
  controller_.AttachActions();

  ASSERT_EQ(4u, controller_.internal_result_.size());

  // Match 1 should have the open Lens takeover action.
  EXPECT_TRUE(controller_.internal_result_.match_at(0)->takeover_action);
  EXPECT_EQ(
      controller_.internal_result_.match_at(0)->takeover_action->ActionId(),
      OmniboxActionId::CONTEXTUAL_SEARCH_OPEN_LENS);

  // Others should not.
  EXPECT_FALSE(
      controller_.internal_result_.match_at(1)->takeover_action->ActionId() ==
      OmniboxActionId::CONTEXTUAL_SEARCH_OPEN_LENS);
  EXPECT_FALSE(controller_.internal_result_.match_at(2)->takeover_action);
  EXPECT_FALSE(controller_.internal_result_.match_at(3)->takeover_action);
}

TEST_F(AutocompleteControllerTest,
       ContextualSearchOpenLensActionAttachedPageKeywordMode) {
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::ContextualSearch>
      contextual_search_config;
  contextual_search_config.Get().suggestions_fulfilled_by_lens_supported = true;

  // Create a pedal provider to ensure that the contextual search action takes
  // precedence over the pedal.
  std::unordered_map<OmniboxPedalId, scoped_refptr<OmniboxPedal>> pedals;
  const auto add = [&](OmniboxPedal* pedal) {
    pedals.insert(
        std::make_pair(pedal->PedalId(), base::WrapRefCounted(pedal)));
  };
  add(new TestOmniboxPedalClearBrowsingData());
  provider_client()->set_pedal_provider(std::make_unique<OmniboxPedalProvider>(
      *provider_client(), std::move(pedals)));
  EXPECT_NE(nullptr, provider_client()->GetPedalProvider());

  // Populate template URL service with starter pack entries.
  for (auto& turl_data :
       template_url_starter_pack_data::GetStarterPackEngines()) {
    controller_.template_url_service_->Add(
        std::make_unique<TemplateURL>(std::move(*turl_data)));
  }

  // Create input with lens searchbox page classification.
  controller_.input_ =
      AutocompleteInput(u"@page Summar", metrics::OmniboxEventProto::OTHER,
                        TestSchemeClassifier());
  controller_.input_.set_keyword_mode_entry_method(
      metrics::OmniboxEventProto::SPACE_AT_END);

  AutocompleteMatch match1 = CreateContextualSearchMatch(u"Summary");
  match1.suggest_template = omnibox::SuggestTemplateInfo();
  auto* action1 = match1.suggest_template->add_action_suggestions();
  action1->set_action_type(
      omnibox::SuggestTemplateInfo_TemplateAction_ActionType_CHROME_LENS);

  AutocompleteMatch match2 =
      CreateContextualSearchMatch(u"Summarize this page");
  match2.suggest_template = omnibox::SuggestTemplateInfo();
  auto* action2 = match2.suggest_template->add_action_suggestions();
  action2->set_action_type(
      omnibox::SuggestTemplateInfo_TemplateAction_ActionType_CHROME_LENS);

  SetAutocompleteMatches({match1, match2});

  static_cast<FakeTabMatcher&>(
      const_cast<TabMatcher&>(provider_client()->GetTabMatcher()))
      .set_url_substring_match("matches");

  controller_.AttachActions();

  // The takeover action should be for the contextual search action, not pedals.
  ASSERT_TRUE(controller_.internal_result_.match_at(0)->takeover_action);
  EXPECT_EQ(
      OmniboxActionId::CONTEXTUAL_SEARCH_OPEN_LENS,
      controller_.internal_result_.match_at(0)->takeover_action->ActionId());
  ASSERT_TRUE(controller_.internal_result_.match_at(1)->takeover_action);
  EXPECT_EQ(
      OmniboxActionId::CONTEXTUAL_SEARCH_OPEN_LENS,
      controller_.internal_result_.match_at(1)->takeover_action->ActionId());
}

TEST_F(AutocompleteControllerTest, SmartComposeClearedWithNewResults) {
  auto match1 = CreateSearchMatch("match1", true, 1300);
  EXPECT_THAT(controller_.SimulateAutocompletePass(true, false, {match1}),
              testing::ElementsAreArray({
                  "match1",
              }));

  controller_.internal_result_.set_smart_compose_inline_hint("smart compose!");

  // Verify smart compose field is set initially
  ASSERT_TRUE(
      !controller_.internal_result_.smart_compose_inline_hint().empty());

  EXPECT_THAT(controller_.SimulateAutocompletePass(true, false, {match1}),
              testing::ElementsAreArray({
                  "match1",
              }));

  // Smart compose field should not be set after autocomplete pass with no
  // smart compose result.
  ASSERT_TRUE(controller_.internal_result_.smart_compose_inline_hint().empty());
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
