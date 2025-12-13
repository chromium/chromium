// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/most_visited_sites_provider.h"

#include <list>
#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/history/core/browser/features.h"
#include "components/history/core/browser/top_sites.h"
#include "components/history/core/browser/top_sites_impl.h"
#include "components/omnibox/browser/autocomplete_enums.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/suggestion_group_util.h"
#include "components/omnibox/browser/tab_matcher.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/omnibox/common/omnibox_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "ui/base/device_form_factor.h"

namespace {
struct TestData {
  bool is_search;
  history::MostVisitedURL entry;
};

using OEP = metrics::OmniboxEventProto;
using OFT = metrics::OmniboxFocusType;
using testing::_;

class FakeTopSites : public history::TopSites {
 public:
  FakeTopSites() = default;

  // history::TopSites:
  void GetMostVisitedURLs(GetMostVisitedURLsCallback callback) override {
    callbacks_.push_back(std::move(callback));
  }
  void SyncWithHistory() override {}

  bool HasBlockedUrls() const override { return !blocked_urls_.empty(); }
  void AddBlockedUrl(const GURL& url) override {
    blocked_urls_.insert(url.spec());
  }
  void RemoveBlockedUrl(const GURL& url) override {
    blocked_urls_.erase(url.spec());
  }
  bool IsBlocked(const GURL& url) override {
    return blocked_urls_.count(url.spec()) > 0;
  }
  void ClearBlockedUrls() override { blocked_urls_.clear(); }
  bool IsFull() override { return false; }
  bool loaded() const override { return false; }
  history::PrepopulatedPageList GetPrepopulatedPages() override {
    return history::PrepopulatedPageList();
  }
  void OnNavigationCommitted(const GURL& url) override {}
  int NumBlockedSites() const override { return blocked_urls_.size(); }

  // RefcountedKeyedService:
  void ShutdownOnUIThread() override {}

  // Only runs a single callback, so that the test can specify a different
  // set per call.
  // Returns true if there was a recipient to receive the URLs and the list was
  // emitted, otherwise returns false.
  bool EmitURLs(const std::vector<TestData>& data) {
    if (callbacks_.empty())
      return false;

    history::MostVisitedURLList urls;
    for (const auto& test_element : data) {
      urls.push_back(test_element.entry);
    }

    std::move(callbacks_.front()).Run(std::move(urls));
    callbacks_.pop_front();
    return true;
  }

  const std::set<std::string>& blocked_urls() const { return blocked_urls_; }

 protected:
  // A test-specific field for controlling when most visited callback is run
  // after top sites have been requested.
  std::list<GetMostVisitedURLsCallback> callbacks_;
  std::set<std::string> blocked_urls_;

  ~FakeTopSites() override = default;
};

constexpr const auto* WEB_URL = u"https://example.com/";

enum class ExpectedUiType { kAggregateMatch, kIndividualTiles };

const std::vector<TestData> DefaultTestData() {
  return {{false, {GURL("http://www.a.art/"), u"A art"}},
          {false, {GURL("http://www.b.biz/"), u"B biz"}},
          {false, {GURL("http://www.c.com/"), u"C com"}},
          {false, {GURL("http://www.d.de/"), u"D de"}},
          {true, {GURL("http://www.google.com/search?q=abc"), u"abc"}}};
}

class MockHistoryService : public history::HistoryService {
 public:
  MockHistoryService() = default;
  ~MockHistoryService() override = default;

  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              QueryMostVisitedURLs,
              (int result_count,
               history::HistoryService::QueryMostVisitedURLsCallback callback,
               base::CancelableTaskTracker* tracker,
               const std::optional<std::string>& recency_factor_name,
               std::optional<size_t> recency_window_days),
              (override));
  MOCK_METHOD(void, DeleteURLs, (const std::vector<GURL>& urls), (override));
};

}  // namespace

class MostVisitedSitesProviderTest : public testing::Test,
                                     public AutocompleteProviderListener {
 public:
  void SetUp() override;

 protected:
  // Construct AutocompleteInput object a hypothetical Omnibox session context.
  // Does not run any validation on the supplied values, allowing any
  // combination (including invalid ones) to be used to create AutocompleteInput
  // context object.
  AutocompleteInput BuildAutocompleteInput(
      const std::u16string& input_url,
      const std::u16string& current_url,
      metrics::OmniboxEventProto::PageClassification page_class,
      metrics::OmniboxFocusType focus_type) {
    AutocompleteInput input(input_url, page_class, TestSchemeClassifier());
    input.set_focus_type(focus_type);
    input.set_current_url(GURL(current_url));
    return input;
  }

  // Helper method, constructing a valid AutocompleteInput object for a website
  // visit.
  AutocompleteInput BuildAutocompleteInputForWebOnFocus() {
    return BuildAutocompleteInput(WEB_URL, WEB_URL,
                                  metrics::OmniboxEventProto::OTHER,
                                  metrics::OmniboxFocusType::INTERACTION_FOCUS);
  }

  // A prefetch AutocompleteInput on Web.
  AutocompleteInput BuildAutocompletePrefetchInputForWeb() {
    return BuildAutocompleteInput(
        WEB_URL, WEB_URL, metrics::OmniboxEventProto::OTHER_ZPS_PREFETCH,
        metrics::OmniboxFocusType::INTERACTION_FOCUS);
  }

  // Iterate over all matches offered by the Provider and verify these against
  // the supplied list of History URLs.
  void CheckMatchesEquivalentTo(const std::vector<TestData>& data,
                                ExpectedUiType ui_type);

  // Iterate over all matches offered by the Provider and verify these against
  // the supplied list of History URLs for desktop.
  void CheckDesktopMatchesEquivalentTo(const std::vector<TestData>& data,
                                       size_t url_limit);

  // Returns total number of all NAVSUGGEST and TILE_NAVSUGGEST elements.
  size_t NumMostVisitedMatches();

  // Returns the N-th match of a particular type, skipping over all matches of
  // other types. If match of that type does not exist, or there are not enough
  // elements of that type, this call returns null.
  const AutocompleteMatch* GetMatch(AutocompleteMatchType::Type type,
                                    size_t index);

  // AutocompleteProviderListener:
  void OnProviderUpdate(bool updated_matches,
                        const AutocompleteProvider* provider) override;

  base::HistogramTester histogram_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  FakeAutocompleteProviderClient client_;
  scoped_refptr<FakeTopSites> top_sites_;
  scoped_refptr<MostVisitedSitesProvider> provider_;
  int provider_update_count_{};
};

size_t MostVisitedSitesProviderTest::NumMostVisitedMatches() {
  const auto& result = provider_->matches();
  size_t count = 0;
  for (const auto& match : result) {
    if ((match.type == AutocompleteMatchType::TILE_NAVSUGGEST) ||
        (match.type == AutocompleteMatchType::NAVSUGGEST) ||
        (match.type == AutocompleteMatchType::TILE_MOST_VISITED_SITE) ||
        (match.type == AutocompleteMatchType::TILE_REPEATABLE_QUERY)) {
      ++count;
    }
  }
  return count;
}

const AutocompleteMatch* MostVisitedSitesProviderTest::GetMatch(
    AutocompleteMatchType::Type type,
    size_t index) {
  const auto& result = provider_->matches();
  for (const auto& match : result) {
    if (match.type == type) {
      if (!index)
        return &match;
      --index;
    }
  }
  return nullptr;
}

void MostVisitedSitesProviderTest::CheckMatchesEquivalentTo(
    const std::vector<TestData>& data,
    ExpectedUiType ui_type) {
  // Compare the AutocompleteResult against a set of URLs that we expect to see.
  // Note that additional matches may be offered if other providers are also
  // registered in the same category as MostVisitedSitesProvider.
  // We ignore all matches that are not ours.
  const auto& result = provider_->matches();

  size_t match_index = 0;

  if (ui_type == ExpectedUiType::kAggregateMatch) {
    ASSERT_EQ(1ul, NumMostVisitedMatches())
        << "Expected only one TILE_NAVSUGGEST match";
    for (const auto& match : result) {
      if (match.type != AutocompleteMatchType::TILE_NAVSUGGEST)
        continue;
      EXPECT_TRUE(match.subtypes.contains(
          omnibox::SUBTYPE_ZERO_PREFIX_LOCAL_FREQUENT_URLS));
      EXPECT_TRUE(match.subtypes.contains(omnibox::SUBTYPE_URL_BASED));
      const auto& tiles = match.suggest_tiles;
      ASSERT_EQ(data.size(), tiles.size()) << "Wrong number of tiles reported";
      for (size_t index = 0u; index < data.size(); index++) {
        EXPECT_EQ(data[index].entry.url, tiles[index].url)
            << "Invalid Tile URL at position " << index;
        EXPECT_EQ(data[index].entry.title, tiles[index].title)
            << "Invalid Tile Title at position " << index;
      }
      break;
    }
  } else if (ui_type == ExpectedUiType::kIndividualTiles) {
    ASSERT_EQ(data.size(), NumMostVisitedMatches())
        << "Unexpected number of TILE matches";
    int expected_relevance = omnibox::kMostVisitedTilesZeroSuggestHighRelevance;
    for (const auto& match : result) {
      if (data[match_index].is_search) {
        EXPECT_EQ(match.type, AutocompleteMatchType::TILE_REPEATABLE_QUERY);
        EXPECT_TRUE(match.subtypes.contains(
            omnibox::SUBTYPE_ZERO_PREFIX_LOCAL_FREQUENT_QUERIES));
      } else {
        EXPECT_EQ(match.type, AutocompleteMatchType::TILE_MOST_VISITED_SITE);
        EXPECT_TRUE(match.subtypes.contains(
            omnibox::SUBTYPE_ZERO_PREFIX_LOCAL_FREQUENT_URLS));
        EXPECT_TRUE(match.subtypes.contains(omnibox::SUBTYPE_URL_BASED));
      }

      EXPECT_EQ(data[match_index].entry.url, match.destination_url)
          << "Invalid Match URL at position " << match_index;
      EXPECT_EQ(data[match_index].entry.title, match.description)
          << "Invalid Match Title at position " << match_index;
      EXPECT_EQ(expected_relevance, match.relevance)
          << "Invalid Match Relevance at position " << match_index;
      ++match_index;
      --expected_relevance;
    }
  }
}

void MostVisitedSitesProviderTest::CheckDesktopMatchesEquivalentTo(
    const std::vector<TestData>& data,
    size_t url_limit) {
  // Compare the AutocompleteResult against a set of URLs that we expect to see.
  // Note that additional matches may be offered if other providers are also
  // registered in the same category as MostVisitedSitesProvider.
  // We ignore all matches that are not ours.
  const auto& result = provider_->matches();

  size_t match_index = 0;
  ASSERT_EQ(url_limit, NumMostVisitedMatches())
      << "Unexpected number of TILE matches";
  int expected_relevance = omnibox::kMostVisitedTilesZeroSuggestHighRelevance;
  for (const auto& match : result) {
    EXPECT_EQ(match.type, AutocompleteMatchType::TILE_MOST_VISITED_SITE);
    EXPECT_TRUE(match.subtypes.contains(
        omnibox::SUBTYPE_ZERO_PREFIX_LOCAL_FREQUENT_URLS));
    EXPECT_TRUE(match.subtypes.contains(omnibox::SUBTYPE_URL_BASED));

    EXPECT_EQ(data[match_index].entry.url, match.destination_url)
        << "Invalid Match URL at position " << match_index;
    EXPECT_EQ(data[match_index].entry.title, match.description)
        << "Invalid Match Title at position " << match_index;
    EXPECT_EQ(expected_relevance, match.relevance)
        << "Invalid Match Relevance at position " << match_index;
    EXPECT_EQ(omnibox::GROUP_MOST_VISITED, match.suggestion_group_id);
    ++match_index;
    --expected_relevance;
  }
}

void MostVisitedSitesProviderTest::SetUp() {
  top_sites_ = new FakeTopSites();

  client_.set_top_sites(top_sites_);

  // For tests requiring direct interaction with the Provider.
  provider_ = new MostVisitedSitesProvider(&client_, this);
}

void MostVisitedSitesProviderTest::OnProviderUpdate(
    bool updated_matches,
    const AutocompleteProvider* provider) {
  provider_update_count_++;
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

constexpr const auto* SRP_URL = u"https://www.google.com/?q=flowers";
constexpr const auto* FTP_URL = u"ftp://just.for.filtering.com";

TEST_F(MostVisitedSitesProviderTest, TestMostVisitedCallback) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(
      omnibox::kMostVisitedTilesHorizontalRenderGroup);
  auto input = BuildAutocompleteInputForWebOnFocus();
  provider_->Start(input, true);
  EXPECT_EQ(0u, NumMostVisitedMatches());
  auto test_data = DefaultTestData();
  EXPECT_TRUE(top_sites_->EmitURLs(test_data));
  CheckMatchesEquivalentTo(test_data, ExpectedUiType::kAggregateMatch);
  EXPECT_EQ(1, provider_update_count_);
  provider_->Stop(AutocompleteStopReason::kClobbered);

  // Observe that subsequent request does not return stale data.
  provider_->Start(input, true);
  provider_->Stop(AutocompleteStopReason::kClobbered);
  // Since this provider's async logic is still in-flight (`EmitURLs()` has not
  // been called yet), we should not be reporting anything from past runs.
  EXPECT_EQ(0ul, NumMostVisitedMatches());
  EXPECT_EQ(1, provider_update_count_);

  // Most visited results arriving after Stop() has been called, ensure they
  // are not displayed.
  std::vector<TestData> new_urls{{
      false,
      {GURL("http://www.g.gov/"), u"G gov"},
  }};
  EXPECT_TRUE(top_sites_->EmitURLs(new_urls));
  EXPECT_EQ(0ul, NumMostVisitedMatches());
  EXPECT_EQ(1, provider_update_count_);

  provider_->Start(input, true);
  provider_->Stop(AutocompleteStopReason::kClobbered);
  provider_->Start(input, true);

  // Stale results (reported for the first of the two Start() requests) should
  // be rejected.
  EXPECT_TRUE(top_sites_->EmitURLs(DefaultTestData()));
  EXPECT_EQ(0ul, NumMostVisitedMatches());
  EXPECT_EQ(1, provider_update_count_);

  // Results for the second Start() action should be recorded.
  EXPECT_TRUE(top_sites_->EmitURLs(test_data));
  CheckMatchesEquivalentTo(test_data, ExpectedUiType::kAggregateMatch);
  EXPECT_EQ(2, provider_update_count_);
  provider_->Stop(AutocompleteStopReason::kClobbered);
}

TEST_F(MostVisitedSitesProviderTest, TestMostVisitedNavigateToSearchPage) {
  provider_->Start(BuildAutocompleteInputForWebOnFocus(), true);
  EXPECT_EQ(0u, NumMostVisitedMatches());
  // Stop() doesn't always get called.

  auto srp_input = BuildAutocompleteInput(
      SRP_URL, SRP_URL,
      metrics::OmniboxEventProto::SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
      metrics::OmniboxFocusType::INTERACTION_FOCUS);

  provider_->Start(srp_input, true);
  EXPECT_EQ(0u, NumMostVisitedMatches());

  // Most visited results arriving after a new request has been started.
  EXPECT_TRUE(top_sites_->EmitURLs(DefaultTestData()));
  EXPECT_EQ(0u, NumMostVisitedMatches());
}

TEST_F(MostVisitedSitesProviderTest, AllowMostVisitedSitesSuggestions) {
  // MostVisited should never deal with prefix suggestions.
  EXPECT_FALSE(provider_->AllowMostVisitedSitesSuggestions(
      &client_, BuildAutocompleteInput(WEB_URL, WEB_URL, OEP::OTHER,
                                       OFT::INTERACTION_DEFAULT)));

  // This should always be true, as otherwise we will break MostVisited.
  EXPECT_TRUE(provider_->AllowMostVisitedSitesSuggestions(
      &client_, BuildAutocompleteInput(WEB_URL, WEB_URL, OEP::OTHER,
                                       OFT::INTERACTION_FOCUS)));

  // Verifies that non-permitted schemes are rejected.
  EXPECT_FALSE(provider_->AllowMostVisitedSitesSuggestions(
      &client_, BuildAutocompleteInput(FTP_URL, FTP_URL, OEP::OTHER,
                                       OFT::INTERACTION_FOCUS)));

  // Offer MV sites when the User is visiting a website and deletes text.
  EXPECT_TRUE(provider_->AllowMostVisitedSitesSuggestions(
      &client_, BuildAutocompleteInput(WEB_URL, WEB_URL, OEP::OTHER,
                                       OFT::INTERACTION_FOCUS)));
}

TEST_F(MostVisitedSitesProviderTest, NoSRPCoverage) {
  EXPECT_FALSE(provider_->AllowMostVisitedSitesSuggestions(
      &client_,
      BuildAutocompleteInput(WEB_URL, WEB_URL,
                             OEP::SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
                             OFT::INTERACTION_FOCUS)));
}

TEST_F(MostVisitedSitesProviderTest, TestCreateMostVisitedMatch) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(
      omnibox::kMostVisitedTilesHorizontalRenderGroup);
  provider_->Start(BuildAutocompleteInputForWebOnFocus(), true);
  EXPECT_EQ(0u, NumMostVisitedMatches());
  // Accept only direct TopSites data.
  auto test_data = DefaultTestData();
  EXPECT_TRUE(top_sites_->EmitURLs(test_data));
  CheckMatchesEquivalentTo(test_data, ExpectedUiType::kAggregateMatch);
}

TEST_F(MostVisitedSitesProviderTest, NoMatchesWhenNoMostVisitedSites) {
  // Start with no URLs.
  provider_->Start(BuildAutocompleteInputForWebOnFocus(), true);
  EXPECT_EQ(0u, NumMostVisitedMatches());
  // Accept only direct TopSites data, confirm no matches are built.
  EXPECT_TRUE(top_sites_->EmitURLs({}));
  EXPECT_EQ(0u, NumMostVisitedMatches());
}

TEST_F(MostVisitedSitesProviderTest,
       NoMatchesWhenTopSitesNotLoadedAndWantAsyncMatchesFalse) {
  // Assume that top sites list has not been loaded yet from the DB.
  ASSERT_FALSE(top_sites_->loaded());
  auto input = BuildAutocompleteInputForWebOnFocus();
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_DEFAULT);
  input.set_omit_asynchronous_matches(true);
  provider_->Start(input, true);
  EXPECT_TRUE(provider_->done());
  EXPECT_EQ(0u, NumMostVisitedMatches());
  // No callbacks should have been added due to early return.
  EXPECT_FALSE(top_sites_->EmitURLs(DefaultTestData()));
  EXPECT_EQ(0u, NumMostVisitedMatches());
}

TEST_F(MostVisitedSitesProviderTest, TestDeleteMostVisitedElement) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(
      omnibox::kMostVisitedTilesHorizontalRenderGroup);
  // Make a copy (intentional - we'll modify this later)
  provider_->Start(BuildAutocompleteInputForWebOnFocus(), true);
  // Accept only direct TopSites data.
  auto test_data = DefaultTestData();
  EXPECT_TRUE(top_sites_->EmitURLs(test_data));
  CheckMatchesEquivalentTo(test_data, ExpectedUiType::kAggregateMatch);

  // Commence delete.
  histogram_.ExpectTotalCount("Omnibox.SuggestTiles.TileTypeCount.Search", 1);
  histogram_.ExpectBucketCount("Omnibox.SuggestTiles.TileTypeCount.Search", 1,
                               1);
  histogram_.ExpectTotalCount("Omnibox.SuggestTiles.TileTypeCount.URL", 1);
  histogram_.ExpectBucketCount("Omnibox.SuggestTiles.TileTypeCount.URL", 4, 1);
  histogram_.ExpectTotalCount("Omnibox.SuggestTiles.DeletedTileIndex", 0);
  auto* match = GetMatch(AutocompleteMatchType::TILE_NAVSUGGEST, 0);
  ASSERT_NE(nullptr, match) << "No TILE_NAVSUGGEST Match found";
  provider_->DeleteMatchElement(*match, 1);
  histogram_.ExpectTotalCount("Omnibox.SuggestTiles.DeletedTileIndex", 1);
  histogram_.ExpectBucketCount("Omnibox.SuggestTiles.DeletedTileIndex", 1, 1);
  // Note: TileTypeCounts are not emitted after deletion.

  // Observe that the URL is now blocked and removed from suggestion.
  auto deleted_url = test_data[1].entry.url;
  test_data.erase(test_data.begin() + 1);
  CheckMatchesEquivalentTo(test_data, ExpectedUiType::kAggregateMatch);
  EXPECT_TRUE(top_sites_->IsBlocked(deleted_url));
}

TEST_F(MostVisitedSitesProviderTest, NoMatchesWhenLastURLIsDeleted) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(
      omnibox::kMostVisitedTilesHorizontalRenderGroup);

  // Start with just one URL.
  std::vector<TestData> urls{{
      {false, {GURL("http://www.a.art/"), u"A art"}},
  }};

  provider_->Start(BuildAutocompleteInputForWebOnFocus(), true);
  EXPECT_TRUE(top_sites_->EmitURLs(urls));
  CheckMatchesEquivalentTo(urls, ExpectedUiType::kAggregateMatch);

  // Commence delete of the only item that we have.
  histogram_.ExpectTotalCount("Omnibox.SuggestTiles.TileTypeCount.Search", 1);
  histogram_.ExpectBucketCount("Omnibox.SuggestTiles.TileTypeCount.Search", 0,
                               1);
  histogram_.ExpectTotalCount("Omnibox.SuggestTiles.TileTypeCount.URL", 1);
  histogram_.ExpectBucketCount("Omnibox.SuggestTiles.TileTypeCount.URL", 1, 1);
  histogram_.ExpectTotalCount("Omnibox.SuggestTiles.DeletedTileIndex", 0);
  auto* match = GetMatch(AutocompleteMatchType::TILE_NAVSUGGEST, 0);
  ASSERT_NE(nullptr, match) << "No TILE_NAVSUGGEST Match found";
  provider_->DeleteMatchElement(*match, 0);
  histogram_.ExpectTotalCount("Omnibox.SuggestTiles.DeletedTileIndex", 1);
  histogram_.ExpectBucketCount("Omnibox.SuggestTiles.DeletedTileIndex", 0, 1);
  // Note: TileTypeCounts are not emitted after deletion.

  // Confirm no more NAVSUGGEST matches are offered.
  EXPECT_EQ(0u, NumMostVisitedMatches());
}

TEST_F(MostVisitedSitesProviderTest,
       TestCreateMostVisitedHorizontalGroupTiles) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({omnibox::kMostVisitedTilesHorizontalRenderGroup},
                            {});

  provider_->Start(BuildAutocompleteInputForWebOnFocus(), true);
  EXPECT_EQ(0u, NumMostVisitedMatches());
  // Accept only direct TopSites data.
  auto test_data = DefaultTestData();
  EXPECT_TRUE(top_sites_->EmitURLs(test_data));
  CheckMatchesEquivalentTo(test_data, ExpectedUiType::kIndividualTiles);
}

#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

#if !(BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS))
TEST_F(MostVisitedSitesProviderTest, TestCreateMostVisitedTopSitesMatches) {
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::OmniboxUrlSuggestionsOnFocus>
      scoped_config;
  scoped_config.Get().enabled = true;
  scoped_config.Get().directly_query_history_service = false;
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::OmniboxZpsSuggestionLimit>
      suggestion_limit_scoped_config;
  suggestion_limit_scoped_config.Get().enabled = true;
  suggestion_limit_scoped_config.Get().max_suggestions = 8U;
  suggestion_limit_scoped_config.Get().max_url_suggestions = 4U;
  suggestion_limit_scoped_config.Get().max_search_suggestions = 4U;

  provider_->Start(BuildAutocompleteInputForWebOnFocus(), true);

  EXPECT_EQ(0u, NumMostVisitedMatches());
  // Accept only direct TopSites data.
  auto test_data = DefaultTestData();
  EXPECT_TRUE(top_sites_->EmitURLs(test_data));
  CheckDesktopMatchesEquivalentTo(
      test_data, suggestion_limit_scoped_config.Get().max_url_suggestions);
}

TEST_F(MostVisitedSitesProviderTest, DesktopProviderDoesNotAllowChromeSites) {
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::OmniboxUrlSuggestionsOnFocus>
      scoped_config;
  scoped_config.Get().enabled = true;

  const auto* chrome_url = u"chrome://omnibox";

  // Verifies that non-permitted schemes are rejected.
  EXPECT_TRUE(provider_->AllowMostVisitedSitesSuggestions(
      &client_, BuildAutocompleteInputForWebOnFocus()));

  // Verifies that non-permitted schemes are rejected.
  EXPECT_FALSE(provider_->AllowMostVisitedSitesSuggestions(
      &client_, BuildAutocompleteInput(chrome_url, chrome_url, OEP::OTHER,
                                       OFT::INTERACTION_FOCUS)));
}

TEST_F(MostVisitedSitesProviderTest, TestDesktopQueryingHistoryService) {
  // Set a MockHistoryService.
  auto history_service = std::make_unique<MockHistoryService>();
  auto& history_service_ref = *history_service;
  client_.set_history_service(std::move(history_service));
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::OmniboxUrlSuggestionsOnFocus>
      scoped_config;
  scoped_config.Get().enabled = true;
  scoped_config.Get().most_visited_recency_window = 4;
  scoped_config.Get().most_visited_recency_factor =
      history::kMvtScoringParamRecencyFactor_Classic;
  scoped_config.Get().prefetch_most_visited_sites = false;
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::OmniboxZpsSuggestionLimit>
      suggestion_limit_scoped_config;
  suggestion_limit_scoped_config.Get().enabled = true;
  suggestion_limit_scoped_config.Get().max_suggestions = 8U;

  AutocompleteInput input(BuildAutocompleteInputForWebOnFocus());

  history::HistoryService::QueryMostVisitedURLsCallback callback;
  EXPECT_CALL(history_service_ref, QueryMostVisitedURLs(_, _, _, _, _))
      .WillOnce([&](int result_count,
                    history::HistoryService::QueryMostVisitedURLsCallback cb,
                    base::CancelableTaskTracker* tracker,
                    std::optional<std::string> recency_factor_name,
                    std::optional<size_t> recency_window_days)
                    -> base::CancelableTaskTracker::TaskId {
        // Add 1 to simulate 1 site being open.
        EXPECT_EQ(static_cast<int>(provider_->GetRequestedResultSize(input)),
                  result_count);
        EXPECT_TRUE(recency_factor_name.has_value());
        EXPECT_EQ(history::kMvtScoringParamRecencyFactor_Classic,
                  recency_factor_name.value());
        EXPECT_TRUE(recency_window_days.has_value());
        EXPECT_EQ(4u, recency_window_days.value());
        callback = std::move(cb);
        return {};
      });
  provider_->Start(input, false);
  EXPECT_FALSE(provider_->done());
  history::MostVisitedURLList result;
  for (const auto& test_element : DefaultTestData()) {
    result.push_back(test_element.entry);
  }
  std::move(callback).Run(std::move(result));
  // Shouldn't include the match from the SRP.
  EXPECT_TRUE(provider_->done());
  ASSERT_EQ(4u, provider_->matches().size());
}

TEST_F(MostVisitedSitesProviderTest, TestDeleteMatch) {
  // Set a MockHistoryService.
  auto history_service = std::make_unique<MockHistoryService>();
  auto& history_service_ref = *history_service;
  client_.set_history_service(std::move(history_service));
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::OmniboxUrlSuggestionsOnFocus>
      scoped_config;
  scoped_config.Get().enabled = true;
  scoped_config.Get().prefetch_most_visited_sites = false;
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::OmniboxZpsSuggestionLimit>
      suggestion_limit_scoped_config;
  suggestion_limit_scoped_config.Get().enabled = true;
  suggestion_limit_scoped_config.Get().max_suggestions = 8U;

  AutocompleteInput input(BuildAutocompleteInputForWebOnFocus());

  history::HistoryService::QueryMostVisitedURLsCallback callback;
  EXPECT_CALL(history_service_ref, QueryMostVisitedURLs(_, _, _, _, _))
      .WillOnce([&](int result_count,
                    history::HistoryService::QueryMostVisitedURLsCallback cb,
                    base::CancelableTaskTracker* tracker,
                    std::optional<std::string> recency_factor_name,
                    std::optional<size_t> recency_window_days)
                    -> base::CancelableTaskTracker::TaskId {
        // Add 1 to simulate 1 site being open.
        EXPECT_EQ(static_cast<int>(provider_->GetRequestedResultSize(input)),
                  result_count);
        callback = std::move(cb);
        return {};
      });

  provider_->Start(input, false);
  histogram_.ExpectTotalCount(
      "Omnibox.MostVisitedProvider.QueryMostVisitedURLs.NumRequested", 1);
  EXPECT_FALSE(provider_->done());
  history::MostVisitedURLList result;
  for (const auto& test_element : DefaultTestData()) {
    result.push_back(test_element.entry);
  }

  std::move(callback).Run(std::move(result));
  histogram_.ExpectTotalCount(
      "Omnibox.MostVisitedProvider.QueryMostVisitedURLs.NumReceived", 1);
  histogram_.ExpectTotalCount(
      "Omnibox.MostVisitedProvider.QueryMostVisitedURLs.Duration", 1);

  // Shouldn't include the match from the SRP.
  EXPECT_TRUE(provider_->done());
  ASSERT_EQ(4u, provider_->matches().size());

  EXPECT_CALL(history_service_ref, DeleteURLs(_))
      .WillOnce([&](const std::vector<GURL>& urls) -> void {
        EXPECT_FALSE(urls.empty());
        EXPECT_EQ(provider_->matches().at(0).destination_url, urls.at(0));
        return;
      });

  provider_->DeleteMatch(provider_->matches().at(0));

  ASSERT_EQ(3u, provider_->matches().size());
}

TEST_F(MostVisitedSitesProviderTest, PrefetchingUpdatesCachedSites) {
  // Set a MockHistoryService.
  auto history_service = std::make_unique<MockHistoryService>();
  auto& history_service_ref = *history_service;
  client_.set_history_service(std::move(history_service));

  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::OmniboxUrlSuggestionsOnFocus>
      scoped_config;
  scoped_config.Get().enabled = true;
  scoped_config.Get().prefetch_most_visited_sites = true;
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::OmniboxZpsSuggestionLimit>
      suggestion_limit_scoped_config;
  suggestion_limit_scoped_config.Get().enabled = true;
  suggestion_limit_scoped_config.Get().max_suggestions = 8U;

  AutocompleteInput input(BuildAutocompletePrefetchInputForWeb());

  history::HistoryService::QueryMostVisitedURLsCallback callback;
  EXPECT_CALL(history_service_ref, QueryMostVisitedURLs(_, _, _, _, _))
      .Times(2)
      .WillRepeatedly(
          [&](int result_count,
              history::HistoryService::QueryMostVisitedURLsCallback cb,
              base::CancelableTaskTracker* tracker,
              std::optional<std::string> recency_factor_name,
              std::optional<size_t> recency_window_days)
              -> base::CancelableTaskTracker::TaskId {
            // Add 1 to simulate 1 site being open.
            EXPECT_EQ(
                static_cast<int>(provider_->GetRequestedResultSize(input)),
                result_count);
            callback = std::move(cb);
            return {};
          });

  // `StartPrefetch()` shouldn't affect provider state.
  EXPECT_TRUE(provider_->done());
  provider_->StartPrefetch(input);
  // Number of sites requested should be logged at the beginnign of the history
  // query.
  histogram_.ExpectTotalCount(
      "Omnibox.MostVisitedProvider.QueryMostVisitedURLs.NumRequested", 1);
  EXPECT_TRUE(provider_->done());
  history::MostVisitedURLList result;
  for (const auto& test_element : DefaultTestData()) {
    result.push_back(test_element.entry);
  }

  // Updates cache.
  std::move(callback).Run(std::move(result));
  // Duration should be logged when cache is updated.
  histogram_.ExpectTotalCount(
      "Omnibox.MostVisitedProvider.QueryMostVisitedURLs.Duration", 1);
  histogram_.ExpectTotalCount(
      "Omnibox.MostVisitedProvider.QueryMostVisitedURLs.NumReceived", 1);

  // `StartPrefetch()` should update the list of cached sites, but not
  // update the actual provider's matches.
  ASSERT_EQ(5u, provider_->GetCachedSitesForTesting().size());
  ASSERT_EQ(0u, provider_->matches().size());

  // Calling Start() should use the cached sites to create matches. Shouldn't
  // include the SRP result.
  provider_->Start(input, false);
  ASSERT_EQ(4u, provider_->matches().size());
  // Duration shouldn't be recorded again when results are returned
  // synchronously.
  histogram_.ExpectTotalCount(
      "Omnibox.MostVisitedProvider.QueryMostVisitedURLs.Duration", 1);
  histogram_.ExpectTotalCount(
      "Omnibox.MostVisitedProvider.QueryMostVisitedURLs.NumReceived", 1);
}

TEST_F(MostVisitedSitesProviderTest,
       StartDoesNotUpdateMatchesWhenPrefetchEnabled) {
  // Set a MockHistoryService.
  auto history_service = std::make_unique<MockHistoryService>();
  auto& history_service_ref = *history_service;
  client_.set_history_service(std::move(history_service));
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::OmniboxUrlSuggestionsOnFocus>
      scoped_config;
  scoped_config.Get().enabled = true;
  scoped_config.Get().prefetch_most_visited_sites = true;
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::OmniboxZpsSuggestionLimit>
      suggestion_limit_scoped_config;
  suggestion_limit_scoped_config.Get().enabled = true;
  suggestion_limit_scoped_config.Get().max_suggestions = 8U;

  AutocompleteInput input(BuildAutocompleteInputForWebOnFocus());

  history::HistoryService::QueryMostVisitedURLsCallback callback;
  EXPECT_CALL(history_service_ref, QueryMostVisitedURLs(_, _, _, _, _))
      .WillOnce([&](int result_count,
                    history::HistoryService::QueryMostVisitedURLsCallback cb,
                    base::CancelableTaskTracker* tracker,
                    std::optional<std::string> recency_factor_name,
                    std::optional<size_t> recency_window_days)
                    -> base::CancelableTaskTracker::TaskId {
        // Add 1 to simulate 1 site being open.
        EXPECT_EQ(static_cast<int>(provider_->GetRequestedResultSize(input)),
                  result_count);
        callback = std::move(cb);
        return {};
      });

  provider_->Start(input, false);
  history::MostVisitedURLList result;
  for (const auto& test_element : DefaultTestData()) {
    result.push_back(test_element.entry);
  }

  std::move(callback).Run(std::move(result));

  EXPECT_TRUE(provider_->done());
  // `Start()` when prefetch is enabled should update the list of cached sites,
  // but not update the actual provider's matches if the cache is empty.
  ASSERT_EQ(5u, provider_->GetCachedSitesForTesting().size());
  ASSERT_EQ(0u, provider_->matches().size());
}

TEST_F(MostVisitedSitesProviderTest, BlocklistedURLs) {
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::OmniboxUrlSuggestionsOnFocus>
      scoped_config;
  scoped_config.Get().enabled = true;
  AutocompleteInput input(BuildAutocompleteInputForWebOnFocus());
  history::MostVisitedURLList result;
  std::vector<TestData> test_data = {
      {false, {GURL("http://www.nonblocked.com"), u"Non blocked"}},
      {false, {GURL("http://accounts.google.com/signin"), u"Blocked"}}};
  result.push_back(test_data[0].entry);
  result.push_back(test_data[1].entry);
  provider_->OnMostVisitedUrlsAvailable(input, result);
  // Shouldn't include the "accounts.google.com" URL since it is blocklisted.
  ASSERT_EQ(1u, provider_->matches().size());
  ASSERT_EQ("http://www.nonblocked.com/",
            provider_->matches().at(0).destination_url.spec());
}

TEST_F(MostVisitedSitesProviderTest, TestDeleteWithPrefetching) {
  // Set a MockHistoryService.
  auto history_service = std::make_unique<MockHistoryService>();
  auto& history_service_ref = *history_service;
  client_.set_history_service(std::move(history_service));
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::OmniboxUrlSuggestionsOnFocus>
      scoped_config;
  scoped_config.Get().enabled = true;
  scoped_config.Get().prefetch_most_visited_sites = true;
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::OmniboxZpsSuggestionLimit>
      suggestion_limit_scoped_config;
  suggestion_limit_scoped_config.Get().enabled = true;
  suggestion_limit_scoped_config.Get().max_suggestions = 8U;

  history::HistoryService::QueryMostVisitedURLsCallback callback;
  EXPECT_CALL(history_service_ref, QueryMostVisitedURLs(_, _, _, _, _))
      .WillRepeatedly(
          [&](int result_count,
              history::HistoryService::QueryMostVisitedURLsCallback cb,
              base::CancelableTaskTracker* tracker,
              std::optional<std::string> recency_factor_name,
              std::optional<size_t> recency_window_days)
              -> base::CancelableTaskTracker::TaskId {
            callback = std::move(cb);
            return {};
          });

  AutocompleteInput input(BuildAutocompleteInputForWebOnFocus());
  provider_->StartPrefetch(input);
  history::MostVisitedURLList result;

  // Populates cached sites.
  for (const auto& test_element : DefaultTestData()) {
    result.push_back(test_element.entry);
  }
  std::move(callback).Run(std::move(result));
  ASSERT_EQ(5u, provider_->GetCachedSitesForTesting().size());

  // Shouldn't include the match from the SRP.
  EXPECT_TRUE(provider_->done());
  ASSERT_EQ(0u, provider_->matches().size());

  EXPECT_CALL(history_service_ref, DeleteURLs(_))
      .WillOnce([&](const std::vector<GURL>& urls) -> void {
        EXPECT_FALSE(urls.empty());
        EXPECT_EQ(provider_->matches().at(0).destination_url, urls.at(0));
        return;
      });

  // Calling `Start()` will use the list of cached sites, but won't include the
  // SRP site..
  provider_->Start(input, false);
  ASSERT_EQ(4u, provider_->matches().size());

  provider_->DeleteMatch(provider_->matches().at(0));

  // `DeleteMatch()` should remove from both the cached sites list and from
  // `matches_`.
  ASSERT_EQ(3u, provider_->matches().size());
  ASSERT_EQ(4u, provider_->GetCachedSitesForTesting().size());

  // Calling `Start()` uses the `cached_sites_`, which should now only return
  // 3 valid sites.
  provider_->Start(input, false);
  ASSERT_EQ(3u, provider_->matches().size());
}

TEST_F(MostVisitedSitesProviderTest, DuplicateSuggestions) {
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::OmniboxUrlSuggestionsOnFocus>
      scoped_config;
  scoped_config.Get().enabled = true;
  std::vector<TestData> test_data = {
      {false, {GURL("http://www.mail.com"), u"Mail"}},
      {false, {GURL("http://different.mail.com/signin"), u"Mail"}},
      {false, {GURL("http://www.samesites.com/c/#ref"), u"Same URL"}},
      {false,
       {GURL("http://www.samesites.com/c/#differentref"), u"Same URL 2"}},
      {false,
       {GURL("http://www.samesites.com/differentpath/#ref"),
        u"Different URL"}}};
  history::MostVisitedURLList result;
  for (auto& data : test_data) {
    result.push_back(data.entry);
  }
  AutocompleteInput input(BuildAutocompleteInputForWebOnFocus());
  provider_->OnMostVisitedUrlsAvailable(input, result);
  // Filter by same titles and stripped urls when deduping suggestions within
  // the suggestion list.
  ASSERT_EQ(3u, provider_->matches().size());
  ASSERT_EQ("http://www.mail.com/",
            provider_->matches().at(0).destination_url.spec());
  ASSERT_EQ("http://www.samesites.com/c/#ref",
            provider_->matches().at(1).destination_url.spec());
  ASSERT_EQ("http://www.samesites.com/differentpath/#ref",
            provider_->matches().at(2).destination_url.spec());
}

TEST_F(MostVisitedSitesProviderTest, TestProviderDoneWithEmptyCachedSites) {
  // Set a MockHistoryService.
  auto history_service = std::make_unique<MockHistoryService>();
  auto& history_service_ref = *history_service;
  client_.set_history_service(std::move(history_service));
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::OmniboxUrlSuggestionsOnFocus>
      scoped_config;
  scoped_config.Get().enabled = true;
  scoped_config.Get().prefetch_most_visited_sites = true;

  history::HistoryService::QueryMostVisitedURLsCallback callback;
  EXPECT_CALL(history_service_ref, QueryMostVisitedURLs(_, _, _, _, _))
      .WillRepeatedly(
          [&](int result_count,
              history::HistoryService::QueryMostVisitedURLsCallback cb,
              base::CancelableTaskTracker* tracker,
              std::optional<std::string> recency_factor_name,
              std::optional<size_t> recency_window_days)
              -> base::CancelableTaskTracker::TaskId {
            callback = std::move(cb);
            return {};
          });

  AutocompleteInput input(BuildAutocompleteInputForWebOnFocus());
  // Simulate a prefetch that returns no results.
  provider_->StartPrefetch(input);
  history::MostVisitedURLList result;
  std::move(callback).Run(std::move(result));
  ASSERT_EQ(0u, provider_->GetCachedSitesForTesting().size());

  // Provider should still be set to done immediately after calling `Start()`
  // even if cached sites is empty.
  provider_->Start(input, false);
  EXPECT_TRUE(provider_->done());
}

TEST_F(MostVisitedSitesProviderTest, DedupingOpenTabs) {
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::OmniboxUrlSuggestionsOnFocus>
      scoped_config;
  scoped_config.Get().enabled = true;

  AutocompleteInput input(BuildAutocompleteInputForWebOnFocus());
  history::MostVisitedURLList result;

  FakeTabMatcher& tab_matcher = static_cast<FakeTabMatcher&>(
      const_cast<TabMatcher&>(client_.GetTabMatcher()));

  tab_matcher.AddOpenTab(FakeTabMatcher::TabWrapper(
      u"Test", GURL("http://foo.org/path/#ref"), base::Time::Now()));
  tab_matcher.AddOpenTab(FakeTabMatcher::TabWrapper(
      u"Testing", GURL("http://bar.org"), base::Time::Now()));

  std::vector<TestData> test_data = {
      // Tabs with same title and url should be considered matches.
      {false, {GURL("http://bar.org"), u"Testing"}},
      // Tabs with same title should be considered matches even if urls are
      // different.
      {false, {GURL("http://different.org"), u"Testing"}},
      // Sites with same path and different refs should be considered matches.
      {false, {GURL("http://foo.org/path/#differentref"), u"Different Name"}},
      // Sites with different path should not be considered matches.
      {false, {GURL("http://foo.org/differentpath/#ref"), u"Different Name"}},
      // Sites with same match and different refs and query params should be
      // considered matches.
      {false, {GURL("http://foo.org/path/#ref?param=123"), u"Different Name"}}};
  for (auto& data : test_data) {
    result.push_back(data.entry);
  }
  provider_->OnMostVisitedUrlsAvailable(input, result);
  ASSERT_EQ(1u, provider_->matches().size());
  ASSERT_EQ("http://foo.org/differentpath/#ref",
            provider_->matches().at(0).destination_url.spec());
}

#endif  // !(BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS))
