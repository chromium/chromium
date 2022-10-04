// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/zero_suggest_cache_service.h"

#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

class FakeObserver : public ZeroSuggestCacheService::Observer {
 public:
  explicit FakeObserver(const std::string& page_url) { page_url_ = page_url; }
  FakeObserver(const FakeObserver&) = delete;
  FakeObserver& operator=(const FakeObserver&) = delete;

  void OnZeroSuggestResponseUpdated(const std::string& page_url,
                                    const std::string& response) override {
    if (page_url_ == page_url) {
      data_ = response;
    }
  }

  const std::string& GetData() const { return data_; }

 private:
  std::string page_url_;
  std::string data_;
};

struct CacheEntry {
  std::string url;
  std::string response;
};

TEST(ZeroSuggestCacheServiceTest, CacheStartsEmpty) {
  ZeroSuggestCacheService cache_svc(1);
  EXPECT_TRUE(cache_svc.IsCacheEmpty());
}

TEST(ZeroSuggestCacheServiceTest, StoreResponsePopulatesCache) {
  ZeroSuggestCacheService cache_svc(1);
  cache_svc.StoreZeroSuggestResponse("https://www.google.com", "foo");
  EXPECT_FALSE(cache_svc.IsCacheEmpty());
}

TEST(ZeroSuggestCacheServiceTest, StoreResponseRecordsMemoryUsageHistogram) {
  base::HistogramTester histogram_tester;
  ZeroSuggestCacheService cache_svc(1);

  const std::string page_url = "https://www.google.com";
  const std::string response = "foo";
  const std::string histogram = "Omnibox.ZeroSuggestProvider.CacheMemoryUsage";

  cache_svc.StoreZeroSuggestResponse(page_url, response);
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(page_url), response);
  histogram_tester.ExpectTotalCount(histogram, 1);

  cache_svc.StoreZeroSuggestResponse(page_url, "");
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(page_url), "");
  histogram_tester.ExpectTotalCount(histogram, 2);

  cache_svc.StoreZeroSuggestResponse("", response);
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(""), response);
  histogram_tester.ExpectTotalCount(histogram, 3);

  cache_svc.StoreZeroSuggestResponse("", "");
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(""), "");
  histogram_tester.ExpectTotalCount(histogram, 4);
}

TEST(ZeroSuggestCacheServiceTest, StoreResponseUpdatesExistingEntry) {
  ZeroSuggestCacheService cache_svc(1);

  const std::string page_url = "https://www.google.com";
  const std::string old_response = "foo";
  const std::string new_response = "bar";

  cache_svc.StoreZeroSuggestResponse(page_url, old_response);
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(page_url), old_response);

  cache_svc.StoreZeroSuggestResponse(page_url, new_response);
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(page_url), new_response);
}

TEST(ZeroSuggestCacheServiceTest, StoreResponseNotifiesObservers) {
  ZeroSuggestCacheService cache_svc(2);

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
  EXPECT_EQ(goog_observer.GetData(), "");
  EXPECT_EQ(other_goog_observer.GetData(), "");
  EXPECT_EQ(fb_observer.GetData(), "");

  cache_svc.StoreZeroSuggestResponse(goog_url, "foo");

  // Only the relevant observers should have been notified.
  EXPECT_EQ(goog_observer.GetData(), "foo");
  EXPECT_EQ(other_goog_observer.GetData(), "foo");
  EXPECT_EQ(fb_observer.GetData(), "");

  cache_svc.StoreZeroSuggestResponse(fb_url, "bar");

  // Only the relevant observer should have been notified.
  EXPECT_EQ(goog_observer.GetData(), "foo");
  EXPECT_EQ(other_goog_observer.GetData(), "foo");
  EXPECT_EQ(fb_observer.GetData(), "bar");

  cache_svc.StoreZeroSuggestResponse(goog_url, "eggs");

  // The relevant observers should have received an updated value.
  EXPECT_EQ(goog_observer.GetData(), "eggs");
  EXPECT_EQ(other_goog_observer.GetData(), "eggs");
  EXPECT_EQ(fb_observer.GetData(), "bar");

  cache_svc.RemoveObserver(&fb_observer);
  cache_svc.StoreZeroSuggestResponse(fb_url, "spam");

  // The relevant observer should NOT have been notified (since it was removed
  // prior to updating the cache).
  EXPECT_EQ(goog_observer.GetData(), "eggs");
  EXPECT_EQ(other_goog_observer.GetData(), "eggs");
  EXPECT_EQ(fb_observer.GetData(), "bar");
}

TEST(ZeroSuggestCacheServiceTest, LeastRecentItemIsEvicted) {
  ZeroSuggestCacheService cache_svc(2);

  CacheEntry entry1 = {"https://www.facebook.com", "foo"};
  CacheEntry entry2 = {"https://www.google.com", "bar"};
  CacheEntry entry3 = {"https://www.example.com", "eggs"};

  cache_svc.StoreZeroSuggestResponse(entry1.url, entry1.response);
  cache_svc.StoreZeroSuggestResponse(entry2.url, entry2.response);

  // Fill up the zero suggest cache to max capacity.
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(entry1.url), entry1.response);
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(entry2.url), entry2.response);

  cache_svc.StoreZeroSuggestResponse(entry3.url, entry3.response);

  // "Least recently used" entry should now have been evicted from the cache.
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(entry1.url), "");
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(entry2.url), entry2.response);
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(entry3.url), entry3.response);
}

TEST(ZeroSuggestCacheServiceTest, ReadResponseWillRetrieveMatchingData) {
  ZeroSuggestCacheService cache_svc(1);

  const std::string page_url = "https://www.google.com";
  const std::string response = "foo";
  cache_svc.StoreZeroSuggestResponse(page_url, response);

  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(page_url), response);
}

TEST(ZeroSuggestCacheServiceTest, ReadResponseUpdatesRecency) {
  ZeroSuggestCacheService cache_svc(2);

  CacheEntry entry1 = {"https://www.google.com", "foo"};
  CacheEntry entry2 = {"https://www.facebook.com", "bar"};
  CacheEntry entry3 = {"https://www.example.com", "eggs"};

  // Fill up the zero suggest cache to max capacity.
  cache_svc.StoreZeroSuggestResponse(entry1.url, entry1.response);
  cache_svc.StoreZeroSuggestResponse(entry2.url, entry2.response);

  // Read the oldest entry in the cache, thereby marking the more recent entry
  // as "least recently used".
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(entry1.url), entry1.response);

  cache_svc.StoreZeroSuggestResponse(entry3.url, entry3.response);

  // Since the second request was the "least recently used", it should have been
  // evicted.
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(entry2.url), "");
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(entry1.url), entry1.response);
  EXPECT_EQ(cache_svc.ReadZeroSuggestResponse(entry3.url), entry3.response);
}

TEST(ZeroSuggestCacheServiceTest, ClearCacheResultsInEmptyCache) {
  ZeroSuggestCacheService cache_svc(1);

  cache_svc.StoreZeroSuggestResponse("https://www.google.com", "foo");

  cache_svc.ClearCache();

  EXPECT_TRUE(cache_svc.IsCacheEmpty());
}
