// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/history_cluster_provider.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "components/history/core/browser/history_service.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/history_clusters_prefs.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/history_clusters/core/history_clusters_service_test_api.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/fake_autocomplete_provider.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_triggered_feature_service.h"
#include "components/omnibox/browser/search_provider.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/omnibox_proto/groups.pb.h"

AutocompleteMatch CreateMatch(std::u16string contents,
                              bool is_search = true,
                              int relevance = 100) {
  AutocompleteMatch match;
  match.contents = contents;
  match.type = is_search ? AutocompleteMatchType::SEARCH_SUGGEST
                         : AutocompleteMatchType::HISTORY_URL;
  match.relevance = relevance;
  return match;
}

class HistoryClustersProviderTest : public testing::Test,
                                    public AutocompleteProviderListener {
 public:
  void SetUp() override {
    config_.is_journeys_enabled_no_locale_check = true;
    config_.omnibox_history_cluster_provider = true;
    // Setting this to false even though users see true behavior so that we do
    // not need to register history clusters specific prefs in this test.
    config_.persist_caches_to_prefs = false;
    history_clusters::SetConfigForTesting(config_);

    CHECK(history_dir_.CreateUniqueTempDir());
    history_service_ =
        history::CreateHistoryService(history_dir_.GetPath(), true);

    autocomplete_provider_client_ =
        std::make_unique<FakeAutocompleteProviderClient>();
    static_cast<sync_preferences::TestingPrefServiceSyncable*>(
        autocomplete_provider_client_->GetPrefs())
        ->registry()
        ->RegisterBooleanPref(history_clusters::prefs::kVisible, true);

    history_clusters_service_ =
        std::make_unique<history_clusters::HistoryClustersService>(
            "en-US", history_service_.get(),
            /*url_loader_factory=*/nullptr,
            /*engagement_score_provider=*/nullptr,
            /*template_url_service=*/nullptr,
            /*optimization_guide_decider=*/nullptr,
            autocomplete_provider_client_->GetPrefs());

    history_clusters_service_test_api_ =
        std::make_unique<history_clusters::HistoryClustersServiceTestApi>(
            history_clusters_service_.get(), history_service_.get());
    history_clusters_service_test_api_->SetAllKeywordsCache(
        {{u"keyword", {}}, {u"keyword2", {}}});
    autocomplete_provider_client_->set_history_clusters_service(
        history_clusters_service_.get());

    search_provider_ =
        new FakeAutocompleteProvider(AutocompleteProvider::Type::TYPE_SEARCH);
    history_url_provider_ = new FakeAutocompleteProvider(
        AutocompleteProvider::Type::TYPE_HISTORY_URL);
    history_quick_provider_ = new FakeAutocompleteProvider(
        AutocompleteProvider::Type::TYPE_HISTORY_QUICK);
    provider_ = new HistoryClusterProvider(
        autocomplete_provider_client_.get(), this, search_provider_.get(),
        history_url_provider_.get(), history_quick_provider_.get());
  }

  void TearDown() override {
    autocomplete_provider_client_->set_history_clusters_service(nullptr);
  }

  ~HistoryClustersProviderTest() override {
    // The provider will kick off an async task to refresh the keyword cache.
    // Wait for it to avoid it possibly being processed after the next test case
    // begins.
    history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());
  }

  // AutocompleteProviderListener
  void OnProviderUpdate(bool updated_matches,
                        const AutocompleteProvider* provider) override {
    on_provider_update_calls_.push_back(updated_matches);
  }

  void VerifyFeatureTriggered(bool expected) {
    EXPECT_EQ(
        autocomplete_provider_client_->GetOmniboxTriggeredFeatureService()
            ->GetFeatureTriggeredInSession(
                metrics::OmniboxEventProto_Feature_HISTORY_CLUSTER_SUGGESTION),
        expected);
  }

  // Tracks `OnProviderUpdate()` invocations.
  std::vector<bool> on_provider_update_calls_;

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<FakeAutocompleteProviderClient> autocomplete_provider_client_;

  base::ScopedTempDir history_dir_;
  std::unique_ptr<history::HistoryService> history_service_;
  std::unique_ptr<history_clusters::HistoryClustersService>
      history_clusters_service_;

  scoped_refptr<FakeAutocompleteProvider> search_provider_;
  scoped_refptr<FakeAutocompleteProvider> history_url_provider_;
  scoped_refptr<FakeAutocompleteProvider> history_quick_provider_;
  scoped_refptr<HistoryClusterProvider> provider_;

  std::unique_ptr<history_clusters::HistoryClustersServiceTestApi>
      history_clusters_service_test_api_;

  history_clusters::Config config_;
};

TEST_F(HistoryClustersProviderTest, WantAsynchronousMatchesFalse) {
  // When `input.omit_asynchronous_matches_` is true, should not attempt
  // to provide suggestions.
  AutocompleteInput input;
  input.set_omit_asynchronous_matches(true);

  EXPECT_TRUE(provider_->done());
  provider_->Start(input, false);
  EXPECT_TRUE(provider_->done());
}

TEST_F(HistoryClustersProviderTest, SyncSearchMatches) {
  // Test the unlikely, but valid, case where the search provider completes
  // before the history cluster provider begins.

  AutocompleteInput input;
  input.set_omit_asynchronous_matches(false);

  search_provider_->matches_ = {CreateMatch(u"keyword")};
  search_provider_->done_ = true;
  EXPECT_TRUE(provider_->done());
  provider_->Start(input, false);
  EXPECT_TRUE(provider_->done());
  ASSERT_EQ(provider_->matches().size(), 1u);
  EXPECT_EQ(provider_->matches()[0].relevance, 900);
  EXPECT_EQ(provider_->matches()[0].description, u"keyword");
  EXPECT_EQ(provider_->matches()[0].contents, u"Resume browsing");
  EXPECT_EQ(provider_->matches()[0].fill_into_edit, u"keyword");
  EXPECT_EQ(provider_->matches()[0].destination_url,
            GURL("chrome://history/grouped?q=keyword"));

  EXPECT_TRUE(on_provider_update_calls_.empty());
}

TEST_F(HistoryClustersProviderTest, AsyncSearchMatches) {
  // Test the more common case where the search provider completes after the
  // history cluster provider begins.

  AutocompleteInput input;
  input.set_omit_asynchronous_matches(false);

  // `done()` should be true before starting.
  EXPECT_TRUE(provider_->done());

  // `done()` should be false after starting.
  search_provider_->done_ = false;
  provider_->Start(input, false);
  EXPECT_FALSE(provider_->done());

  // Calling `Start()` or `OnProviderUpdate()` should not process search matches
  // if the search provider is not done when called.
  search_provider_->matches_ = {CreateMatch(u"keyword")};
  provider_->Start(input, false);
  provider_->OnProviderUpdate(true, nullptr);
  search_provider_->done_ = true;
  EXPECT_FALSE(provider_->done());
  EXPECT_TRUE(provider_->matches().empty());

  // Calling `OnProviderUpdate()` should process search matches if the search
  // provider is done.
  provider_->OnProviderUpdate(true, nullptr);
  EXPECT_TRUE(provider_->done());
  ASSERT_EQ(provider_->matches().size(), 1u);
  EXPECT_EQ(provider_->matches()[0].description, u"keyword");

  EXPECT_THAT(on_provider_update_calls_, testing::ElementsAre(true));
}

TEST_F(HistoryClustersProviderTest, EmptySyncSearchMatches) {
  // Test the sync case where the search provider finds no matches.

  AutocompleteInput input;
  input.set_omit_asynchronous_matches(false);

  provider_->Start(input, false);
  EXPECT_TRUE(provider_->done());
  EXPECT_TRUE(provider_->matches().empty());

  EXPECT_TRUE(on_provider_update_calls_.empty());
}

TEST_F(HistoryClustersProviderTest, EmptyAsyncSearchMatches) {
  // Test the async case where the search provider finds no matches.

  AutocompleteInput input;
  input.set_omit_asynchronous_matches(false);

  search_provider_->done_ = false;
  provider_->Start(input, false);
  search_provider_->done_ = true;
  EXPECT_FALSE(provider_->done());
  provider_->OnProviderUpdate(false, nullptr);
  EXPECT_TRUE(provider_->done());
  EXPECT_TRUE(provider_->matches().empty());

  EXPECT_THAT(on_provider_update_calls_, testing::ElementsAre(false));
}

TEST_F(HistoryClustersProviderTest, MultipassSearchMatches) {
  // Test the case where the search provider finds matches in multiple passes.
  // This is typically the case; search-what-you-typed and search-history
  // suggestions are produced syncly, while the other search types from
  // the server are produced asyncly.

  AutocompleteInput input;
  input.set_omit_asynchronous_matches(false);

  // Simulate receiving sync search matches.
  search_provider_->done_ = false;
  search_provider_->matches_.push_back(CreateMatch(u"keyword"));
  search_provider_->matches_.push_back(CreateMatch(u"dolphin"));
  provider_->Start(input, false);
  EXPECT_FALSE(provider_->done());

  // Simulate receiving async search matches.
  provider_->OnProviderUpdate(true, nullptr);
  EXPECT_FALSE(provider_->done());

  // Simulate receiving the last set of async search matches.
  search_provider_->done_ = true;
  provider_->OnProviderUpdate(true, nullptr);
  EXPECT_TRUE(provider_->done());
  ASSERT_EQ(provider_->matches().size(), 1u);
  EXPECT_EQ(provider_->matches()[0].description, u"keyword");

  EXPECT_THAT(on_provider_update_calls_, testing::ElementsAre(true));
}

TEST_F(HistoryClustersProviderTest, MultipassSyncSearchMatches) {
  // Like `MultipassSearchMatches` above, test the case where the search
  // provider tries multiple passes. But in this case, it finds matches in only
  // the sync pass.

  AutocompleteInput input;
  input.set_omit_asynchronous_matches(false);

  // Simulate receiving sync search matches.
  search_provider_->done_ = false;
  search_provider_->matches_.push_back(CreateMatch(u"keyword"));
  search_provider_->matches_.push_back(CreateMatch(u"Levon Aronian"));
  provider_->Start(input, false);
  EXPECT_FALSE(provider_->done());

  // Simulate receiving async search update with no matches.
  search_provider_->done_ = true;
  provider_->OnProviderUpdate(false, nullptr);
  EXPECT_TRUE(provider_->done());
  ASSERT_EQ(provider_->matches().size(), 1u);

  EXPECT_THAT(on_provider_update_calls_, testing::ElementsAre(true));
}

TEST_F(HistoryClustersProviderTest, NoKeywordMatches) {
  // Test the case where none of the search matches match a keyword.

  AutocompleteInput input;
  input.set_omit_asynchronous_matches(false);

  search_provider_->matches_ = {CreateMatch(u"key"), CreateMatch(u"keyworddd"),
                                CreateMatch(u"Tigran Petrosian")};
  search_provider_->done_ = false;
  provider_->Start(input, false);
  search_provider_->done_ = true;
  provider_->OnProviderUpdate(false, nullptr);
  EXPECT_TRUE(provider_->done());
  EXPECT_TRUE(provider_->matches().empty());

  // ALso test that `provider_` calls `OnProviderUpdate()` with false when it
  // completes asyncly without matches.
  EXPECT_THAT(on_provider_update_calls_, testing::ElementsAre(false));
}

TEST_F(HistoryClustersProviderTest, MultipleKeyworddMatches) {
  // Test the case where multiple of the search matches match a keyword.

  AutocompleteInput input;
  input.set_omit_asynchronous_matches(false);

  search_provider_->matches_ = {CreateMatch(u"keyword2"),
                                CreateMatch(u"keyword"),
                                CreateMatch(u"Lilit Mkrtchian")};
  search_provider_->done_ = true;
  provider_->Start(input, false);
  EXPECT_TRUE(provider_->done());
  ASSERT_EQ(provider_->matches().size(), 1u);
  EXPECT_EQ(provider_->matches()[0].description, u"keyword2");

  // Also test that `provider_` does not call `OnProviderUpdate()` when it
  // completes syncly, even if it has matches.
  EXPECT_TRUE(on_provider_update_calls_.empty());
}

TEST_F(HistoryClustersProviderTest, NavIntent_TopAndHighScoringNav) {
  // When there is a nav suggest that is both high scoring (>1300) and top
  // scoring, don't provide history cluster suggestions.
  AutocompleteInput input;
  input.set_omit_asynchronous_matches(false);

  search_provider_->done_ = false;
  history_url_provider_->done_ = false;
  provider_->Start(input, false);

  // Simulate history URL provider completing.
  history_url_provider_->matches_ = {CreateMatch(u"history", false, 1500)};
  history_url_provider_->done_ = true;
  provider_->OnProviderUpdate(true, nullptr);
  EXPECT_FALSE(provider_->done());

  // Simulate search provider completing.
  search_provider_->matches_ = {CreateMatch(u"keyword", true, 1499)};
  search_provider_->done_ = true;
  provider_->OnProviderUpdate(true, nullptr);

  EXPECT_TRUE(provider_->done());
  EXPECT_TRUE(provider_->matches().empty());
  EXPECT_THAT(on_provider_update_calls_, testing::ElementsAre(false));
}

TEST_F(HistoryClustersProviderTest, SearchIntent_TopScoringNav) {
  // When there is a nav suggest that is top scoring but not high scoring
  // (>1300), provide history cluster suggestions.

  AutocompleteInput input;
  input.set_omit_asynchronous_matches(false);

  search_provider_->done_ = false;
  history_url_provider_->done_ = false;
  provider_->Start(input, false);

  // Simulate history URL provider completing.
  history_url_provider_->matches_ = {CreateMatch(u"history", false, 1200)};
  history_url_provider_->done_ = true;
  provider_->OnProviderUpdate(true, nullptr);
  EXPECT_FALSE(provider_->done());

  // Simulate search provider completing.
  search_provider_->matches_ = {CreateMatch(u"keyword", true, 100)};
  search_provider_->done_ = true;
  provider_->OnProviderUpdate(true, nullptr);

  EXPECT_TRUE(provider_->done());
  ASSERT_EQ(provider_->matches().size(), 1u);
  EXPECT_THAT(on_provider_update_calls_, testing::ElementsAre(true));
}

TEST_F(HistoryClustersProviderTest, SearchIntent_HighScoringNav) {
  // When there is a nav suggest that is high scoring (>1300) but not top
  // scoring, provide history cluster suggestions.

  AutocompleteInput input;
  input.set_omit_asynchronous_matches(false);

  search_provider_->done_ = false;
  history_quick_provider_->done_ = false;
  provider_->Start(input, false);

  // Simulate history URL provider completing.
  history_quick_provider_->matches_ = {CreateMatch(u"history", false, 1500)};
  history_quick_provider_->done_ = true;
  provider_->OnProviderUpdate(true, nullptr);
  EXPECT_FALSE(provider_->done());

  // Simulate search provider completing.
  search_provider_->matches_ = {CreateMatch(u"keyword", true, 1501)};
  search_provider_->done_ = true;
  provider_->OnProviderUpdate(true, nullptr);

  EXPECT_TRUE(provider_->done());
  ASSERT_EQ(provider_->matches().size(), 1u);
  EXPECT_THAT(on_provider_update_calls_, testing::ElementsAre(true));
}

TEST_F(HistoryClustersProviderTest, Counterfactual_Disabled) {
  AutocompleteInput input;
  input.set_omit_asynchronous_matches(false);

  // When there are no matches, should not log the feature triggered.
  search_provider_->done_ = true;
  provider_->Start(input, false);
  EXPECT_TRUE(provider_->matches().empty());
  VerifyFeatureTriggered(false);

  // When there are matches, should log the feature triggered.
  search_provider_->matches_ = {CreateMatch(u"keyword")};
  provider_->Start(input, false);
  EXPECT_FALSE(provider_->matches().empty());
  VerifyFeatureTriggered(true);
}

TEST_F(HistoryClustersProviderTest, Counterfactual_Enabled) {
  config_.omnibox_history_cluster_provider_counterfactual = true;
  history_clusters::SetConfigForTesting(config_);

  AutocompleteInput input;
  input.set_omit_asynchronous_matches(false);

  // When there are no matches, should not log the feature triggered.
  search_provider_->done_ = true;
  provider_->Start(input, false);
  EXPECT_TRUE(provider_->matches().empty());
  VerifyFeatureTriggered(false);

  // When there are matches, should (a) log the feature triggered and (b) hide
  // the matches.
  search_provider_->matches_ = {CreateMatch(u"keyword")};
  provider_->Start(input, false);
  EXPECT_TRUE(provider_->matches().empty());
  VerifyFeatureTriggered(true);
}

TEST_F(HistoryClustersProviderTest, Grouping_Ranking) {
  // Should not have groups.
  AutocompleteInput input;
  input.set_omit_asynchronous_matches(false);
  search_provider_->matches_ = {CreateMatch(u"keyword")};
  search_provider_->done_ = true;

  provider_->Start(input, false);
  ASSERT_EQ(provider_->matches().size(), 1u);
  EXPECT_EQ(provider_->matches()[0].suggestion_group_id, std::nullopt);
}
