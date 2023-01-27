// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/history_clusters_action.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/test/task_environment.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/history_clusters_prefs.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/history_clusters/core/history_clusters_service_test_api.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/actions/omnibox_action_concepts.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace history_clusters {

struct MatchData {
  std::u16string contents = u"keyword";  // Also assigned to `destination_url`.
  int relevance = 1000;
  AutocompleteMatchType::Type type =
      AutocompleteMatchType::Type::SEARCH_SUGGEST;
  bool already_has_action = false;
  bool expect_history_clusters_action = false;
};

ACMatches CreateACMatches(std::vector<MatchData> matches_data) {
  ACMatches matches;
  matches.reserve(matches_data.size());
  base::ranges::transform(
      matches_data, std::back_inserter(matches), [](const auto& match_data) {
        AutocompleteMatch match(nullptr, match_data.relevance, true,
                                match_data.type);
        match.contents = match_data.contents;
        match.destination_url = GURL(u"https://" + match_data.contents);
        if (match_data.already_has_action) {
          match.actions.push_back(base::MakeRefCounted<OmniboxAction>(
              OmniboxAction::LabelStrings{}, GURL{}));
        }
        return match;
      });
  return matches;
}

class HistoryClustersActionTest : public testing::Test {
 public:
  HistoryClustersActionTest() = default;

  // `history_dir_` needs to be initialized once only.
  void SetUp() override {
    CHECK(history_dir_.CreateUniqueTempDir());
    history_service_ =
        history::CreateHistoryService(history_dir_.GetPath(), true);

    prefs_enabled_.registry()->RegisterBooleanPref(
        history_clusters::prefs::kVisible, true);

    search_actions_config_.is_journeys_enabled_no_locale_check = true;
    search_actions_config_.omnibox_action = true;
    search_actions_config_.omnibox_action_on_navigation_intents = false;
    search_actions_config_.omnibox_action_on_urls = false;
    SetConfigForTesting(search_actions_config_);

    url_actions_config_.is_journeys_enabled_no_locale_check = true;
    url_actions_config_.omnibox_action = true;
    url_actions_config_.omnibox_action_on_navigation_intents = false;
    url_actions_config_.omnibox_action_on_urls = true;
    SetConfigForTesting(url_actions_config_);
  }

  // `history_clusters_service_` needs to be initialized repeatedly since it
  // caches `config.is_journeys_enabled_no_locale_check` on initialization.
  void SetUpWithConfig(Config config) {
    SetConfigForTesting(config);

    history_clusters_service_ = std::make_unique<HistoryClustersService>(
        "en-US", history_service_.get(),
        /*entity_metadata_provider=*/nullptr,
        /*url_loader_factory=*/nullptr,
        /*engagement_score_provider=*/nullptr,
        /*template_url_service=*/nullptr,
        /*optimization_guide_decider=*/nullptr, /*pref_service=*/nullptr);

    history_clusters_service_test_api_ =
        std::make_unique<HistoryClustersServiceTestApi>(
            history_clusters_service_.get(), history_service_.get());
    history_clusters_service_test_api_->SetAllKeywordsCache(
        {{u"keyword", history::ClusterKeywordData()}});
    history_clusters_service_test_api_->SetAllUrlKeywordsCache(
        {"http://keyword/"});
  }

  void TestAttachHistoryClustersActions(std::vector<MatchData> matches_data,
                                        HistoryClustersService* service,
                                        TestingPrefServiceSimple* prefs) {
    AutocompleteResult result;
    result.AppendMatches(CreateACMatches(matches_data));

    AttachHistoryClustersActions(service, prefs, result);

    for (size_t i = 0; i < matches_data.size(); ++i) {
      bool has_history_clusters_action =
          result.match_at(i)->GetPrimaryAction() &&
          result.match_at(i)->GetPrimaryAction()->GetID() ==
              static_cast<int32_t>(OmniboxActionId::HISTORY_CLUSTERS);
      EXPECT_EQ(has_history_clusters_action,
                matches_data[i].expect_history_clusters_action);
    }

    // `AttachHistoryClustersActions()` will kick off an async task to refresh
    // the keyword cache. Wait for it to avoid it possibly being processed after
    // the next test case begins.
    history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());
  }

  void TestAttachHistoryClustersActions(std::vector<MatchData> matches_data) {
    TestAttachHistoryClustersActions(
        matches_data, history_clusters_service_.get(), &prefs_enabled_);
  }

  base::test::TaskEnvironment task_environment_;

  base::ScopedTempDir history_dir_;
  std::unique_ptr<history::HistoryService> history_service_;
  std::unique_ptr<HistoryClustersService> history_clusters_service_;

  std::unique_ptr<HistoryClustersServiceTestApi>
      history_clusters_service_test_api_;

  // Commonly used configs & prefs used or derived from in the tests.
  Config search_actions_config_;
  Config url_actions_config_;
  TestingPrefServiceSimple prefs_enabled_;
};

TEST_F(HistoryClustersActionTest, AttachHistoryClustersActions) {
  {
    SCOPED_TRACE("Shouldn't add action if history cluster service is nullptr.");
    SetUpWithConfig(search_actions_config_);
    TestAttachHistoryClustersActions({{}}, nullptr, &prefs_enabled_);
  }

  {
    SCOPED_TRACE("Shouldn't add action if journey feature is disabled.");
    Config config = search_actions_config_;
    config.is_journeys_enabled_no_locale_check = false;
    SetUpWithConfig(config);
    TestAttachHistoryClustersActions({{}});
  }

  {
    SCOPED_TRACE("Shouldn't add action if action chip feature is disabled.");
    Config config = search_actions_config_;
    config.omnibox_action = false;
    SetUpWithConfig(config);
    TestAttachHistoryClustersActions({{}});
  }

  {
    SCOPED_TRACE("Shouldn't add action if `kVisible` pref is false.");
    SetUpWithConfig(search_actions_config_);
    TestingPrefServiceSimple prefs_disabled;
    prefs_disabled.registry()->RegisterBooleanPref(
        history_clusters::prefs::kVisible, false);
    TestAttachHistoryClustersActions({{}}, history_clusters_service_.get(),
                                     &prefs_disabled);
  }

  {
    SCOPED_TRACE("Shouldn't add action if no matches.");
    SetUpWithConfig(search_actions_config_);
    TestAttachHistoryClustersActions({});
  }

  {
    SCOPED_TRACE(
        "Shouldn't add action if `result` contains a pedal, even if it's on a "
        "different match.");
    Config config = search_actions_config_;
    config.omnibox_action_with_pedals = false;
    SetUpWithConfig(config);
    TestAttachHistoryClustersActions({
        {},
        {.contents = u"pedal-match",
         .relevance = 500,
         .already_has_action = true},
    });
  }

  {
    SCOPED_TRACE("Should add action if a search suggestion matches.");
    SetUpWithConfig(search_actions_config_);
    TestAttachHistoryClustersActions(
        {{.expect_history_clusters_action = true}});
  }

  {
    SCOPED_TRACE(
        "Should add action if an action is search entity "
        "suggestion matches.");
    SetUpWithConfig(search_actions_config_);
    TestAttachHistoryClustersActions(
        {{.type = AutocompleteMatchType::Type::SEARCH_SUGGEST_ENTITY,
          .expect_history_clusters_action = true}});
  }

  {
    SCOPED_TRACE(
        "Should add action if a navigation suggestion matches and "
        "`omnibox_action_on_urls` is enabled.");
    SetUpWithConfig(url_actions_config_);
    TestAttachHistoryClustersActions(
        {{.type = AutocompleteMatchType::Type::HISTORY_TITLE,
          .expect_history_clusters_action = true}});
  }

  {
    SCOPED_TRACE(
        "Should not add action if a navigation suggestion matches and "
        "`omnibox_action_on_urls` is disabled.");
    SetUpWithConfig(search_actions_config_);
    TestAttachHistoryClustersActions(
        {{.type = AutocompleteMatchType::Type::HISTORY_TITLE}});
  }

  {
    SCOPED_TRACE(
        "Should add action if both a search and navigation suggestions "
        "match and `omnibox_action_on_urls` is disabled. The search suggestion "
        "should have an action, even if it is ranked & scored lower.");
    SetUpWithConfig(search_actions_config_);
    TestAttachHistoryClustersActions(
        {{.type = AutocompleteMatchType::Type::HISTORY_TITLE},
         {.relevance = 900, .expect_history_clusters_action = true}});
  }

  {
    SCOPED_TRACE(
        "Should add action only to first matching suggestion, even if it is "
        "scored lower.");
    SetUpWithConfig(search_actions_config_);
    TestAttachHistoryClustersActions(
        {{
             .contents = u"bad-keyword",
             .relevance = 1200,
         },
         {.relevance = 900, .expect_history_clusters_action = true},
         {}});
  }

  {
    SCOPED_TRACE(
        "Should add action if a search suggestion matches and the top-scoring "
        "suggestion is a low score navigation suggestion.");
    SetUpWithConfig(search_actions_config_);
    TestAttachHistoryClustersActions(
        {{u"keyword", 1000, AutocompleteMatchType::Type::HISTORY_TITLE},
         {u"keyword", 900, AutocompleteMatchType::Type::SEARCH_SUGGEST, false,
          true}});
  }

  {
    SCOPED_TRACE(
        "Should add action to a top-scoring navigation suggestion, if it is "
        "not high-scoring.");
    SetUpWithConfig(url_actions_config_);
    TestAttachHistoryClustersActions(
        {{.type = AutocompleteMatchType::Type::HISTORY_TITLE,
          .expect_history_clusters_action = true}});
  }

  {
    SCOPED_TRACE(
        "Should not add action if a search suggestion matches and the top "
        "scoring suggestion is a high score navigation suggestion, even if it "
        "doesn't match.");
    SetUpWithConfig(search_actions_config_);
    TestAttachHistoryClustersActions(
        {{.relevance = 1340},
         {
             .contents = u"bad-keyword",
             .relevance = 1350,
             .type = AutocompleteMatchType::Type::HISTORY_TITLE,
         }});
  }

  {
    SCOPED_TRACE(
        "Should add action if a search suggestion matches and the top "
        "scoring suggestion is a search suggestion even if there is a high "
        "score navigation suggestion.");
    SetUpWithConfig(search_actions_config_);
    TestAttachHistoryClustersActions(
        {{.relevance = 1340,
          .type = AutocompleteMatchType::Type::HISTORY_TITLE},
         {.relevance = 1350, .expect_history_clusters_action = true}});
  }

  {
    SCOPED_TRACE(
        "Should add action to a top scoring, high score navigation "
        "suggestion if `omnibox_action_on_navigation_intents` is enabled.");
    Config config = url_actions_config_;
    config.omnibox_action_on_navigation_intents = true;
    SetUpWithConfig(config);
    TestAttachHistoryClustersActions({
        {.relevance = 1350,
         .type = AutocompleteMatchType::Type::HISTORY_TITLE,
         .expect_history_clusters_action = true},
    });
  }
}

}  // namespace history_clusters
