// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/most_visited_sites_provider.h"

#include <list>
#include <map>
#include <memory>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/history/core/browser/top_sites.h"
#include "components/ntp_tiles/icon_cacher.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/omnibox_focus_type.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

namespace {
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

  // RefcountedKeyedService:
  void ShutdownOnUIThread() override {}

  // Only runs a single callback, so that the test can specify a different
  // set per call.
  // Returns true if there was a recipient to receive the URLs and the list was
  // emitted, otherwise returns false.
  bool EmitURLs() {
    if (callbacks_.empty())
      return false;
    std::move(callbacks_.front()).Run(std::move(urls_));
    callbacks_.pop_front();
    return true;
  }

  history::MostVisitedURLList& urls() { return urls_; }
  const std::set<std::string>& blocked_urls() const { return blocked_urls_; }

 protected:
  // A test-specific field for controlling when most visited callback is run
  // after top sites have been requested.
  std::list<GetMostVisitedURLsCallback> callbacks_;
  history::MostVisitedURLList urls_;
  std::set<std::string> blocked_urls_;

  ~FakeTopSites() override = default;
};

constexpr const auto* WEB_URL = u"https://example.com/";
constexpr const auto* NTP_URL = u"chrome://newtab";
constexpr const auto* SRP_URL = u"https://www.google.com/?q=flowers";
constexpr const auto* FTP_URL = u"ftp://just.for.filtering.com";
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
      OmniboxFocusType focus_type) {
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
                                  OmniboxFocusType::ON_FOCUS);
  }

  // Iterate over all matches offered by the Provider and verify these against
  // the supplied list of History URLs.
  void CheckMatchesEquivalentTo(const history::MostVisitedURLList& urls,
                                bool expect_tiles);

  // Returns total number of all NAVSUGGEST and TILE_NAVSUGGEST elements.
  size_t NumMostVisitedMatches();

  // Returns the N-th match of a particular type, skipping over all matches of
  // other types. If match of that type does not exist, or there are not enough
  // elements of that type, this call returns null.
  const AutocompleteMatch* GetMatch(AutocompleteMatchType::Type type,
                                    size_t index);

  // AutocompleteProviderListener:
  void OnProviderUpdate(bool updated_matches) override;

  std::unique_ptr<base::test::SingleThreadTaskEnvironment> task_environment_;
  scoped_refptr<FakeTopSites> top_sites_;
  scoped_refptr<FakeTopSites> top_sites_for_start_surface_;
  scoped_refptr<MostVisitedSitesProvider> provider_;
  base::test::ScopedFeatureList features_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  std::unique_ptr<ntp_tiles::MostVisitedSites> ntp_top_sites_;
  std::unique_ptr<AutocompleteController> controller_;
};

size_t MostVisitedSitesProviderTest::NumMostVisitedMatches() {
  const auto& result = controller_->result();
  size_t count = 0;
  for (const auto& match : result) {
    if ((match.type == AutocompleteMatchType::TILE_NAVSUGGEST) ||
        (match.type == AutocompleteMatchType::NAVSUGGEST)) {
      ++count;
    }
  }
  return count;
}

const AutocompleteMatch* MostVisitedSitesProviderTest::GetMatch(
    AutocompleteMatchType::Type type,
    size_t index) {
  const auto& result = controller_->result();
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
    const history::MostVisitedURLList& urls,
    bool expect_tiles) {
  // Compare the AutocompleteResult against a set of URLs that we expect to see.
  // Note that additional matches may be offered if other providers are also
  // registered in the same category as MostVisitedSitesProvider.
  // We ignore all matches that are not ours.
  const auto& result = controller_->result();

  size_t match_index = 0;

  if (expect_tiles) {
    ASSERT_EQ(1ul, NumMostVisitedMatches())
        << "Expected only one TILE_NAVSUGGEST match";
    for (const auto& match : result) {
      if (match.type != AutocompleteMatchType::TILE_NAVSUGGEST)
        continue;
      const auto& tiles = match.navsuggest_tiles;
      ASSERT_EQ(urls.size(), tiles.size()) << "Wrong number of tiles reported";
      for (size_t index = 0u; index < urls.size(); index++) {
        EXPECT_EQ(urls[index].url, tiles[index].url)
            << "Invalid Tile URL at position " << index;
        EXPECT_EQ(urls[index].title, tiles[index].title)
            << "Invalid Tile Title at position " << index;
      }
      break;
    }
  } else {
    ASSERT_EQ(urls.size(), NumMostVisitedMatches())
        << "Unexpected number of NAVSUGGEST matches";
    for (const auto& match : result) {
      if (match.type != AutocompleteMatchType::NAVSUGGEST)
        continue;

      EXPECT_EQ(urls[match_index].url, match.destination_url)
          << "Invalid Match URL at position " << match_index;
      EXPECT_EQ(urls[match_index].title, match.description)
          << "Invalid Match Title at position " << match_index;
      ++match_index;
    }
  }
}

void MostVisitedSitesProviderTest::SetUp() {
  task_environment_ =
      std::make_unique<base::test::SingleThreadTaskEnvironment>();
  ntp_tiles::MostVisitedSites::RegisterProfilePrefs(pref_service_.registry());
  top_sites_ = new FakeTopSites();
  top_sites_for_start_surface_ = new FakeTopSites();

  // Note: std::make_unique<> fails here because it is unable to deduce argument
  // types.
  ntp_top_sites_.reset(new ntp_tiles::MostVisitedSites(
      &pref_service_, top_sites_for_start_surface_, {}, {}, {}, {}, true));
  auto client = std::make_unique<FakeAutocompleteProviderClient>();
  client->set_top_sites(top_sites_);
  client->set_ntp_most_visited_sites(ntp_top_sites_.get());

  // For tests requiring direct interaction with the Provider.
  provider_ = new MostVisitedSitesProvider(client.get(), this);

  // For tests not requiring direct interaction with the Provider.
  controller_ = std::make_unique<AutocompleteController>(
      std::move(client), AutocompleteProvider::TYPE_MOST_VISITED_SITES);

  // Inject a few URLs to
  std::array<history::MostVisitedURL, 5> test_data{{
      {GURL("http://www.a.art/"), u"A art"},
      {GURL("http://www.b.biz/"), u"B biz"},
      {GURL("http://www.c.com/"), u"C com"},
      {GURL("http://www.d.de/"), u"D de"},
      {GURL("http://www.e.edu/"), u"E edu"},
  }};

  top_sites_->urls().assign(test_data.begin(), test_data.end());
  top_sites_for_start_surface_->urls().assign(test_data.rbegin(),
                                              test_data.rend());
}

void MostVisitedSitesProviderTest::OnProviderUpdate(bool updated_matches) {}

class MostVisitedSitesProviderWithMatchesTest
    : public MostVisitedSitesProviderTest {
 public:
  void SetUp() override {
    features_.InitAndDisableFeature(omnibox::kMostVisitedTiles);
    MostVisitedSitesProviderTest::SetUp();
  }
};

TEST_F(MostVisitedSitesProviderWithMatchesTest, TestMostVisitedCallback) {
  auto input = BuildAutocompleteInputForWebOnFocus();
  controller_->Start(input);
  EXPECT_EQ(0u, NumMostVisitedMatches());
  EXPECT_TRUE(top_sites_->EmitURLs());
  CheckMatchesEquivalentTo(top_sites_->urls(), /* expect_tiles=*/false);
  controller_->Stop(false);

  controller_->Start(input);
  controller_->Stop(false);
  EXPECT_EQ(0u, NumMostVisitedMatches());

  // Most visited results arriving after Stop() has been called, ensure they
  // are not displayed.
  EXPECT_TRUE(top_sites_->EmitURLs());
  EXPECT_EQ(0u, NumMostVisitedMatches());

  controller_->Start(input);
  controller_->Stop(false);
  controller_->Start(input);

  // Stale results should get rejected.
  EXPECT_TRUE(top_sites_->EmitURLs());
  EXPECT_EQ(0u, NumMostVisitedMatches());

  // Results for the second Start() action should be recorded.
  EXPECT_TRUE(top_sites_->EmitURLs());
  EXPECT_EQ(top_sites_->urls().size(), NumMostVisitedMatches());
  controller_->Stop(false);
}

TEST_F(MostVisitedSitesProviderWithMatchesTest,
       TestMostVisitedNavigateToSearchPage) {
  controller_->Start(BuildAutocompleteInputForWebOnFocus());
  EXPECT_EQ(0u, NumMostVisitedMatches());
  // Stop() doesn't always get called.

  auto srp_input = BuildAutocompleteInput(
      SRP_URL, SRP_URL,
      metrics::OmniboxEventProto::SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
      OmniboxFocusType::ON_FOCUS);

  controller_->Start(srp_input);
  EXPECT_EQ(0u, NumMostVisitedMatches());

  // Most visited results arriving after a new request has been started.
  EXPECT_TRUE(top_sites_->EmitURLs());
  EXPECT_EQ(0u, NumMostVisitedMatches());
}

TEST_F(MostVisitedSitesProviderWithMatchesTest,
       TestStartSurfaceSourcingTilesFromItsSource) {
  controller_->Start(BuildAutocompleteInput(
      {}, NTP_URL, metrics::OmniboxEventProto::START_SURFACE_HOMEPAGE,
      OmniboxFocusType::ON_FOCUS));
  EXPECT_EQ(0u, NumMostVisitedMatches());
  // Start surface should not be waiting for old top_sites.
  EXPECT_FALSE(top_sites_->EmitURLs());
  EXPECT_EQ(0u, NumMostVisitedMatches());
  // Start surface should use its dedicated source instead.
  EXPECT_TRUE(top_sites_for_start_surface_->EmitURLs());
  CheckMatchesEquivalentTo(top_sites_for_start_surface_->urls(),
                           /* expect_tiles=*/true);
}

TEST_F(MostVisitedSitesProviderTest,
       TestDeleteMostVisitedElementForStartSurface) {
  // Make a copy (intentional - we'll modify this later)
  auto urls = top_sites_for_start_surface_->urls();
  controller_->Start(BuildAutocompleteInput(
      {}, NTP_URL, metrics::OmniboxEventProto::START_SURFACE_HOMEPAGE,
      OmniboxFocusType::ON_FOCUS));

  EXPECT_TRUE(top_sites_for_start_surface_->EmitURLs());
  CheckMatchesEquivalentTo(urls, /* expect_tiles=*/true);

  // Delete tile #3
  auto* match = GetMatch(AutocompleteMatchType::TILE_NAVSUGGEST, 0);
  ASSERT_NE(nullptr, match) << "No TILE_NAVSUGGEST Match found";
  controller_->DeleteMatchElement(*match, 2);

  // Observe that the URL is now blocked and removed from suggestion.
  auto deleted_url = urls[2].url;
  urls.erase(urls.begin() + 2);
  CheckMatchesEquivalentTo(urls, /* expect_tiles=*/true);
  // Note: when Start Surface is being used, we want to make sure we delete
  // tiles from all sources.
  EXPECT_TRUE(top_sites_->IsBlocked(deleted_url));
  EXPECT_TRUE(top_sites_for_start_surface_->IsBlocked(deleted_url));
}

class ParameterizedMostVisitedSitesProviderTest
    : public MostVisitedSitesProviderTest,
      public ::testing::WithParamInterface<bool> {
  void SetUp() override {
    features_.InitWithFeatureState(omnibox::kMostVisitedTiles, GetParam());
    MostVisitedSitesProviderTest::SetUp();
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         ParameterizedMostVisitedSitesProviderTest,
                         ::testing::Bool(),
                         [](const auto& info) {
                           return info.param ? "SingleMatchWithTiles"
                                             : "IndividualMatches";
                         });

TEST_P(ParameterizedMostVisitedSitesProviderTest,
       AllowMostVisitedSitesSuggestions) {
  using OEP = metrics::OmniboxEventProto;
  using OFT = OmniboxFocusType;

  // MostVisited should never deal with prefix suggestions.
  EXPECT_FALSE(provider_->AllowMostVisitedSitesSuggestions(
      BuildAutocompleteInput(WEB_URL, WEB_URL, OEP::OTHER, OFT::DEFAULT)));

  // This should always be true, as otherwise we will break MostVisited.
  EXPECT_TRUE(provider_->AllowMostVisitedSitesSuggestions(
      BuildAutocompleteInput(WEB_URL, WEB_URL, OEP::OTHER, OFT::ON_FOCUS)));

  // Verifies that non-permitted schemes are rejected.
  EXPECT_FALSE(provider_->AllowMostVisitedSitesSuggestions(
      BuildAutocompleteInput(FTP_URL, FTP_URL, OEP::OTHER, OFT::ON_FOCUS)));

  // Offer MV sites when the User is visiting a website and deletes text.
  EXPECT_TRUE(
      provider_->AllowMostVisitedSitesSuggestions(BuildAutocompleteInput(
          WEB_URL, WEB_URL, OEP::OTHER, OFT::DELETED_PERMANENT_TEXT)));

  // Verifies that metrics::OmniboxEventProto::START_SURFACE_HOMEPAGE is allowed
  // for MostVisited.
  EXPECT_TRUE(
      provider_->AllowMostVisitedSitesSuggestions(BuildAutocompleteInput(
          {}, NTP_URL, OEP::START_SURFACE_HOMEPAGE, OFT::ON_FOCUS)));

  // Verifies that metrics::OmniboxEventProto::START_SURFACE_NEW_TAB is allowed
  // for MostVisited.
  EXPECT_TRUE(
      provider_->AllowMostVisitedSitesSuggestions(BuildAutocompleteInput(
          {}, NTP_URL, OEP::START_SURFACE_NEW_TAB, OFT::ON_FOCUS)));
}

TEST_P(ParameterizedMostVisitedSitesProviderTest, TestCreateMostVisitedMatch) {
  controller_->Start(BuildAutocompleteInputForWebOnFocus());
  EXPECT_EQ(0u, NumMostVisitedMatches());
  // Confirm that the StartSurface source is rejected.
  EXPECT_FALSE(top_sites_for_start_surface_->EmitURLs());
  EXPECT_EQ(0u, NumMostVisitedMatches());
  // Accept only direct TopSites data.
  EXPECT_TRUE(top_sites_->EmitURLs());
  CheckMatchesEquivalentTo(top_sites_->urls(), GetParam());
}

TEST_P(ParameterizedMostVisitedSitesProviderTest,
       NoMatchesWhenNoMostVisitedSites) {
  // Start with no URLs.
  top_sites_->urls().clear();
  controller_->Start(BuildAutocompleteInputForWebOnFocus());
  EXPECT_EQ(0u, NumMostVisitedMatches());
  // Confirm that the StartSurface source is rejected.
  EXPECT_FALSE(top_sites_for_start_surface_->EmitURLs());
  EXPECT_EQ(0u, NumMostVisitedMatches());
  // Accept only direct TopSites data, confirm no matches are built.
  EXPECT_TRUE(top_sites_->EmitURLs());
  EXPECT_EQ(0u, NumMostVisitedMatches());
}

TEST_P(ParameterizedMostVisitedSitesProviderTest,
       TestDeleteMostVisitedElement) {
  // Make a copy (intentional - we'll modify this later)
  auto urls = top_sites_->urls();
  controller_->Start(BuildAutocompleteInputForWebOnFocus());
  // Confirm that the StartSurface source is rejected.
  EXPECT_FALSE(top_sites_for_start_surface_->EmitURLs());
  EXPECT_EQ(0u, NumMostVisitedMatches());
  // Accept only direct TopSites data.
  EXPECT_TRUE(top_sites_->EmitURLs());
  CheckMatchesEquivalentTo(urls, GetParam());

  // Commence delete.
  if (GetParam()) {
    auto* match = GetMatch(AutocompleteMatchType::TILE_NAVSUGGEST, 0);
    ASSERT_NE(nullptr, match) << "No TILE_NAVSUGGEST Match found";
    controller_->DeleteMatchElement(*match, 1);
  } else {
    auto* match = GetMatch(AutocompleteMatchType::NAVSUGGEST, 1);
    ASSERT_NE(nullptr, match) << "No NAVSUGGEST Match found";
    controller_->DeleteMatch(*match);
  }

  // Observe that the URL is now blocked and removed from suggestion.
  auto deleted_url = urls[1].url;
  urls.erase(urls.begin() + 1);
  CheckMatchesEquivalentTo(urls, GetParam());
  EXPECT_TRUE(top_sites_->IsBlocked(deleted_url));
}

TEST_P(ParameterizedMostVisitedSitesProviderTest,
       NoMatchesWhenLastURLIsDeleted) {
  // Start with just one URL.
  auto& urls = top_sites_->urls();
  urls.clear();
  urls.emplace_back(GURL("http://www.a.art/"), u"A art");

  controller_->Start(BuildAutocompleteInputForWebOnFocus());
  EXPECT_TRUE(top_sites_->EmitURLs());
  CheckMatchesEquivalentTo(urls, GetParam());

  // Commence delete of the only item that we have.
  if (GetParam()) {
    auto* match = GetMatch(AutocompleteMatchType::TILE_NAVSUGGEST, 0);
    ASSERT_NE(nullptr, match) << "No TILE_NAVSUGGEST Match found";
    controller_->DeleteMatchElement(*match, 0);
  } else {
    auto* match = GetMatch(AutocompleteMatchType::NAVSUGGEST, 0);
    ASSERT_NE(nullptr, match) << "No NAVSUGGEST Match found";
    controller_->DeleteMatch(*match);
  }

  // Confirm no more NAVSUGGEST matches are offered.
  EXPECT_EQ(0u, NumMostVisitedMatches());
}
