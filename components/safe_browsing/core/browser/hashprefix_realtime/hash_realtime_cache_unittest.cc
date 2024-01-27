// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_cache.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace safe_browsing {

class HashRealTimeCacheTest : public PlatformTest {
 protected:
  V5::Duration CreateCacheDuration(int seconds, int nanos) {
    V5::Duration cache_duration;
    cache_duration.set_seconds(seconds);
    cache_duration.set_nanos(nanos);
    return cache_duration;
  }
  // Does not populate the "attributes" field.
  V5::FullHash CreateBasicFullHash(std::string full_hash_str,
                                   std::vector<V5::ThreatType> threat_types) {
    V5::FullHash full_hash_object;
    full_hash_object.set_full_hash(full_hash_str);
    for (const auto& threat_type : threat_types) {
      auto* details = full_hash_object.add_full_hash_details();
      details->set_threat_type(threat_type);
    }
    return full_hash_object;
  }
  void AddThreatTypeAndAttributes(V5::FullHash& full_hash_object,
                                  V5::ThreatType threat_type,
                                  std::vector<V5::ThreatAttribute> attributes) {
    auto* details = full_hash_object.add_full_hash_details();
    details->set_threat_type(threat_type);
    for (const auto& attribute : attributes) {
      details->add_attributes(attribute);
    }
  }
  void CheckAndResetCacheHitsAndMisses(int num_hits, int num_misses) {
    histogram_tester_->ExpectBucketCount("SafeBrowsing.HPRT.CacheHit",
                                         /*sample=*/true,
                                         /*expected_count=*/num_hits);
    histogram_tester_->ExpectBucketCount("SafeBrowsing.HPRT.CacheHit",
                                         /*sample=*/false,
                                         /*expected_count=*/num_misses);
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }
  void CheckAndResetCacheDurationLogs(
      std::optional<int> initial_cache_duration_sec,
      std::optional<int> remaining_cache_duration_sec) {
    if (initial_cache_duration_sec.has_value()) {
      histogram_tester_->ExpectUniqueSample(
          /*name=*/"SafeBrowsing.HPRT.CacheDuration.InitialOnSet",
          /*sample=*/initial_cache_duration_sec.value() * 1000,  // sec to ms
          /*expected_bucket_count=*/1);
    } else {
      histogram_tester_->ExpectTotalCount(
          /*name=*/"SafeBrowsing.HPRT.CacheDuration.InitialOnSet",
          /*expected_count=*/0);
    }
    if (remaining_cache_duration_sec.has_value()) {
      histogram_tester_->ExpectUniqueSample(
          /*name=*/"SafeBrowsing.HPRT.CacheDuration.RemainingOnHit",
          /*sample=*/remaining_cache_duration_sec.value() * 1000,  // sec to ms
          /*expected_bucket_count=*/1);
    } else {
      histogram_tester_->ExpectTotalCount(
          /*name=*/"SafeBrowsing.HPRT.CacheDuration.RemainingOnHit",
          /*expected_count=*/0);
    }
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }
  void CheckAndResetCacheSizeOnClear(int num_hash_prefixes,
                                     int num_full_hashes) {
    histogram_tester_->ExpectBucketCount(
        "SafeBrowsing.HPRT.Cache.HashPrefixCount",
        /*sample=*/num_hash_prefixes,
        /*expected_count=*/1);
    histogram_tester_->ExpectBucketCount(
        "SafeBrowsing.HPRT.Cache.FullHashCount",
        /*sample=*/num_full_hashes,
        /*expected_count=*/1);
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }
  int GetNumCacheEntries(std::unique_ptr<HashRealTimeCache>& cache) {
    // This includes expired entries that have not yet been cleaned up too.
    return cache->cache_.size();
  }
  void CacheEntry(std::unique_ptr<HashRealTimeCache>& cache_internal,
                  std::string full_hash,
                  int cache_duration_seconds) {
    cache_internal->CacheSearchHashesResponse(
        {full_hash.substr(0, 4)},
        {CreateBasicFullHash(full_hash, {V5::ThreatType::MALWARE})},
        CreateCacheDuration(cache_duration_seconds, 0));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<base::HistogramTester> histogram_tester_ =
      std::make_unique<base::HistogramTester>();
};

TEST_F(HashRealTimeCacheTest, TestCacheMatching_EmptyCache) {
  auto cache = std::make_unique<HashRealTimeCache>();
  EXPECT_TRUE(cache->SearchCache({}).empty());
  CheckAndResetCacheHitsAndMisses(/*num_hits=*/0, /*num_misses=*/0);
  EXPECT_TRUE(cache->SearchCache({"aaaa"}).empty());
  CheckAndResetCacheHitsAndMisses(/*num_hits=*/0, /*num_misses=*/1);
  EXPECT_TRUE(cache->SearchCache({"aaaa", "bbbb"}).empty());
  CheckAndResetCacheHitsAndMisses(/*num_hits=*/0, /*num_misses=*/2);
}

TEST_F(HashRealTimeCacheTest, TestCacheMatching_BasicFunctionality) {
  base::HistogramTester histogram_tester;
  auto cache = std::make_unique<HashRealTimeCache>();
  // The below is done within a block to ensure that the cache works even once
  // the inputs to CacheSearchHashesResponse have been destructed.
  {
    std::vector<std::string> requested_hash_prefixes = {"aaaa", "bbbb", "cccc",
                                                        "dddd"};
    std::vector<V5::FullHash> response_full_hashes = {
        CreateBasicFullHash(
            "aaaa1111111111111111111111111111",
            {V5::ThreatType::SOCIAL_ENGINEERING, V5::ThreatType::MALWARE,
             V5::ThreatType::UNWANTED_SOFTWARE, V5::ThreatType::API_ABUSE}),
        CreateBasicFullHash("aaaa2222222222222222222222222222",
                            {V5::ThreatType::MALWARE}),
        CreateBasicFullHash("aaaa3333333333333333333333333333",
                            {V5::ThreatType::API_ABUSE}),
        CreateBasicFullHash("cccc1111111111111111111111111111",
                            {V5::ThreatType::API_ABUSE,
                             V5::ThreatType::ABUSIVE_EXPERIENCE_VIOLATION,
                             V5::ThreatType::BETTER_ADS_VIOLATION,
                             V5::ThreatType::ABUSIVE_EXPERIENCE_VIOLATION,
                             V5::ThreatType::POTENTIALLY_HARMFUL_APPLICATION}),
    };
    cache->CacheSearchHashesResponse(requested_hash_prefixes,
                                     response_full_hashes,
                                     CreateCacheDuration(300, 0));
  }

  // Searching for no prefix or for prefixes not in the request should yield
  // empty cache results.
  EXPECT_TRUE(cache->SearchCache({}).empty());
  CheckAndResetCacheHitsAndMisses(/*num_hits=*/0, /*num_misses=*/0);
  EXPECT_TRUE(cache->SearchCache({"eeee"}).empty());
  CheckAndResetCacheHitsAndMisses(/*num_hits=*/0, /*num_misses=*/1);
  EXPECT_TRUE(cache->SearchCache({"eeee", "ffff"}).empty());
  CheckAndResetCacheHitsAndMisses(/*num_hits=*/0, /*num_misses=*/2);

  std::set<std::string> hash_prefixes_to_search = {"aaaa", "bbbb", "cccc",
                                                   "dddd", "eeee", "ffff"};
  auto cache_results = cache->SearchCache(hash_prefixes_to_search);
  CheckAndResetCacheHitsAndMisses(/*num_hits=*/4, /*num_misses=*/2);

  // Don't expect cache results for eeee and ffff, since they are not in the
  // cache. Expect cache results for all other prefixes.
  EXPECT_EQ(cache_results.size(), 4u);
  EXPECT_TRUE(base::Contains(cache_results, "aaaa"));
  EXPECT_TRUE(base::Contains(cache_results, "bbbb"));
  EXPECT_TRUE(base::Contains(cache_results, "cccc"));
  EXPECT_TRUE(base::Contains(cache_results, "dddd"));
  EXPECT_FALSE(base::Contains(cache_results, "eeee"));
  EXPECT_FALSE(base::Contains(cache_results, "ffff"));

  // bbbb and dddd should both have empty results, because they did not have any
  // corresponding full hashes.
  EXPECT_TRUE(cache_results["bbbb"].empty());
  EXPECT_TRUE(cache_results["dddd"].empty());
  // cccc should also have empty results, because the threat types returned by
  // the server for that full hash were not relevant for hash-prefix real-time
  // lookups.
  EXPECT_TRUE(cache_results["cccc"].empty());

  // aaaa should match both aaaa...1 and aaaa...2, but not aaaa....3 due to
  // irrelevant threat types.
  EXPECT_EQ(cache_results["aaaa"].size(), 2u);
  // aaaa...1 should only contain relevant threat types.
  auto aaaa1_results = cache_results["aaaa"][0];
  EXPECT_EQ(aaaa1_results.full_hash(), "aaaa1111111111111111111111111111");
  auto aaaa1_details = aaaa1_results.full_hash_details();
  EXPECT_EQ(aaaa1_details.size(), 3);
  EXPECT_EQ(aaaa1_details[0].threat_type(), V5::ThreatType::SOCIAL_ENGINEERING);
  EXPECT_TRUE(aaaa1_details[0].attributes().empty());
  EXPECT_EQ(aaaa1_details[1].threat_type(), V5::ThreatType::MALWARE);
  EXPECT_TRUE(aaaa1_details[1].attributes().empty());
  EXPECT_EQ(aaaa1_details[2].threat_type(), V5::ThreatType::UNWANTED_SOFTWARE);
  EXPECT_TRUE(aaaa1_details[2].attributes().empty());
  // aaaa...2 should have one threat type (malware).
  auto aaaa2_results = cache_results["aaaa"][1];
  EXPECT_EQ(aaaa2_results.full_hash(), "aaaa2222222222222222222222222222");
  auto aaaa2_details = aaaa2_results.full_hash_details();
  EXPECT_EQ(aaaa2_details.size(), 1);
  EXPECT_EQ(aaaa2_details[0].threat_type(), V5::ThreatType::MALWARE);
  EXPECT_TRUE(aaaa2_details[0].attributes().empty());
}

TEST_F(HashRealTimeCacheTest, TestCacheMatching_Expiration) {
  auto cache = std::make_unique<HashRealTimeCache>();
  // The below are done within blocks to ensure that the cache works even once
  // the inputs to CacheSearchHashesResponse have been destructed.
  {
    std::vector<std::string> requested_hash_prefixes = {"aaaa"};
    std::vector<V5::FullHash> response_full_hashes = {
        CreateBasicFullHash(
            "aaaa1111111111111111111111111111",
            {V5::ThreatType::SOCIAL_ENGINEERING, V5::ThreatType::MALWARE,
             V5::ThreatType::UNWANTED_SOFTWARE, V5::ThreatType::API_ABUSE}),
    };
    cache->CacheSearchHashesResponse(requested_hash_prefixes,
                                     response_full_hashes,
                                     CreateCacheDuration(300, 0));
  }
  task_environment_.FastForwardBy(base::Seconds(299));
  {
    std::vector<std::string> requested_hash_prefixes = {"cccc"};
    std::vector<V5::FullHash> response_full_hashes = {
        CreateBasicFullHash("cccc1111111111111111111111111111",
                            {V5::ThreatType::MALWARE}),
    };
    cache->CacheSearchHashesResponse(requested_hash_prefixes,
                                     response_full_hashes,
                                     CreateCacheDuration(300, 0));
  }

  // aaaa expires at 300 seconds. cccc expires at 599 seconds.
  // Current time = 299 seconds. aaaa and cccc have not expired.
  EXPECT_FALSE(cache->SearchCache({"aaaa"}).empty());
  CheckAndResetCacheHitsAndMisses(/*num_hits=*/1, /*num_misses=*/0);
  EXPECT_FALSE(cache->SearchCache({"cccc"}).empty());
  CheckAndResetCacheHitsAndMisses(/*num_hits=*/1, /*num_misses=*/0);
  EXPECT_EQ(cache->SearchCache({"aaaa", "cccc"}).size(), 2u);
  CheckAndResetCacheHitsAndMisses(/*num_hits=*/2, /*num_misses=*/0);
  // Current time = 300 seconds. aaaa has expired. cccc has not expired.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(cache->SearchCache({"aaaa"}).empty());
  CheckAndResetCacheHitsAndMisses(/*num_hits=*/0, /*num_misses=*/1);
  EXPECT_FALSE(cache->SearchCache({"cccc"}).empty());
  CheckAndResetCacheHitsAndMisses(/*num_hits=*/1, /*num_misses=*/0);
  EXPECT_EQ(cache->SearchCache({"aaaa", "cccc"}).size(), 1u);
  CheckAndResetCacheHitsAndMisses(/*num_hits=*/1, /*num_misses=*/1);
  // Current time = 598 seconds. aaaa has expired. cccc has not expired.
  task_environment_.FastForwardBy(base::Seconds(298));
  EXPECT_TRUE(cache->SearchCache({"aaaa"}).empty());
  CheckAndResetCacheHitsAndMisses(/*num_hits=*/0, /*num_misses=*/1);
  EXPECT_FALSE(cache->SearchCache({"cccc"}).empty());
  CheckAndResetCacheHitsAndMisses(/*num_hits=*/1, /*num_misses=*/0);
  EXPECT_EQ(cache->SearchCache({"aaaa", "cccc"}).size(), 1u);
  CheckAndResetCacheHitsAndMisses(/*num_hits=*/1, /*num_misses=*/1);
  // Current time = 599 seconds. aaaa and cccc have expired.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(cache->SearchCache({"aaaa"}).empty());
  CheckAndResetCacheHitsAndMisses(/*num_hits=*/0, /*num_misses=*/1);
  EXPECT_TRUE(cache->SearchCache({"cccc"}).empty());
  CheckAndResetCacheHitsAndMisses(/*num_hits=*/0, /*num_misses=*/1);
  EXPECT_TRUE(cache->SearchCache({"aaaa", "cccc"}).empty());
  CheckAndResetCacheHitsAndMisses(/*num_hits=*/0, /*num_misses=*/2);
}

TEST_F(HashRealTimeCacheTest, TestCacheMatching_ExpirationNanos) {
  auto cache = std::make_unique<HashRealTimeCache>();
  // The below are done within blocks to ensure that the cache works even once
  // the inputs to CacheSearchHashesResponse have been destructed.
  {
    std::vector<std::string> requested_hash_prefixes = {"aaaa"};
    std::vector<V5::FullHash> response_full_hashes = {
        CreateBasicFullHash(
            "aaaa1111111111111111111111111111",
            {V5::ThreatType::SOCIAL_ENGINEERING, V5::ThreatType::MALWARE,
             V5::ThreatType::UNWANTED_SOFTWARE, V5::ThreatType::API_ABUSE}),
    };
    cache->CacheSearchHashesResponse(requested_hash_prefixes,
                                     response_full_hashes,
                                     CreateCacheDuration(300, 500000000));
  }
  task_environment_.FastForwardBy(base::Seconds(300));

  // aaaa expires at 300.5 seconds.
  // Current time = 300.0 seconds. aaaa has not expired.
  EXPECT_FALSE(cache->SearchCache({"aaaa"}).empty());
  CheckAndResetCacheHitsAndMisses(/*num_hits=*/1, /*num_misses=*/0);
  // Current time = 300.5 seconds. aaaa has expired.
  task_environment_.FastForwardBy(base::Nanoseconds(500000000));
  EXPECT_TRUE(cache->SearchCache({"aaaa"}).empty());
  CheckAndResetCacheHitsAndMisses(/*num_hits=*/0, /*num_misses=*/1);
}

TEST_F(HashRealTimeCacheTest, TestCacheMatching_Attributes) {
  auto cache = std::make_unique<HashRealTimeCache>();
  // The below is done within a block to ensure that the cache works even once
  // the inputs to CacheSearchHashesResponse have been destructed.
  {
    std::vector<std::string> requested_hash_prefixes = {"aaaa", "bbbb"};
    auto full_hash_1 =
        CreateBasicFullHash("aaaa1111111111111111111111111111", {});
    AddThreatTypeAndAttributes(full_hash_1, V5::ThreatType::SOCIAL_ENGINEERING,
                               {V5::ThreatAttribute::FRAME_ONLY});
    AddThreatTypeAndAttributes(full_hash_1, V5::ThreatType::MALWARE,
                               {V5::ThreatAttribute::FRAME_ONLY});
    AddThreatTypeAndAttributes(
        full_hash_1, V5::ThreatType::API_ABUSE,
        {V5::ThreatAttribute::CANARY, V5::ThreatAttribute::FRAME_ONLY});
    AddThreatTypeAndAttributes(full_hash_1, V5::ThreatType::UNWANTED_SOFTWARE,
                               {});
    std::vector<V5::FullHash> response_full_hashes = {
        full_hash_1, CreateBasicFullHash("aaaa2222222222222222222222222222",
                                         {V5::ThreatType::MALWARE})};
    cache->CacheSearchHashesResponse(requested_hash_prefixes,
                                     response_full_hashes,
                                     CreateCacheDuration(300, 0));
  }

  std::set<std::string> hash_prefixes_to_search = {"aaaa", "bbbb"};
  auto cache_results = cache->SearchCache(hash_prefixes_to_search);
  CheckAndResetCacheHitsAndMisses(/*num_hits=*/2, /*num_misses=*/0);

  // Sanity check that adding attributes for aaaa hashes does not change the
  // fact that there should be no bbbb full hashes / associated attributes.
  EXPECT_TRUE(base::Contains(cache_results, "bbbb"));
  EXPECT_TRUE(cache_results["bbbb"].empty());

  // We expect aaaa...1 and aaaa...2 both to be in the cache.
  EXPECT_EQ(cache_results["aaaa"].size(), 2u);
  // aaaa...1 should be filtered down to relevant threat types, meaning some
  // attributes get filtered out too since they are associated with a specific
  // threat type.
  auto aaaa1_results = cache_results["aaaa"][0];
  EXPECT_EQ(aaaa1_results.full_hash(), "aaaa1111111111111111111111111111");
  auto aaaa1_details = aaaa1_results.full_hash_details();
  EXPECT_EQ(aaaa1_details.size(), 3);
  EXPECT_EQ(aaaa1_details[0].threat_type(), V5::ThreatType::SOCIAL_ENGINEERING);
  EXPECT_EQ(aaaa1_details[0].attributes().size(), 1);
  EXPECT_EQ(aaaa1_details[0].attributes()[0], V5::ThreatAttribute::FRAME_ONLY);
  EXPECT_EQ(aaaa1_details[1].threat_type(), V5::ThreatType::MALWARE);
  EXPECT_EQ(aaaa1_details[1].attributes().size(), 1);
  EXPECT_EQ(aaaa1_details[1].attributes()[0], V5::ThreatAttribute::FRAME_ONLY);
  EXPECT_EQ(aaaa1_details[2].threat_type(), V5::ThreatType::UNWANTED_SOFTWARE);
  EXPECT_TRUE(aaaa1_details[2].attributes().empty());
  // Sanity check that aaaa...2 has no attributes in spite of aaaa...1 having
  // attributes.
  auto aaaa2_results = cache_results["aaaa"][1];
  EXPECT_EQ(aaaa2_results.full_hash(), "aaaa2222222222222222222222222222");
  auto aaaa2_details = aaaa2_results.full_hash_details();
  EXPECT_EQ(aaaa2_details.size(), 1);
  EXPECT_EQ(aaaa2_details[0].threat_type(), V5::ThreatType::MALWARE);
  EXPECT_TRUE(aaaa2_details[0].attributes().empty());
}

TEST_F(HashRealTimeCacheTest, TestCacheMatching_OverwrittenEntry) {
  auto cache = std::make_unique<HashRealTimeCache>();
  // The below are done within blocks to ensure that the cache works even once
  // the inputs to CacheSearchHashesResponse have been destructed.
  {
    // Set up the cache for Request #1.
    std::vector<std::string> requested_hash_prefixes = {"aaaa"};
    std::vector<V5::FullHash> response_full_hashes = {
        CreateBasicFullHash(
            "aaaa1111111111111111111111111111",
            {V5::ThreatType::SOCIAL_ENGINEERING, V5::ThreatType::MALWARE,
             V5::ThreatType::UNWANTED_SOFTWARE, V5::ThreatType::API_ABUSE}),
    };
    cache->CacheSearchHashesResponse(requested_hash_prefixes,
                                     response_full_hashes,
                                     CreateCacheDuration(300, 0));
  }
  // Confirm the cache has the expected results.
  auto cache_results_1 = cache->SearchCache({"aaaa"});
  CheckAndResetCacheHitsAndMisses(/*num_hits=*/1, /*num_misses=*/0);
  EXPECT_EQ(cache_results_1.size(), 1u);
  EXPECT_EQ(cache_results_1["aaaa"].size(), 1u);
  EXPECT_EQ(cache_results_1["aaaa"][0].full_hash(),
            "aaaa1111111111111111111111111111");
  EXPECT_EQ(cache_results_1["aaaa"][0].full_hash_details_size(), 3);

  {
    // Set up the cache for Request #2, overwriting the results of Request #1.
    std::vector<std::string> requested_hash_prefixes = {"aaaa"};
    std::vector<V5::FullHash> response_full_hashes = {
        CreateBasicFullHash("aaaa2222222222222222222222222222",
                            {V5::ThreatType::MALWARE}),
    };
    cache->CacheSearchHashesResponse(requested_hash_prefixes,
                                     response_full_hashes,
                                     CreateCacheDuration(300, 0));
  }

  // If there is a race where there are two outgoing hash-prefix real-time
  // requests for the same prefix, the later-responding result replaces the
  // earlier-responding result. In practice, the two results are expected to be
  // the same almost always, but if they are not, this is how the cache behaves.
  auto cache_results_2 = cache->SearchCache({"aaaa"});
  CheckAndResetCacheHitsAndMisses(/*num_hits=*/1, /*num_misses=*/0);
  EXPECT_EQ(cache_results_2.size(), 1u);
  EXPECT_EQ(cache_results_2["aaaa"].size(), 1u);
  EXPECT_EQ(cache_results_2["aaaa"][0].full_hash(),
            "aaaa2222222222222222222222222222");
  EXPECT_EQ(cache_results_2["aaaa"][0].full_hash_details_size(), 1);

  task_environment_.FastForwardBy(base::Seconds(150));
  {
    // Set up the cache for Request #3, overwriting the results of Request #2.
    // The main overwriting here is just the cache duration, since 150 seconds
    // have passed.
    std::vector<std::string> requested_hash_prefixes = {"aaaa"};
    std::vector<V5::FullHash> response_full_hashes = {
        CreateBasicFullHash("aaaa2222222222222222222222222222",
                            {V5::ThreatType::MALWARE}),
    };
    cache->CacheSearchHashesResponse(requested_hash_prefixes,
                                     response_full_hashes,
                                     CreateCacheDuration(300, 0));
  }

  // Confirm caching Request #3 overwrote the cache duration. If it didn't, then
  // the results of Request #2 would already have expired.
  task_environment_.FastForwardBy(base::Seconds(150));
  EXPECT_FALSE(cache->SearchCache({"aaaa"}).empty());
  CheckAndResetCacheHitsAndMisses(/*num_hits=*/1, /*num_misses=*/0);

  // Confirm Request #3's cache duration is respected.
  task_environment_.FastForwardBy(base::Seconds(149));
  EXPECT_FALSE(cache->SearchCache({"aaaa"}).empty());
  CheckAndResetCacheHitsAndMisses(/*num_hits=*/1, /*num_misses=*/0);
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(cache->SearchCache({"aaaa"}).empty());
  CheckAndResetCacheHitsAndMisses(/*num_hits=*/0, /*num_misses=*/1);
}

TEST_F(HashRealTimeCacheTest, TestCacheMatching_CacheDurationLogging) {
  auto cache = std::make_unique<HashRealTimeCache>();
  std::vector<std::string> requested_hash_prefixes = {"aaaa"};
  std::vector<V5::FullHash> response_full_hashes = {
      CreateBasicFullHash("aaaa1111111111111111111111111111",
                          {V5::ThreatType::SOCIAL_ENGINEERING}),
  };
  cache->CacheSearchHashesResponse(requested_hash_prefixes,
                                   response_full_hashes,
                                   CreateCacheDuration(300, 0));
  CheckAndResetCacheDurationLogs(
      /*initial_cache_duration_sec=*/300,
      /*remaining_cache_duration_sec=*/std::nullopt);

  cache->SearchCache({"aaaa"});
  CheckAndResetCacheDurationLogs(/*initial_cache_duration_sec=*/std::nullopt,
                                 /*remaining_cache_duration_sec=*/300);
  task_environment_.FastForwardBy(base::Seconds(299));
  cache->SearchCache({"aaaa"});
  CheckAndResetCacheDurationLogs(/*initial_cache_duration_sec=*/std::nullopt,
                                 /*remaining_cache_duration_sec=*/1);
  task_environment_.FastForwardBy(base::Seconds(1));
  cache->SearchCache({"aaaa"});
  CheckAndResetCacheDurationLogs(
      /*initial_cache_duration_sec=*/std::nullopt,
      /*remaining_cache_duration_sec=*/std::nullopt);
}

TEST_F(HashRealTimeCacheTest, TestClearExpiredResults_EmptyCache) {
  auto cache = std::make_unique<HashRealTimeCache>();
  EXPECT_EQ(GetNumCacheEntries(cache), 0);
  cache->ClearExpiredResults();
  EXPECT_EQ(GetNumCacheEntries(cache), 0);
}

TEST_F(HashRealTimeCacheTest, TestClearExpiredResults_NoExpiredResults) {
  auto cache = std::make_unique<HashRealTimeCache>();
  CacheEntry(cache, "aaaa1111111111111111111111111111", 300);
  CacheEntry(cache, "cccc1111111111111111111111111111", 500);

  EXPECT_EQ(GetNumCacheEntries(cache), 2);
  EXPECT_TRUE(base::Contains(cache->SearchCache({"aaaa"}), "aaaa"));
  EXPECT_TRUE(base::Contains(cache->SearchCache({"cccc"}), "cccc"));
  cache->ClearExpiredResults();
  EXPECT_EQ(GetNumCacheEntries(cache), 2);
  EXPECT_TRUE(base::Contains(cache->SearchCache({"aaaa"}), "aaaa"));
  EXPECT_TRUE(base::Contains(cache->SearchCache({"cccc"}), "cccc"));
}

TEST_F(HashRealTimeCacheTest, TestClearExpiredResults_OneExpiredResult) {
  auto cache = std::make_unique<HashRealTimeCache>();
  CacheEntry(cache, "aaaa1111111111111111111111111111", 300);
  CacheEntry(cache, "cccc1111111111111111111111111111", 500);

  // After 400 seconds, aaaa is expired but not cccc.
  task_environment_.FastForwardBy(base::Seconds(400));
  EXPECT_EQ(GetNumCacheEntries(cache), 2);
  EXPECT_FALSE(base::Contains(cache->SearchCache({"aaaa"}), "aaaa"));
  EXPECT_TRUE(base::Contains(cache->SearchCache({"cccc"}), "cccc"));
  cache->ClearExpiredResults();
  EXPECT_EQ(GetNumCacheEntries(cache), 1);
  EXPECT_FALSE(base::Contains(cache->SearchCache({"aaaa"}), "aaaa"));
  EXPECT_TRUE(base::Contains(cache->SearchCache({"cccc"}), "cccc"));
}

TEST_F(HashRealTimeCacheTest, TestClearExpiredResults_SomeExpiredResults) {
  auto cache = std::make_unique<HashRealTimeCache>();
  auto soon = 300;
  auto later = 500;
  CacheEntry(cache, "aaaa1111111111111111111111111111", soon);
  CacheEntry(cache, "bbbb1111111111111111111111111111", later);
  CacheEntry(cache, "cccc1111111111111111111111111111", soon);
  CacheEntry(cache, "dddd1111111111111111111111111111", soon);
  CacheEntry(cache, "eeee1111111111111111111111111111", soon);
  CacheEntry(cache, "ffff1111111111111111111111111111", later);
  CacheEntry(cache, "gggg1111111111111111111111111111", later);
  CacheEntry(cache, "hhhh1111111111111111111111111111", soon);

  auto validate_cache_contents = [](std::unique_ptr<HashRealTimeCache>&
                                        cache_internal) {
    EXPECT_FALSE(base::Contains(cache_internal->SearchCache({"aaaa"}), "aaaa"));
    EXPECT_TRUE(base::Contains(cache_internal->SearchCache({"bbbb"}), "bbbb"));
    EXPECT_FALSE(base::Contains(cache_internal->SearchCache({"cccc"}), "cccc"));
    EXPECT_FALSE(base::Contains(cache_internal->SearchCache({"dddd"}), "dddd"));
    EXPECT_FALSE(base::Contains(cache_internal->SearchCache({"eeee"}), "eeee"));
    EXPECT_TRUE(base::Contains(cache_internal->SearchCache({"ffff"}), "ffff"));
    EXPECT_TRUE(base::Contains(cache_internal->SearchCache({"gggg"}), "gggg"));
    EXPECT_FALSE(base::Contains(cache_internal->SearchCache({"hhhh"}), "hhhh"));
  };

  // After 400 seconds, all of the "soon" prefixes have expired, and none of the
  // "later" prefixes have.
  task_environment_.FastForwardBy(base::Seconds(400));
  EXPECT_EQ(GetNumCacheEntries(cache), 8);
  validate_cache_contents(cache);
  cache->ClearExpiredResults();
  EXPECT_EQ(GetNumCacheEntries(cache), 3);
  validate_cache_contents(cache);
}

TEST_F(HashRealTimeCacheTest,
       TestClearExpiredResults_SomeExpiredResultsReversed) {
  // The main difference between TestClearExpiredResults_SomeExpiredResults
  // above and this one is that whether an entry is expired is reversed. This is
  // to confirm that the iterative deletion in ClearExpiredResults works as
  // expected regardless of ordering.
  auto cache = std::make_unique<HashRealTimeCache>();
  auto soon = 300;
  auto later = 500;
  CacheEntry(cache, "aaaa1111111111111111111111111111", later);
  CacheEntry(cache, "bbbb1111111111111111111111111111", soon);
  CacheEntry(cache, "cccc1111111111111111111111111111", later);
  CacheEntry(cache, "dddd1111111111111111111111111111", later);
  CacheEntry(cache, "eeee1111111111111111111111111111", later);
  CacheEntry(cache, "ffff1111111111111111111111111111", soon);
  CacheEntry(cache, "gggg1111111111111111111111111111", soon);
  CacheEntry(cache, "hhhh1111111111111111111111111111", later);

  auto validate_cache_contents = [](std::unique_ptr<HashRealTimeCache>&
                                        cache_internal) {
    EXPECT_TRUE(base::Contains(cache_internal->SearchCache({"aaaa"}), "aaaa"));
    EXPECT_FALSE(base::Contains(cache_internal->SearchCache({"bbbb"}), "bbbb"));
    EXPECT_TRUE(base::Contains(cache_internal->SearchCache({"cccc"}), "cccc"));
    EXPECT_TRUE(base::Contains(cache_internal->SearchCache({"dddd"}), "dddd"));
    EXPECT_TRUE(base::Contains(cache_internal->SearchCache({"eeee"}), "eeee"));
    EXPECT_FALSE(base::Contains(cache_internal->SearchCache({"ffff"}), "ffff"));
    EXPECT_FALSE(base::Contains(cache_internal->SearchCache({"gggg"}), "gggg"));
    EXPECT_TRUE(base::Contains(cache_internal->SearchCache({"hhhh"}), "hhhh"));
  };

  // After 400 seconds, all of the "soon" prefixes have expired, and none of the
  // "later" prefixes have.
  task_environment_.FastForwardBy(base::Seconds(400));
  EXPECT_EQ(GetNumCacheEntries(cache), 8);
  validate_cache_contents(cache);
  cache->ClearExpiredResults();
  EXPECT_EQ(GetNumCacheEntries(cache), 5);
  validate_cache_contents(cache);
}

TEST_F(HashRealTimeCacheTest, TestClearExpiredResults_AllExpiredResults) {
  auto cache = std::make_unique<HashRealTimeCache>();
  CacheEntry(cache, "aaaa1111111111111111111111111111", 300);
  CacheEntry(cache, "cccc1111111111111111111111111111", 500);

  // After 500 seconds, both have expired.
  task_environment_.FastForwardBy(base::Seconds(500));
  EXPECT_EQ(GetNumCacheEntries(cache), 2);
  EXPECT_FALSE(base::Contains(cache->SearchCache({"aaaa"}), "aaaa"));
  EXPECT_FALSE(base::Contains(cache->SearchCache({"cccc"}), "cccc"));
  cache->ClearExpiredResults();
  EXPECT_EQ(GetNumCacheEntries(cache), 0);
  EXPECT_FALSE(base::Contains(cache->SearchCache({"aaaa"}), "aaaa"));
  EXPECT_FALSE(base::Contains(cache->SearchCache({"cccc"}), "cccc"));
}

TEST_F(HashRealTimeCacheTest, TestClearExpiredResults_Logging) {
  auto cache = std::make_unique<HashRealTimeCache>();

  // Cache is empty.
  cache->ClearExpiredResults();
  CheckAndResetCacheSizeOnClear(/*num_hash_prefixes=*/0, /*num_full_hashes=*/0);

  // Cache has 1 hash prefix with 1 full hash in it.
  cache->CacheSearchHashesResponse(
      {"aaaa"},
      {CreateBasicFullHash("aaaa1111111111111111111111111111",
                           {V5::ThreatType::MALWARE})},
      CreateCacheDuration(300, 0));
  cache->ClearExpiredResults();
  CheckAndResetCacheSizeOnClear(/*num_hash_prefixes=*/1, /*num_full_hashes=*/1);

  // Cache has 2 hash prefixes and 3 full hashes (aaaa entry from above remains
  // included).
  cache->CacheSearchHashesResponse(
      {"bbbb"},
      {CreateBasicFullHash("bbbb1111111111111111111111111111",
                           {V5::ThreatType::MALWARE}),
       CreateBasicFullHash("bbbb2222222222222222222222222222",
                           {V5::ThreatType::MALWARE})},
      CreateCacheDuration(500, 0));
  cache->ClearExpiredResults();
  CheckAndResetCacheSizeOnClear(/*num_hash_prefixes=*/2, /*num_full_hashes=*/3);

  // 400 seconds later, the first addition to the cache has expired. The logs
  // should still report 2 hash prefixes and 3 full hashes, because they report
  // the size at the time the cache started being cleared, not afterwards.
  task_environment_.FastForwardBy(base::Seconds(400));
  cache->ClearExpiredResults();
  CheckAndResetCacheSizeOnClear(/*num_hash_prefixes=*/2, /*num_full_hashes=*/3);

  // Clearing the expired results again now displays the size with just the
  // second addition to the cache.
  cache->ClearExpiredResults();
  CheckAndResetCacheSizeOnClear(/*num_hash_prefixes=*/1, /*num_full_hashes=*/2);

  // 100 seconds later, the second addition to the cache has expired. The log
  // still includes it in the size (same rationale as above).
  task_environment_.FastForwardBy(base::Seconds(100));
  cache->ClearExpiredResults();
  CheckAndResetCacheSizeOnClear(/*num_hash_prefixes=*/1, /*num_full_hashes=*/2);

  // Clearing the expired results again now logs that the cache is empty.
  cache->ClearExpiredResults();
  CheckAndResetCacheSizeOnClear(/*num_hash_prefixes=*/0, /*num_full_hashes=*/0);
}

}  // namespace safe_browsing
