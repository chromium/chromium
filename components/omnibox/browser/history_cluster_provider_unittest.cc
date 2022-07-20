// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/history_cluster_provider.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "components/history/core/browser/history_service.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/history_clusters_prefs.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/history_clusters/core/history_clusters_service_test_api.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/search_provider.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class FakeSearchProvider : public SearchProvider {
 public:
  explicit FakeSearchProvider(AutocompleteProviderClient* client,
                              AutocompleteProviderListener* listener)
      : SearchProvider(client, listener) {}

  using SearchProvider::done_;
  using SearchProvider::matches_;

 private:
  ~FakeSearchProvider() override = default;
};

AutocompleteMatch CreateMatch(std::u16string contents) {
  AutocompleteMatch match;
  match.contents = contents;
  return match;
}

class HistoryClustersProviderTest : public testing::Test,
                                    public AutocompleteProviderListener {
 public:
  HistoryClustersProviderTest() {
    config_.is_journeys_enabled_no_locale_check = true;
    config_.omnibox_history_cluster_provider = true;
    history_clusters::SetConfigForTesting(config_);

    CHECK(history_dir_.CreateUniqueTempDir());
    history_service_ =
        history::CreateHistoryService(history_dir_.GetPath(), true);

    history_clusters_service_ =
        std::make_unique<history_clusters::HistoryClustersService>(
            "en-US", history_service_.get(),
            /*entity_metadata_provider=*/nullptr,
            /*url_loader_factory=*/nullptr,
            /*engagement_score_provider=*/nullptr,
            /*optimization_guide_decider=*/nullptr);

    history_clusters_service_test_api_ =
        std::make_unique<history_clusters::HistoryClustersServiceTestApi>(
            history_clusters_service_.get(), history_service_.get());
    history_clusters_service_test_api_->SetAllKeywordsCache(
        {{u"keyword", {}}, {u"keyword2", {}}});

    autocomplete_provider_client_ =
        std::make_unique<FakeAutocompleteProviderClient>();
    autocomplete_provider_client_->set_history_clusters_service(
        history_clusters_service_.get());
    static_cast<TestingPrefServiceSimple*>(
        autocomplete_provider_client_->GetPrefs())
        ->registry()
        ->RegisterBooleanPref(history_clusters::prefs::kVisible, true);

    search_provider_ =
        new FakeSearchProvider(autocomplete_provider_client_.get(), this);
    provider_ = new HistoryClusterProvider(autocomplete_provider_client_.get(),
                                           this, search_provider_.get());
  };

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
  };

  // Tracks `OnProviderUpdate()` invocations.
  std::vector<bool> on_provider_update_calls_;

  base::test::TaskEnvironment task_environment_;

  base::ScopedTempDir history_dir_;
  std::unique_ptr<history::HistoryService> history_service_;
  std::unique_ptr<history_clusters::HistoryClustersService>
      history_clusters_service_;

  std::unique_ptr<FakeAutocompleteProviderClient> autocomplete_provider_client_;

  scoped_refptr<FakeSearchProvider> search_provider_;
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
  EXPECT_EQ(provider_->matches()[0].contents,
            u"chrome://history/journeys?q=keyword");
  EXPECT_EQ(provider_->matches()[0].fill_into_edit,
            u"chrome://history/journeys?q=keyword");
  EXPECT_EQ(provider_->matches()[0].destination_url,
            GURL("chrome://history/journeys?q=keyword"));

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

  // Calling `Start()` or `OnProviderUpdate()` should process search matches
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

TEST_F(HistoryClustersProviderTest, NoKeyworddMatches) {
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
