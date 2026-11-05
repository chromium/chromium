// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/printer_config_cache.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/run_until.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Maintainer's notes:
//
// 1. The use of base::Unretained throughout this suite is appropriate
//    because the sequences of each test live as long as the test does.
//    Real consumers probably can't do this.
// 2. The passage of time is controlled by a mock clock, so Fetch()
//    invocations not preceded by advancing the clock past kMaxAge never
//    hit the "networked fetch" codepath for already cached resources.

namespace chromeos {
namespace {

// Defines some resources (URLs and contents) used throughout this
// test suite.

// Name of the "known-good" resource.
const char kKnownGoodResourceURL[] =
    "https://printerconfigurations.googleusercontent.com/chromeos_printing/"
    "known-good";

// Arbitrary content for the "known-good" resource.
const char kKnownGoodResourceContent[] = "yakisaba";

// Name of the "known-bad" resource.
const char kKnownBadResourceURL[] =
    "https://printerconfigurations.googleusercontent.com/chromeos_printing/"
    "known-bad";

// Defines an arbitrary time increment by which we advance the Clock.
constexpr base::TimeDelta kTestingIncrement = base::Seconds(1LL);

// Maximum age of cached entries for the class under test.
constexpr base::TimeDelta kMaxAge = base::Seconds(32LL);

// Defines a time of fetch used to construct FetchResult instances that
// you'll use with the TimeInsensitiveFetchResultEquals matcher.
constexpr base::Time kUnusedTimeOfFetch;

MATCHER_P(TimeInsensitiveFetchResultEquals, expected, "") {
  return arg.succeeded == expected.succeeded && arg.key == expected.key &&
         arg.contents == expected.contents;
}

MATCHER_P(FetchResultEquals, expected, "") {
  return arg.succeeded == expected.succeeded && arg.key == expected.key &&
         arg.contents == expected.contents &&
         arg.time_of_fetch == expected.time_of_fetch;
}

class PrinterConfigCacheTest : public ::testing::Test {
 public:
  // Creates |this| with
  // *  a testing task environment for testing sequenced code,
  // *  a testing clock for time-aware testing, and
  // *  a testing URLLoaderFactory loaned to the class under test.
  PrinterConfigCacheTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        cache_(PrinterConfigCache::Create(&clock_,
                                          kMaxAge,
                                          loader_factory_.GetSafeWeakWrapper(),
                                          /*use_localhost_as_root=*/false)) {}

  // Sets up the default responses to dispense.
  void SetUp() override {
    // Dispenses the "known-good" resource with its content.
    loader_factory_.AddResponse(kKnownGoodResourceURL,
                                kKnownGoodResourceContent);

    // Dispenses the "known-bad" resource with no content and an
    // arbitrary HTTP error.
    loader_factory_.AddResponse(kKnownBadResourceURL, "",
                                net::HTTP_NOT_ACCEPTABLE);
  }

  // Method passed as a FetchCallback (partially bound) to
  // cache_.Fetch(). Saves the |result| in the |fetched_results_|.
  void CaptureFetchResult(const PrinterConfigCache::FetchResult& result) {
    fetched_results_.push_back(result);
  }

  void AdvanceClock(base::TimeDelta amount = kTestingIncrement) {
    clock_.Advance(amount);
  }

 protected:
  // Landing area used to collect Fetch()ed results.
  std::vector<PrinterConfigCache::FetchResult> fetched_results_;

  // Loader factory for testing loaned to |cache_|.
  network::TestURLLoaderFactory loader_factory_;

  // Environment for task schedulers.
  base::test::TaskEnvironment task_environment_;

  // Controlled clock that dispenses times of Fetch().
  base::SimpleTestClock clock_;

  // Class under test.
  std::unique_ptr<PrinterConfigCache> cache_;
};

// Tests that we can succeed in Fetch()ing anything at all.
TEST_F(PrinterConfigCacheTest, SucceedAtSingleFetch) {
  base::test::TestFuture<const PrinterConfigCache::FetchResult&> test_future;

  // Fetches the "known-good" resource.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PrinterConfigCache::Fetch, base::Unretained(cache_.get()),
                     "known-good", test_future.GetCallback()));

  EXPECT_THAT(
      test_future.Get(),
      TimeInsensitiveFetchResultEquals(PrinterConfigCache::FetchResult::Success(
          "known-good", "yakisaba", kUnusedTimeOfFetch)));
}

// Tests that we fail to Fetch() the "known-bad" resource.
TEST_F(PrinterConfigCacheTest, FailAtSingleFetch) {
  base::test::TestFuture<const PrinterConfigCache::FetchResult&> test_future;

  // Fetches the "known-bad" resource.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PrinterConfigCache::Fetch, base::Unretained(cache_.get()),
                     "known-bad", test_future.GetCallback()));

  EXPECT_THAT(test_future.Get(),
              TimeInsensitiveFetchResultEquals(
                  PrinterConfigCache::FetchResult::Failure("known-bad")));
}

// Tests that a Fetch() after the cached entry is older than kMaxAge
// performs a networked fetch.
TEST_F(PrinterConfigCacheTest, RefreshSubsequentFetch) {
  // Fetches the "known-good" resource with its stock contents.
  base::test::TestFuture<const PrinterConfigCache::FetchResult&>
      first_test_future;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PrinterConfigCache::Fetch, base::Unretained(cache_.get()),
                     "known-good", first_test_future.GetCallback()));
  fetched_results_.push_back(first_test_future.Get());

  // To detect a networked fetch, we'll change the served content
  // and check that the subsequent Fetch() recovers the new content.
  loader_factory_.AddResponse(kKnownGoodResourceURL, "one Argentinian peso");

  // We've mutated the content and let the cached entry expire; now, this
  // fetches the "known-good" resource with its new contents.
  AdvanceClock(kMaxAge);
  base::test::TestFuture<const PrinterConfigCache::FetchResult&>
      second_test_future;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PrinterConfigCache::Fetch, base::Unretained(cache_.get()),
                     "known-good", second_test_future.GetCallback()));
  fetched_results_.push_back(second_test_future.Get());

  EXPECT_THAT(
      fetched_results_,
      testing::ElementsAre(
          TimeInsensitiveFetchResultEquals(
              PrinterConfigCache::FetchResult::Success("known-good", "yakisaba",
                                                       kUnusedTimeOfFetch)),
          TimeInsensitiveFetchResultEquals(
              PrinterConfigCache::FetchResult::Success(
                  "known-good", "one Argentinian peso", kUnusedTimeOfFetch))));
}

// Tests that we can Fetch() locally cached contents while they are
// younger than kMaxAge.
TEST_F(PrinterConfigCacheTest, LocallyPerformSubsequentFetch) {
  // Fetches the "known-good" resource with its stock contents.
  base::test::TestFuture<const PrinterConfigCache::FetchResult&>
      first_test_future;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PrinterConfigCache::Fetch, base::Unretained(cache_.get()),
                     "known-good", first_test_future.GetCallback()));
  fetched_results_.push_back(first_test_future.Get());

  // As in the RefreshSubsequentFetch test, we'll change the served
  // content to detect networked fetch requests made.
  loader_factory_.AddResponse(kKnownGoodResourceURL, "apologize darn you");

  // The "live" content in the serving root has changed; now, we perform
  // some local fetches without hitting the network. These Fetch()es
  // will return the stock content.
  base::test::TestFuture<const PrinterConfigCache::FetchResult&>
      second_test_future;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PrinterConfigCache::Fetch, base::Unretained(cache_.get()),
                     "known-good",
                     // Avoids hitting the network because the clock has
                     // not advanced since the first Fetch().
                     base::BindOnce(&PrinterConfigCacheTest::CaptureFetchResult,
                                    base::Unretained(this))));

  // Performs a local Fetch() a few more times for no particular reason.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PrinterConfigCache::Fetch, base::Unretained(cache_.get()),
                     "known-good",
                     base::BindOnce(&PrinterConfigCacheTest::CaptureFetchResult,
                                    base::Unretained(this))));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PrinterConfigCache::Fetch, base::Unretained(cache_.get()),
                     "known-good",
                     base::BindOnce(&PrinterConfigCacheTest::CaptureFetchResult,
                                    base::Unretained(this))));

  // Waits for the three local Fetch()es above to complete, then lets the
  // cached entry expire so the next Fetch() is networked.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return fetched_results_.size() == 4; }));
  AdvanceClock(kMaxAge);

  // Performs a live Fetch(), returning the live (mutated) contents.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PrinterConfigCache::Fetch, base::Unretained(cache_.get()),
                     "known-good", second_test_future.GetCallback()));
  fetched_results_.push_back(second_test_future.Get());

  ASSERT_EQ(fetched_results_.size(), 5ULL);
  EXPECT_THAT(
      fetched_results_,
      testing::ElementsAre(
          TimeInsensitiveFetchResultEquals(
              PrinterConfigCache::FetchResult::Success("known-good", "yakisaba",
                                                       kUnusedTimeOfFetch)),
          TimeInsensitiveFetchResultEquals(
              PrinterConfigCache::FetchResult::Success("known-good", "yakisaba",
                                                       kUnusedTimeOfFetch)),
          TimeInsensitiveFetchResultEquals(
              PrinterConfigCache::FetchResult::Success("known-good", "yakisaba",
                                                       kUnusedTimeOfFetch)),
          TimeInsensitiveFetchResultEquals(
              PrinterConfigCache::FetchResult::Success("known-good", "yakisaba",
                                                       kUnusedTimeOfFetch)),
          TimeInsensitiveFetchResultEquals(
              PrinterConfigCache::FetchResult::Success(
                  "known-good", "apologize darn you", kUnusedTimeOfFetch))));
}

// Tests that Fetch() respects the max age passed to Create(). This is a
// purely time-bound variation on the LocallyPerformSubsequentFetch
// test; the served content doesn't change between RunLoops.
TEST_F(PrinterConfigCacheTest, FetchExpirationIsRespected) {
  // There are no locally resident cache entries at this time; this
  // Fetch() has to be networked.
  base::test::TestFuture<const PrinterConfigCache::FetchResult&>
      first_test_future;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PrinterConfigCache::Fetch, base::Unretained(cache_.get()),
                     "known-good", first_test_future.GetCallback()));
  fetched_results_.push_back(first_test_future.Get());
  const base::Time time_zero = clock_.Now();

  // Advance clock to T+31.
  AdvanceClock(base::Seconds(31LL));

  // The clock does not yet indicate that the locally resident cache
  // entry has expired.
  base::test::TestFuture<const PrinterConfigCache::FetchResult&>
      second_test_future;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PrinterConfigCache::Fetch, base::Unretained(cache_.get()),
                     "known-good", second_test_future.GetCallback()));
  fetched_results_.push_back(second_test_future.Get());
  // We don't capture the time right Now() because the above Fetch()
  // should have replied with local contents, fetched at time_zero.

  // Advance clock to T+32.
  AdvanceClock(base::Seconds(1));

  // The two previous calls to AdvanceClock() will have moved the time
  // beyond kMaxAge, so this Fetch() will be networked.
  base::test::TestFuture<const PrinterConfigCache::FetchResult&>
      third_test_future;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PrinterConfigCache::Fetch,
                                base::Unretained(cache_.get()), "known-good",
                                // Entry fetched at T+0 is now stale at T+32.
                                third_test_future.GetCallback()));
  fetched_results_.push_back(third_test_future.Get());
  const base::Time time_of_third_fetch = clock_.Now();

  EXPECT_THAT(fetched_results_,
              testing::ElementsAre(
                  FetchResultEquals(PrinterConfigCache::FetchResult::Success(
                      "known-good", "yakisaba", time_zero)),
                  FetchResultEquals(PrinterConfigCache::FetchResult::Success(
                      "known-good", "yakisaba", time_zero)),
                  FetchResultEquals(PrinterConfigCache::FetchResult::Success(
                      "known-good", "yakisaba", time_of_third_fetch))));
}

// Tests that we can Drop() locally cached contents.
TEST_F(PrinterConfigCacheTest, DropLocalContents) {
  base::test::TestFuture<const PrinterConfigCache::FetchResult&>
      first_test_future;

  // Fetches the "known-good" resource with its stock contents.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PrinterConfigCache::Fetch, base::Unretained(cache_.get()),
                     "known-good", first_test_future.GetCallback()));
  fetched_results_.push_back(first_test_future.Get());

  // Drops that which we just fetched. This isn't immediately externally
  // visible, but its effects will soon be made apparent.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PrinterConfigCache::Drop,
                                base::Unretained(cache_.get()), "known-good"));

  // Mutates the contents served for the "known-good" resource.
  loader_factory_.AddResponse(kKnownGoodResourceURL, "ultimate dogeza");

  // Fetches the "known-good" resource anew without advancing the clock.
  // This is where the side effect of the prior Drop() call manifests:
  // the "known-good" resource is no longer cached, so even a fresh
  // entry will not spare us a networked fetch.
  base::test::TestFuture<const PrinterConfigCache::FetchResult&>
      second_test_future;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PrinterConfigCache::Fetch, base::Unretained(cache_.get()),
                     "known-good", second_test_future.GetCallback()));
  fetched_results_.push_back(second_test_future.Get());

  // We detect the networked fetch to by observing mutated
  // contents.
  ASSERT_EQ(fetched_results_.size(), 2ULL);
  EXPECT_THAT(
      fetched_results_,
      testing::ElementsAre(
          TimeInsensitiveFetchResultEquals(
              PrinterConfigCache::FetchResult::Success("known-good", "yakisaba",
                                                       kUnusedTimeOfFetch)),
          TimeInsensitiveFetchResultEquals(
              PrinterConfigCache::FetchResult::Success(
                  "known-good", "ultimate dogeza", kUnusedTimeOfFetch))));
}

TEST(PrinterConfigCacheIdleEvictionTest, EvictsExpiredEntriesWhenIdle) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  network::TestURLLoaderFactory loader_factory;
  loader_factory.AddResponse(kKnownGoodResourceURL, kKnownGoodResourceContent);
  loader_factory.AddResponse(
      "https://printerconfigurations.googleusercontent.com/chromeos_printing/"
      "second-good",
      "takoyaki");

  auto cache = PrinterConfigCache::Create(task_environment.GetMockClock(),
                                          base::Minutes(30),
                                          loader_factory.GetSafeWeakWrapper(),
                                          /*use_localhost_as_root=*/false);

  base::test::TestFuture<const PrinterConfigCache::FetchResult&> first_future;
  cache->Fetch("known-good", first_future.GetCallback());
  ASSERT_TRUE(first_future.Get().succeeded);
  EXPECT_EQ(cache->SizeForTesting(), 1u);

  // Fetch a second resource 10 minutes later so expiration times are staggered.
  task_environment.FastForwardBy(base::Minutes(10));
  base::test::TestFuture<const PrinterConfigCache::FetchResult&> second_future;
  cache->Fetch("second-good", second_future.GetCallback());
  ASSERT_TRUE(second_future.Get().succeeded);
  EXPECT_EQ(cache->SizeForTesting(), 2u);

  // Both entries remain cached at T+29m.
  task_environment.FastForwardBy(base::Minutes(19));
  EXPECT_EQ(cache->SizeForTesting(), 2u);

  // At T+30m, the first entry expires while the second remains cached.
  task_environment.FastForwardBy(base::Minutes(1));
  EXPECT_EQ(cache->SizeForTesting(), 1u);

  // At T+40m, the second entry expires as well.
  task_environment.FastForwardBy(base::Minutes(10));
  EXPECT_EQ(cache->SizeForTesting(), 0u);
}

TEST(PrinterConfigCacheIdleEvictionTest, ReschedulesAfterEarlyCleanup) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  network::TestURLLoaderFactory loader_factory;
  loader_factory.AddResponse(kKnownGoodResourceURL, kKnownGoodResourceContent);
  loader_factory.AddResponse(
      "https://printerconfigurations.googleusercontent.com/chromeos_printing/"
      "second-good",
      "takoyaki");

  auto cache = PrinterConfigCache::Create(task_environment.GetMockClock(),
                                          base::Minutes(30),
                                          loader_factory.GetSafeWeakWrapper(),
                                          /*use_localhost_as_root=*/false);

  // Fetches "known-good" at T+0, which schedules cleanup for T+30m.
  base::test::TestFuture<const PrinterConfigCache::FetchResult&> first_future;
  cache->Fetch("known-good", first_future.GetCallback());
  ASSERT_TRUE(first_future.Get().succeeded);

  // Fetches "second-good" at T+5m.
  task_environment.FastForwardBy(base::Minutes(5));
  base::test::TestFuture<const PrinterConfigCache::FetchResult&> second_future;
  cache->Fetch("second-good", second_future.GetCallback());
  ASSERT_TRUE(second_future.Get().succeeded);

  // Drops and refetches "known-good" at T+10m, so the oldest entry is now
  // "second-good" (expiring at T+35m) while the timer is still set for
  // T+30m.
  task_environment.FastForwardBy(base::Minutes(5));
  cache->Drop("known-good");
  base::test::TestFuture<const PrinterConfigCache::FetchResult&> third_future;
  cache->Fetch("known-good", third_future.GetCallback());
  ASSERT_TRUE(third_future.Get().succeeded);
  EXPECT_EQ(cache->SizeForTesting(), 2u);

  // The timer fires early at T+30m without evicting anything.
  task_environment.FastForwardBy(base::Minutes(20));
  EXPECT_EQ(cache->SizeForTesting(), 2u);

  // The rescheduled timer evicts "second-good" at T+35m and "known-good"
  // at T+40m.
  task_environment.FastForwardBy(base::Minutes(5));
  EXPECT_EQ(cache->SizeForTesting(), 1u);
  task_environment.FastForwardBy(base::Minutes(5));
  EXPECT_EQ(cache->SizeForTesting(), 0u);
}

}  // namespace
}  // namespace chromeos
