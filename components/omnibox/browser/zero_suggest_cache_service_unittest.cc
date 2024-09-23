// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/zero_suggest_cache_service.h"

#include <memory>
#include <string>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/omnibox/browser/zero_suggest_provider.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

using CacheEntry = ZeroSuggestCacheService::CacheEntry;

class FakeObserver : public ZeroSuggestCacheService::Observer {
 public:
  explicit FakeObserver(const std::string& page_url) { page_url_ = page_url; }
  FakeObserver(const FakeObserver&) = delete;
  FakeObserver& operator=(const FakeObserver&) = delete;

  void OnZeroSuggestResponseUpdated(const std::string& page_url,
                                    const CacheEntry& response) override {
    if (page_url_ == page_url) {
      data_ = response;
    }
  }

  const CacheEntry& GetData() const { return data_; }

 private:
  std::string page_url_;
  CacheEntry data_;
};

struct TestCacheEntry {
  std::string url;
  std::string response;
};

class ZeroSuggestCacheServiceTest : public testing::TestWithParam<bool> {
 public:
  ZeroSuggestCacheServiceTest() = default;

  ZeroSuggestCacheServiceTest(const ZeroSuggestCacheServiceTest&) = delete;
  ZeroSuggestCacheServiceTest& operator=(const ZeroSuggestCacheServiceTest&) =
      delete;

  void SetUp() override {
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    ZeroSuggestProvider::RegisterProfilePrefs(prefs_->registry());

    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          omnibox::kZeroSuggestInMemoryCaching);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          omnibox::kZeroSuggestInMemoryCaching);
    }
  }

  void TearDown() override { prefs_.reset(); }

  PrefService* GetPrefs() { return prefs_.get(); }

 private:
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, ZeroSuggestCacheServiceTest, testing::Bool());

TEST_P(ZeroSuggestCacheServiceTest, CacheStartsEmpty) {
  ZeroSuggestCacheService cache_svc(std::make_unique<TestSchemeClassifier>(),
                                    GetPrefs(), 1);
  EXPECT_TRUE(cache_svc.IsInMemoryCacheEmptyForTesting());
}

TEST_P(ZeroSuggestCacheServiceTest, StoreResponseRecordsMemoryUsageHistogram) {
  // Cache memory usage histogram is only logged when using the in-memory cache.
  if (!GetParam()) {
    return;
  }

  base::HistogramTester histogram_tester;
  ZeroSuggestCacheService cache_svc(std::make_unique<TestSchemeClassifier>(),
                                    GetPrefs(), 1);

  const std::string page_url = "https://www.google.com";
  const std::string response = "foo";
  const std::string histogram = "Omnibox.ZeroSuggestProvider.CacheMemoryUsage";

  cache_svc.StoreZeroSuggestResponse(page_url, response);
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(page_url).response_json,
            response);
  histogram_tester.ExpectTotalCount(histogram, 2);

  cache_svc.StoreZeroSuggestResponse(page_url, "");
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(page_url).response_json, "");
  histogram_tester.ExpectTotalCount(histogram, 3);

  cache_svc.StoreZeroSuggestResponse("", response);
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse("").response_json, response);
  histogram_tester.ExpectTotalCount(histogram, 4);

  cache_svc.StoreZeroSuggestResponse("", "");
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse("").response_json, "");
  histogram_tester.ExpectTotalCount(histogram, 5);
}

TEST_P(ZeroSuggestCacheServiceTest, StoreResponseUpdatesExistingEntry) {
  ZeroSuggestCacheService cache_svc(std::make_unique<TestSchemeClassifier>(),
                                    GetPrefs(), 1);

  const std::string page_url = "https://www.google.com";
  const std::string old_response = "foo";
  const std::string new_response = "bar";

  cache_svc.StoreZeroSuggestResponse(page_url, old_response);
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(page_url).response_json,
            old_response);

  cache_svc.StoreZeroSuggestResponse(page_url, new_response);
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(page_url).response_json,
            new_response);
}

TEST_P(ZeroSuggestCacheServiceTest, StoreResponseNotifiesObservers) {
  ZeroSuggestCacheService cache_svc(std::make_unique<TestSchemeClassifier>(),
                                    GetPrefs(), 2);

  const std::string goog_url = "https://www.google.com";
  const std::string fb_url = "https://www.facebook.com";

  FakeObserver goog_observer(goog_url);
  FakeObserver other_goog_observer(goog_url);
  FakeObserver fb_observer(fb_url);

  // Attach all observers to the caching service.
  cache_svc.AddObserver(&goog_observer);
  cache_svc.AddObserver(&other_goog_observer);
  cache_svc.AddObserver(&fb_observer);

  // None of the observers should have been notified yet.
  EXPECT_EQ(goog_observer.GetData().response_json, "");
  EXPECT_EQ(other_goog_observer.GetData().response_json, "");
  EXPECT_EQ(fb_observer.GetData().response_json, "");

  cache_svc.StoreZeroSuggestResponse(goog_url, "foo");

  // Only the relevant observers should have been notified.
  EXPECT_EQ(goog_observer.GetData().response_json, "foo");
  EXPECT_EQ(other_goog_observer.GetData().response_json, "foo");
  EXPECT_EQ(fb_observer.GetData().response_json, "");

  cache_svc.StoreZeroSuggestResponse(fb_url, "bar");

  // Only the relevant observer should have been notified.
  EXPECT_EQ(goog_observer.GetData().response_json, "foo");
  EXPECT_EQ(other_goog_observer.GetData().response_json, "foo");
  EXPECT_EQ(fb_observer.GetData().response_json, "bar");

  cache_svc.StoreZeroSuggestResponse(goog_url, "eggs");

  // The relevant observers should have received an updated value.
  EXPECT_EQ(goog_observer.GetData().response_json, "eggs");
  EXPECT_EQ(other_goog_observer.GetData().response_json, "eggs");
  EXPECT_EQ(fb_observer.GetData().response_json, "bar");

  cache_svc.RemoveObserver(&fb_observer);
  cache_svc.StoreZeroSuggestResponse(fb_url, "spam");

  // The relevant observer should NOT have been notified (since it was removed
  // prior to updating the cache).
  EXPECT_EQ(goog_observer.GetData().response_json, "eggs");
  EXPECT_EQ(other_goog_observer.GetData().response_json, "eggs");
  EXPECT_EQ(fb_observer.GetData().response_json, "bar");
}

TEST_P(ZeroSuggestCacheServiceTest, LeastRecentItemIsEvicted) {
  // LRU (recency) logic only takes effect when using in-memory caching.
  if (!GetParam()) {
    return;
  }

  ZeroSuggestCacheService cache_svc(std::make_unique<TestSchemeClassifier>(),
                                    GetPrefs(), 2);

  TestCacheEntry entry1 = {"https://www.facebook.com", "foo"};
  TestCacheEntry entry2 = {"https://www.google.com", "bar"};
  TestCacheEntry entry3 = {"https://www.example.com", "eggs"};

  cache_svc.StoreZeroSuggestResponse(entry1.url, entry1.response);
  cache_svc.StoreZeroSuggestResponse(entry2.url, entry2.response);

  // Fill up the zero suggest cache to max capacity.
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(entry1.url).response_json,
            entry1.response);
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(entry2.url).response_json,
            entry2.response);

  cache_svc.StoreZeroSuggestResponse(entry3.url, entry3.response);

  // "Least recently used" entry should now have been evicted from the cache.
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(entry1.url).response_json, "");
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(entry2.url).response_json,
            entry2.response);
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(entry3.url).response_json,
            entry3.response);
}

TEST_P(ZeroSuggestCacheServiceTest, ReadResponseWillRetrieveMatchingData) {
  ZeroSuggestCacheService cache_svc(std::make_unique<TestSchemeClassifier>(),
                                    GetPrefs(), 1);

  const std::string page_url = "https://www.google.com";
  const std::string response = "foo";
  cache_svc.StoreZeroSuggestResponse(page_url, response);

  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(page_url).response_json,
            response);
}

TEST_P(ZeroSuggestCacheServiceTest, ReadResponseUpdatesRecency) {
  // LRU (recency) logic only takes effect when using in-memory caching.
  if (!GetParam()) {
    return;
  }

  ZeroSuggestCacheService cache_svc(std::make_unique<TestSchemeClassifier>(),
                                    GetPrefs(), 2);

  TestCacheEntry entry1 = {"https://www.google.com", "foo"};
  TestCacheEntry entry2 = {"https://www.facebook.com", "bar"};
  TestCacheEntry entry3 = {"https://www.example.com", "eggs"};

  // Fill up the zero suggest cache to max capacity.
  cache_svc.StoreZeroSuggestResponse(entry1.url, entry1.response);
  cache_svc.StoreZeroSuggestResponse(entry2.url, entry2.response);

  // Read the oldest entry in the cache, thereby marking the more recent entry
  // as "least recently used".
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(entry1.url).response_json,
            entry1.response);

  cache_svc.StoreZeroSuggestResponse(entry3.url, entry3.response);

  // Since the second request was the "least recently used", it should have been
  // evicted.
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(entry2.url).response_json, "");
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(entry1.url).response_json,
            entry1.response);
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(entry3.url).response_json,
            entry3.response);
}

TEST_P(ZeroSuggestCacheServiceTest, ClearCacheResultsInEmptyCache) {
  TestCacheEntry ntp_entry = {"", "foo"};
  TestCacheEntry srp_entry = {"https://www.google.com/search?q=bar", "bar"};

  ZeroSuggestCacheService cache_svc(std::make_unique<TestSchemeClassifier>(),
                                    GetPrefs(), 2);

  cache_svc.StoreZeroSuggestResponse(ntp_entry.url, ntp_entry.response);
  cache_svc.StoreZeroSuggestResponse(srp_entry.url, srp_entry.response);

  EXPECT_FALSE(
      cache_svc.ReadZeroSuggestResponse(ntp_entry.url).response_json.empty());
  EXPECT_FALSE(
      cache_svc.ReadZeroSuggestResponse(srp_entry.url).response_json.empty());

  cache_svc.ClearCache();

  EXPECT_TRUE(
      cache_svc.ReadZeroSuggestResponse(ntp_entry.url).response_json.empty());
  EXPECT_TRUE(
      cache_svc.ReadZeroSuggestResponse(srp_entry.url).response_json.empty());
}

TEST_P(ZeroSuggestCacheServiceTest, CacheLoadsFromPrefsOnStartup) {
  // Persistence logic only executes when using in-memory cache.
  if (!GetParam()) {
    return;
  }

  TestCacheEntry ntp_entry = {"", "foo"};
  TestCacheEntry srp_entry = {"https://www.google.com/search?q=bar", "bar"};
  TestCacheEntry web_entry = {"https://www.example.com", "eggs"};

  base::Value::Dict prefs_dict;
  prefs_dict.Set(ntp_entry.url, ntp_entry.response);
  prefs_dict.Set(srp_entry.url, srp_entry.response);
  prefs_dict.Set(web_entry.url, web_entry.response);

  PrefService* prefs = GetPrefs();
  prefs->SetDict(omnibox::kZeroSuggestCachedResultsWithURL,
                 std::move(prefs_dict));

  ZeroSuggestCacheService cache_svc(std::make_unique<TestSchemeClassifier>(),
                                    prefs, 3);

  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(ntp_entry.url).response_json,
            ntp_entry.response);
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(srp_entry.url).response_json,
            srp_entry.response);
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(web_entry.url).response_json,
            web_entry.response);
}

TEST_P(ZeroSuggestCacheServiceTest, CacheDumpsToPrefsOnShutdown) {
  // Persistence logic only executes when using in-memory cache.
  if (!GetParam()) {
    return;
  }

  TestCacheEntry ntp_entry = {"", "foo"};
  TestCacheEntry srp_entry = {"https://www.google.com/search?q=bar", "bar"};
  TestCacheEntry web_entry = {"https://www.example.com", "eggs"};

  PrefService* prefs = GetPrefs();
  {
    ZeroSuggestCacheService cache_svc(std::make_unique<TestSchemeClassifier>(),
                                      prefs, 3);
    cache_svc.StoreZeroSuggestResponse(ntp_entry.url, ntp_entry.response);
    cache_svc.StoreZeroSuggestResponse(srp_entry.url, srp_entry.response);
    cache_svc.StoreZeroSuggestResponse(web_entry.url, web_entry.response);
  }

  EXPECT_EQ(omnibox::GetUserPreferenceForZeroSuggestCachedResponse(
                prefs, ntp_entry.url),
            ntp_entry.response);
  EXPECT_EQ(omnibox::GetUserPreferenceForZeroSuggestCachedResponse(
                prefs, srp_entry.url),
            srp_entry.response);
  EXPECT_EQ(omnibox::GetUserPreferenceForZeroSuggestCachedResponse(
                prefs, web_entry.url),
            web_entry.response);
}

TEST_P(ZeroSuggestCacheServiceTest, ClearCacheResultsInEmptyPersistencePrefs) {
  PrefService* prefs = GetPrefs();

  TestCacheEntry ntp_entry = {"", "foo"};
  TestCacheEntry srp_entry = {"https://www.google.com/search?q=bar", "bar"};
  TestCacheEntry web_entry = {"https://www.example.com", "eggs"};

  // Store ZPS response on NTP in user prefs.
  prefs->SetString(omnibox::kZeroSuggestCachedResults, ntp_entry.response);

  base::Value::Dict prefs_dict;
  prefs_dict.Set(srp_entry.url, srp_entry.response);
  prefs_dict.Set(web_entry.url, web_entry.response);

  // Store ZPS responses on SRP/Web in user prefs.
  prefs->SetDict(omnibox::kZeroSuggestCachedResultsWithURL,
                 std::move(prefs_dict));

  // Relevant prefs should start off non-empty.
  EXPECT_FALSE(omnibox::GetUserPreferenceForZeroSuggestCachedResponse(
                   prefs, ntp_entry.url)
                   .empty());
  EXPECT_FALSE(omnibox::GetUserPreferenceForZeroSuggestCachedResponse(
                   prefs, srp_entry.url)
                   .empty());
  EXPECT_FALSE(omnibox::GetUserPreferenceForZeroSuggestCachedResponse(
                   prefs, web_entry.url)
                   .empty());

  {
    ZeroSuggestCacheService cache_svc(std::make_unique<TestSchemeClassifier>(),
                                      GetPrefs(), 3);
    if (GetParam()) {
      EXPECT_FALSE(cache_svc.IsInMemoryCacheEmptyForTesting());
    }

    cache_svc.ClearCache();
    EXPECT_TRUE(cache_svc.IsInMemoryCacheEmptyForTesting());
  }

  // Relevant prefs should now be empty.
  EXPECT_TRUE(omnibox::GetUserPreferenceForZeroSuggestCachedResponse(
                  prefs, ntp_entry.url)
                  .empty());
  EXPECT_TRUE(omnibox::GetUserPreferenceForZeroSuggestCachedResponse(
                  prefs, srp_entry.url)
                  .empty());
  EXPECT_TRUE(omnibox::GetUserPreferenceForZeroSuggestCachedResponse(
                  prefs, web_entry.url)
                  .empty());
}

TEST_P(ZeroSuggestCacheServiceTest,
       GetSuggestResultsReturnsEmptyListForInvalidResponseJson) {
  ZeroSuggestCacheService cache_svc(std::make_unique<TestSchemeClassifier>(),
                                    GetPrefs(), 1);

  AutocompleteInput ac_input(u"", metrics::OmniboxEventProto::OTHER,
                             TestSchemeClassifier());
  FakeAutocompleteProviderClient client;

  const std::vector<TestCacheEntry> invalid_json_responses = {
      // Malformed JSON
      {"https://www.example.com", ""},
      // Malformed JSON
      {"https://www.example.com", "({ malformed })"},
      // Malformed JSON
      {"https://www.example.com", "][]["},
      // Invalid JSON (according to parsing logic in
      // `SearchSuggestionParser::ParseSuggestResults()`)
      {"https://www.example.com", "[]"},
      // Invalid JSON
      {"https://www.example.com", "[123]"},
      // Invalid JSON
      {"https://www.example.com", "[[]]"},
      // Invalid JSON
      {"https://www.example.com", "['a': 1]"},
  };

  for (TestCacheEntry entry : invalid_json_responses) {
    cache_svc.StoreZeroSuggestResponse(entry.url, entry.response);
    CacheEntry cached_entry = cache_svc.ReadZeroSuggestResponse(entry.url);
    EXPECT_TRUE(cache_svc.GetSuggestResults(cached_entry).empty());
  }
}
