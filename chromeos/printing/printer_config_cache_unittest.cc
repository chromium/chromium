// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/printer_config_cache.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "net/http/http_status_code.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Maintainer's notes:
//
// 1. The use of base::Unretained throughout this suite is appropriate
//    because the sequences of each test live as long as the test does.
//    Real consumers probably can't do this.
// 2. The passage of time is controlled by a mock clock, so most Fetch()
//    invocations not preceded by clock advancement never hit the
//    "networked fetch" codepath. In such tests, the values of the
//    TimeDelta argument are arbitrary and meaningless.

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
  // *  a loader factory dispenser (specified by header comment on
  //    Create()).
  PrinterConfigCacheTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        cache_(PrinterConfigCache::Create(
            &clock_,
            base::BindLambdaForTesting([&]() {
              return reinterpret_cast<network::mojom::URLLoaderFactory*>(
                  &loader_factory_);
            }),
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
  // Invokes the |quit_closure| to signal the enclosing RunLoop that
  // this method has been called.
  void CaptureFetchResult(base::RepeatingClosure quit_closure,
                          const PrinterConfigCache::FetchResult& result) {
    fetched_results_.push_back(result);

    // The caller may elect to pass a default-constructed
    // RepeatingClosure, indicating that they don't want anything run.
    if (quit_closure) {
      quit_closure.Run();
    }
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
  base::RunLoop run_loop;

  // Fetches the "known-good" resource.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &PrinterConfigCache::Fetch, base::Unretained(cache_.get()),
          "known-good", base::Seconds(0LL),
          base::BindOnce(&PrinterConfigCacheTest::CaptureFetchResult,
                         base::Unretained(this), run_loop.QuitClosure())));
  run_loop.Run();

  ASSERT_EQ(fetched_results_.size(), 1ULL);
  EXPECT_THAT(
      fetched_results_.front(),
      TimeInsensitiveFetchResultEquals(PrinterConfigCache::FetchResult::Success(
          "known-good", "yakisaba", kUnusedTimeOfFetch)));
}

// Tests that we fail to Fetch() the "known-bad" resource.
TEST_F(PrinterConfigCacheTest, FailAtSingleFetch) {
  base::RunLoop run_loop;

  // Fetches the "known-bad" resource.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &PrinterConfigCache::Fetch, base::Unretained(cache_.get()),
          "known-bad", base::Seconds(0LL),
          base::BindOnce(&PrinterConfigCacheTest::CaptureFetchResult,
                         base::Unretained(this), run_loop.QuitClosure())));
  run_loop.Run();

  ASSERT_EQ(fetched_results_.size(), 1ULL);
  EXPECT_THAT(fetched_results_.front(),
              TimeInsensitiveFetchResultEquals(
                  PrinterConfigCache::FetchResult::Failure("known-bad")));
}

// Tests that we can force a networked Fetch() by demanding
// fresh content.
TEST_F(PrinterConfigCacheTest, RefreshSubsequentFetch) {
  // Fetches the "known-good" resource with its stock contents.
  base::RunLoop first_run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PrinterConfigCache::Fetch, base::Unretained(cache_.get()),
                     "known-good", base::Seconds(0LL),
                     base::BindOnce(&PrinterConfigCacheTest::CaptureFetchResult,
                                    base::Unretained(this),
                                    first_run_loop.QuitClosure())));
  first_run_loop.Run();

  ASSERT_EQ(fetched_results_.size(), 1ULL);

  // To detect a networked fetch, we'll change the served content
  // and check that the subsequent Fetch() recovers the new content.
  loader_factory_.AddResponse(kKnownGoodResourceURL, "one Argentinian peso");

  // We've mutated the content; now, this fetches the "known-good"
  // resource with its new contents.
  base::RunLoop second_run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PrinterConfigCache::Fetch, base::Unretained(cache_.get()),
                     "known-good", base::Seconds(0LL),
                     base::BindOnce(&PrinterConfigCacheTest::CaptureFetchResult,
                                    base::Unretained(this),
                                    second_run_loop.QuitClosure())));
  second_run_loop.Run();

  ASSERT_EQ(fetched_results_.size(), 2ULL);

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

// Tests that we can Fetch() locally cached contents by specifying a
// wide age limit.
TEST_F(PrinterConfigCacheTest, LocallyPerformSubsequentFetch) {
  // Fetches the "known-good" resource with its stock contents.
  base::RunLoop first_run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PrinterConfigCache::Fetch, base::Unretained(cache_.get()),
                     "known-good", base::Seconds(0LL),
                     base::BindOnce(&PrinterConfigCacheTest::CaptureFetchResult,
                                    base::Unretained(this),
                                    first_run_loop.QuitClosure())));
  first_run_loop.Run();

  ASSERT_EQ(fetched_results_.size(), 1ULL);

  // As in the RefreshSubsequentFetch test, we'll change the served
  // content to detect networked fetch requests made.
  loader_factory_.AddResponse(kKnownGoodResourceURL, "apologize darn you");

  // The "live" content in the serving root has changed; now, we perform
  // some local fetches without hitting the network. These Fetch()es
  // will return the stock content.
  base::RunLoop second_run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PrinterConfigCache::Fetch, base::Unretained(cache_.get()),
                     "known-good",
                     // Avoids hitting the network by using a long
                     // timeout. Bear in mind that this test controls
                     // the passage of time, so nonzero timeout is
                     // "long" here...
                     base::Seconds(1LL),
                     base::BindOnce(&PrinterConfigCacheTest::CaptureFetchResult,
                                    base::Unretained(this),
                                    // Avoids quitting this RunLoop.
                                    base::RepeatingClosure())));

  // Performs a local Fetch() a few more times for no particular reason.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &PrinterConfigCache::Fetch, base::Unretained(cache_.get()),
          "known-good", base::Seconds(3600LL),
          base::BindOnce(&PrinterConfigCacheTest::CaptureFetchResult,
                         base::Unretained(this), base::RepeatingClosure())));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &PrinterConfigCache::Fetch, base::Unretained(cache_.get()),
          "known-good", base::Seconds(86400LL),
          base::BindOnce(&PrinterConfigCacheTest::CaptureFetchResult,
                         base::Unretained(this), base::RepeatingClosure())));

  // Performs a live Fetch(), returning the live (mutated) contents.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PrinterConfigCache::Fetch, base::Unretained(cache_.get()),
                     "known-good",
                     // Forces the networked fetch.
                     base::Seconds(0LL),
                     base::BindOnce(&PrinterConfigCacheTest::CaptureFetchResult,
                                    base::Unretained(this),
                                    // Ends our RunLoop.
                                    second_run_loop.QuitClosure())));
  second_run_loop.Run();

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

// Tests that Fetch() respects its |expiration| argument. This is a
// purely time-bound variation on the LocallyPerformSubsequentFetch
// test; the served content doesn't change between RunLoops.
TEST_F(PrinterConfigCacheTest, FetchExpirationIsRespected) {
  // This Fetch() is given a useful |expiration|, but it won't matter
  // here since there are no locally resident cache entries at this
  // time; it'll have to be a networked fetch.
  base::RunLoop first_run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PrinterConfigCache::Fetch, base::Unretained(cache_.get()),
                     "known-good", base::Seconds(32LL),
                     base::BindOnce(&PrinterConfigCacheTest::CaptureFetchResult,
                                    base::Unretained(this),
                                    first_run_loop.QuitClosure())));
  first_run_loop.Run();
  ASSERT_EQ(fetched_results_.size(), 1ULL);
  const base::Time time_zero = clock_.Now();

  // Advance clock to T+31.
  AdvanceClock(base::Seconds(31LL));

  // This Fetch() is given the same useful |expiration|; it only matters
  // in that the clock does not yet indicate that the locally resident
  // cache entry has expired.
  base::RunLoop second_run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PrinterConfigCache::Fetch, base::Unretained(cache_.get()),
                     "known-good", base::Seconds(32LL),
                     base::BindOnce(&PrinterConfigCacheTest::CaptureFetchResult,
                                    base::Unretained(this),
                                    second_run_loop.QuitClosure())));
  second_run_loop.Run();
  ASSERT_EQ(fetched_results_.size(), 2ULL);
  // We don't capture the time right Now() because the above Fetch()
  // should have replied with local contents, fetched at time_zero.

  // Advance clock to T+32.
  AdvanceClock(base::Seconds(1));

  // This third Fetch() will be given the same |expiration| as ever.
  // The two previous calls to AdvanceClock() will have moved the time
  // beyond the staleness threshold, though, so this Fetch() will be
  // networked.
  base::RunLoop third_run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PrinterConfigCache::Fetch, base::Unretained(cache_.get()),
                     "known-good",
                     // Entry fetched at T+0 is now stale at T+32.
                     base::Seconds(32LL),
                     base::BindOnce(&PrinterConfigCacheTest::CaptureFetchResult,
                                    base::Unretained(this),
                                    third_run_loop.QuitClosure())));
  third_run_loop.Run();
  ASSERT_EQ(fetched_results_.size(), 3ULL);
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
  base::RunLoop first_run_loop;

  // Fetches the "known-good" resource with its stock contents.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PrinterConfigCache::Fetch, base::Unretained(cache_.get()),
                     "known-good", base::Seconds(604800LL),
                     base::BindOnce(&PrinterConfigCacheTest::CaptureFetchResult,
                                    base::Unretained(this),
                                    first_run_loop.QuitClosure())));
  first_run_loop.Run();

  // Drops that which we just fetched. This isn't immediately externally
  // visible, but its effects will soon be made apparent.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PrinterConfigCache::Drop,
                                base::Unretained(cache_.get()), "known-good"));

  // Mutates the contents served for the "known-good" resource.
  loader_factory_.AddResponse(kKnownGoodResourceURL, "ultimate dogeza");

  // Fetches the "known-good" resource anew with a wide timeout.
  // This is where the side effect of the prior Drop() call manifests:
  // the "known-good" resource is no longer cached, so not even a wide
  // timeout will spare us a networked fetch.
  base::RunLoop second_run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PrinterConfigCache::Fetch, base::Unretained(cache_.get()),
                     "known-good", base::Seconds(18748800LL),
                     base::BindOnce(&PrinterConfigCacheTest::CaptureFetchResult,
                                    base::Unretained(this),
                                    second_run_loop.QuitClosure())));
  second_run_loop.Run();

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

}  // namespace
}  // namespace chromeos
