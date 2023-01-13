// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/zero_suggest_cache_service.h"

#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/zero_suggest_provider.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

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

class ZeroSuggestCacheServiceTest : public testing::Test {
 public:
  ZeroSuggestCacheServiceTest() = default;

  ZeroSuggestCacheServiceTest(const ZeroSuggestCacheServiceTest&) = delete;
  ZeroSuggestCacheServiceTest& operator=(const ZeroSuggestCacheServiceTest&) =
      delete;

  void SetUp() override {
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    ZeroSuggestProvider::RegisterProfilePrefs(prefs_->registry());

    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{omnibox::kZeroSuggestInMemoryCaching},
        /*disabled_features=*/{});
  }

  void TearDown() override { prefs_.reset(); }

  PrefService* GetPrefs() { return prefs_.get(); }

 private:
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ZeroSuggestCacheServiceTest, CacheStartsEmpty) {
  ZeroSuggestCacheService cache_svc(GetPrefs(), 1);
  EXPECT_TRUE(cache_svc.IsCacheEmpty());
}

TEST_F(ZeroSuggestCacheServiceTest, StoreResponsePopulatesCache) {
  ZeroSuggestCacheService cache_svc(GetPrefs(), 1);
  cache_svc.StoreZeroSuggestResponse("https://www.google.com",
                                     CacheEntry("foo"));
  EXPECT_FALSE(cache_svc.IsCacheEmpty());
}

TEST_F(ZeroSuggestCacheServiceTest, StoreResponseRecordsMemoryUsageHistogram) {
  base::HistogramTester histogram_tester;
  ZeroSuggestCacheService cache_svc(GetPrefs(), 1);

  const std::string page_url = "https://www.google.com";
  const std::string response = "foo";
  const std::string histogram = "Omnibox.ZeroSuggestProvider.CacheMemoryUsage";

  cache_svc.StoreZeroSuggestResponse(page_url, CacheEntry(response));
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(page_url).response_json,
            response);
  histogram_tester.ExpectTotalCount(histogram, 1);

  cache_svc.StoreZeroSuggestResponse(page_url, CacheEntry(""));
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(page_url).response_json, "");
  histogram_tester.ExpectTotalCount(histogram, 2);

  cache_svc.StoreZeroSuggestResponse("", CacheEntry(response));
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse("").response_json, response);
  histogram_tester.ExpectTotalCount(histogram, 3);

  cache_svc.StoreZeroSuggestResponse("", CacheEntry(""));
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse("").response_json, "");
  histogram_tester.ExpectTotalCount(histogram, 4);
}

TEST_F(ZeroSuggestCacheServiceTest, StoreResponseUpdatesExistingEntry) {
  ZeroSuggestCacheService cache_svc(GetPrefs(), 1);

  const std::string page_url = "https://www.google.com";
  const std::string old_response = "foo";
  const std::string new_response = "bar";

  cache_svc.StoreZeroSuggestResponse(page_url, CacheEntry(old_response));
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(page_url).response_json,
            old_response);

  cache_svc.StoreZeroSuggestResponse(page_url, CacheEntry(new_response));
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(page_url).response_json,
            new_response);
}

TEST_F(ZeroSuggestCacheServiceTest, StoreResponseNotifiesObservers) {
  ZeroSuggestCacheService cache_svc(GetPrefs(), 2);

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

  cache_svc.StoreZeroSuggestResponse(goog_url, CacheEntry("foo"));

  // Only the relevant observers should have been notified.
  EXPECT_EQ(goog_observer.GetData().response_json, "foo");
  EXPECT_EQ(other_goog_observer.GetData().response_json, "foo");
  EXPECT_EQ(fb_observer.GetData().response_json, "");

  cache_svc.StoreZeroSuggestResponse(fb_url, CacheEntry("bar"));

  // Only the relevant observer should have been notified.
  EXPECT_EQ(goog_observer.GetData().response_json, "foo");
  EXPECT_EQ(other_goog_observer.GetData().response_json, "foo");
  EXPECT_EQ(fb_observer.GetData().response_json, "bar");

  cache_svc.StoreZeroSuggestResponse(goog_url, CacheEntry("eggs"));

  // The relevant observers should have received an updated value.
  EXPECT_EQ(goog_observer.GetData().response_json, "eggs");
  EXPECT_EQ(other_goog_observer.GetData().response_json, "eggs");
  EXPECT_EQ(fb_observer.GetData().response_json, "bar");

  cache_svc.RemoveObserver(&fb_observer);
  cache_svc.StoreZeroSuggestResponse(fb_url, CacheEntry("spam"));

  // The relevant observer should NOT have been notified (since it was removed
  // prior to updating the cache).
  EXPECT_EQ(goog_observer.GetData().response_json, "eggs");
  EXPECT_EQ(other_goog_observer.GetData().response_json, "eggs");
  EXPECT_EQ(fb_observer.GetData().response_json, "bar");
}

TEST_F(ZeroSuggestCacheServiceTest, LeastRecentItemIsEvicted) {
  ZeroSuggestCacheService cache_svc(GetPrefs(), 2);

  TestCacheEntry entry1 = {"https://www.facebook.com", "foo"};
  TestCacheEntry entry2 = {"https://www.google.com", "bar"};
  TestCacheEntry entry3 = {"https://www.example.com", "eggs"};

  cache_svc.StoreZeroSuggestResponse(entry1.url, CacheEntry(entry1.response));
  cache_svc.StoreZeroSuggestResponse(entry2.url, CacheEntry(entry2.response));

  // Fill up the zero suggest cache to max capacity.
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(entry1.url).response_json,
            entry1.response);
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(entry2.url).response_json,
            entry2.response);

  cache_svc.StoreZeroSuggestResponse(entry3.url, CacheEntry(entry3.response));

  // "Least recently used" entry should now have been evicted from the cache.
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(entry1.url).response_json, "");
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(entry2.url).response_json,
            entry2.response);
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(entry3.url).response_json,
            entry3.response);
}

TEST_F(ZeroSuggestCacheServiceTest, ReadResponseWillRetrieveMatchingData) {
  ZeroSuggestCacheService cache_svc(GetPrefs(), 1);

  const std::string page_url = "https://www.google.com";
  const std::string response = "foo";
  cache_svc.StoreZeroSuggestResponse(page_url, CacheEntry(response));

  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(page_url).response_json,
            response);
}

TEST_F(ZeroSuggestCacheServiceTest, ReadResponseUpdatesRecency) {
  ZeroSuggestCacheService cache_svc(GetPrefs(), 2);

  TestCacheEntry entry1 = {"https://www.google.com", "foo"};
  TestCacheEntry entry2 = {"https://www.facebook.com", "bar"};
  TestCacheEntry entry3 = {"https://www.example.com", "eggs"};

  // Fill up the zero suggest cache to max capacity.
  cache_svc.StoreZeroSuggestResponse(entry1.url, CacheEntry(entry1.response));
  cache_svc.StoreZeroSuggestResponse(entry2.url, CacheEntry(entry2.response));

  // Read the oldest entry in the cache, thereby marking the more recent entry
  // as "least recently used".
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(entry1.url).response_json,
            entry1.response);

  cache_svc.StoreZeroSuggestResponse(entry3.url, CacheEntry(entry3.response));

  // Since the second request was the "least recently used", it should have been
  // evicted.
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(entry2.url).response_json, "");
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(entry1.url).response_json,
            entry1.response);
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(entry3.url).response_json,
            entry3.response);
}

TEST_F(ZeroSuggestCacheServiceTest, ClearCacheResultsInEmptyCache) {
  TestCacheEntry ntp_entry = {"", "foo"};
  TestCacheEntry srp_entry = {"https://www.google.com/search?q=bar", "bar"};
  TestCacheEntry web_entry = {"https://www.example.com", "eggs"};

  ZeroSuggestCacheService cache_svc(GetPrefs(), 3);

  cache_svc.StoreZeroSuggestResponse(ntp_entry.url,
                                     CacheEntry(ntp_entry.response));
  cache_svc.StoreZeroSuggestResponse(srp_entry.url,
                                     CacheEntry(srp_entry.response));
  cache_svc.StoreZeroSuggestResponse(web_entry.url,
                                     CacheEntry(web_entry.response));

  EXPECT_FALSE(
      cache_svc.ReadZeroSuggestResponse(ntp_entry.url).response_json.empty());
  EXPECT_FALSE(
      cache_svc.ReadZeroSuggestResponse(srp_entry.url).response_json.empty());
  EXPECT_FALSE(
      cache_svc.ReadZeroSuggestResponse(web_entry.url).response_json.empty());

  cache_svc.ClearCache();

  EXPECT_TRUE(
      cache_svc.ReadZeroSuggestResponse(ntp_entry.url).response_json.empty());
  EXPECT_TRUE(
      cache_svc.ReadZeroSuggestResponse(srp_entry.url).response_json.empty());
  EXPECT_TRUE(
      cache_svc.ReadZeroSuggestResponse(web_entry.url).response_json.empty());
}

TEST_F(ZeroSuggestCacheServiceTest, CacheLoadsFromPrefsOnStartup) {
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

  ZeroSuggestCacheService cache_svc(prefs, 3);

  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(ntp_entry.url).response_json,
            ntp_entry.response);
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(srp_entry.url).response_json,
            srp_entry.response);
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(web_entry.url).response_json,
            web_entry.response);
}

TEST_F(ZeroSuggestCacheServiceTest, CacheDumpsToPrefsOnShutdown) {
  TestCacheEntry ntp_entry = {"", "foo"};
  TestCacheEntry srp_entry = {"https://www.google.com/search?q=bar", "bar"};
  TestCacheEntry web_entry = {"https://www.example.com", "eggs"};

  PrefService* prefs = GetPrefs();
  {
    ZeroSuggestCacheService cache_svc(prefs, 3);
    cache_svc.StoreZeroSuggestResponse(ntp_entry.url,
                                       CacheEntry(ntp_entry.response));
    cache_svc.StoreZeroSuggestResponse(srp_entry.url,
                                       CacheEntry(srp_entry.response));
    cache_svc.StoreZeroSuggestResponse(web_entry.url,
                                       CacheEntry(web_entry.response));
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

TEST_F(ZeroSuggestCacheServiceTest, CacheWorksGivenNullPrefService) {
  TestCacheEntry ntp_entry = {"", "foo"};

  PrefService* prefs = nullptr;
  ZeroSuggestCacheService cache_svc(prefs, 1);
  cache_svc.StoreZeroSuggestResponse(ntp_entry.url,
                                     CacheEntry(ntp_entry.response));

  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(ntp_entry.url).response_json,
            ntp_entry.response);
}
