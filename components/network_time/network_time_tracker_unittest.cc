// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_time/network_time_tracker.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "components/client_update_protocol/ecdsa.h"
#include "components/network_time/network_time_pref_names.h"
#include "components/network_time/network_time_test_utils.h"
#include "components/prefs/testing_pref_service.h"
#include "net/http/http_response_headers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network_time {

namespace {
const uint32_t kOneDayInSeconds = 86400;
const char kFetchFailedHistogram[] = "NetworkTimeTracker.UpdateTimeFetchFailed";
const char kFetchValidHistogram[] = "NetworkTimeTracker.UpdateTimeFetchValid";
const char kClockDivergencePositiveHistogram[] =
    "NetworkTimeTracker.ClockDivergence.Positive";
const char kClockDivergenceNegativeHistogram[] =
    "NetworkTimeTracker.ClockDivergence.Negative";
const char kWallClockBackwardsHistogram[] =
    "NetworkTimeTracker.WallClockRanBackwards";
const char kTimeBetweenFetchesHistogram[] =
    "NetworkTimeTracker.TimeBetweenFetches";
}  // namespace

class NetworkTimeTrackerTest : public ::testing::Test {
 public:
  ~NetworkTimeTrackerTest() override {}

  NetworkTimeTrackerTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::IO),
        field_trial_test_(new FieldTrialTest()),
        clock_(new base::SimpleTestClock),
        tick_clock_(new base::SimpleTestTickClock),
        test_server_(new net::EmbeddedTestServer) {
    NetworkTimeTracker::RegisterPrefs(pref_service_.registry());

    field_trial_test_->SetNetworkQueriesWithVariationsService(
        true, 0.0 /* query probability */,
        NetworkTimeTracker::FETCHES_IN_BACKGROUND_AND_ON_DEMAND);

    shared_url_loader_factory_ =
        base::MakeRefCounted<network::TestSharedURLLoaderFactory>();

    tracker_.reset(new NetworkTimeTracker(
        std::unique_ptr<base::Clock>(clock_),
        std::unique_ptr<const base::TickClock>(tick_clock_), &pref_service_,
        shared_url_loader_factory_));

    // Do this to be sure that |is_null| returns false.
    clock_->Advance(base::TimeDelta::FromDays(111));
    tick_clock_->Advance(base::TimeDelta::FromDays(222));

    // Can not be smaller than 15, it's the NowFromSystemTime() resolution.
    resolution_ = base::TimeDelta::FromMilliseconds(17);
    latency_ = base::TimeDelta::FromMilliseconds(50);
    adjustment_ = 7 * base::TimeDelta::FromMilliseconds(kTicksResolutionMs);
  }

  // Replaces |tracker_| with a new object, while preserving the
  // testing clocks.
  void Reset() {
    base::SimpleTestClock* new_clock = new base::SimpleTestClock();
    new_clock->SetNow(clock_->Now());
    base::SimpleTestTickClock* new_tick_clock = new base::SimpleTestTickClock();
    new_tick_clock->SetNowTicks(tick_clock_->NowTicks());
    clock_ = new_clock;
    tick_clock_ = new_tick_clock;
    tracker_.reset(new NetworkTimeTracker(
        std::unique_ptr<base::Clock>(clock_),
        std::unique_ptr<const base::TickClock>(tick_clock_), &pref_service_,
        shared_url_loader_factory_));
  }

  // Good signature over invalid data, though made with a non-production key.
  static std::unique_ptr<net::test_server::HttpResponse> BadDataResponseHandler(
      const net::test_server::HttpRequest& request) {
    net::test_server::BasicHttpResponse* response =
        new net::test_server::BasicHttpResponse();
    response->set_code(net::HTTP_OK);
    response->set_content(
        ")]}'\n"
        "{\"current_time_millis\":NaN,\"server_nonce\":9.420921002039447E182}");
    response->AddCustomHeader(
        "x-cup-server-proof",
        "3046022100a07aa437b24f1f6bb7ff6f6d1e004dd4bcb717c93e21d6bae5ef8d6d984c"
        "86a7022100e423419ff49fae37b421ef6cdeab348b45c63b236ab365f36f4cd3b4d4d6"
        "d852:"
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b85"
        "5");
    return std::unique_ptr<net::test_server::HttpResponse>(response);
  }

  static std::unique_ptr<net::test_server::HttpResponse>
  BadSignatureResponseHandler(const net::test_server::HttpRequest& request) {
    net::test_server::BasicHttpResponse* response =
        new net::test_server::BasicHttpResponse();
    response->set_code(net::HTTP_OK);
    response->set_content(
        ")]}'\n"
        "{\"current_time_millis\":1461621971825,\"server_nonce\":-6."
        "006853099049523E85}");
    response->AddCustomHeader("x-cup-server-proof", "dead:beef");
    return std::unique_ptr<net::test_server::HttpResponse>(response);
  }

  static std::unique_ptr<net::test_server::HttpResponse>
  ServerErrorResponseHandler(const net::test_server::HttpRequest& request) {
    net::test_server::BasicHttpResponse* response =
        new net::test_server::BasicHttpResponse();
    response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
    return std::unique_ptr<net::test_server::HttpResponse>(response);
  }

  static std::unique_ptr<net::test_server::HttpResponse>
  NetworkErrorResponseHandler(const net::test_server::HttpRequest& request) {
    return std::unique_ptr<net::test_server::HttpResponse>(
        new net::test_server::RawHttpResponse("", ""));
  }

  // Updates the notifier's time with the specified parameters.
  void UpdateNetworkTime(const base::Time& network_time,
                         const base::TimeDelta& resolution,
                         const base::TimeDelta& latency,
                         const base::TimeTicks& post_time) {
    tracker_->UpdateNetworkTime(
        network_time, resolution, latency, post_time);
  }

  // Advances both the system clock and the tick clock.  This should be used for
  // the normal passage of time, i.e. when neither clock is doing anything odd.
  void AdvanceBoth(const base::TimeDelta& delta) {
    tick_clock_->Advance(delta);
    clock_->Advance(delta);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<FieldTrialTest> field_trial_test_;
  base::TimeDelta resolution_;
  base::TimeDelta latency_;
  base::TimeDelta adjustment_;
  base::SimpleTestClock* clock_;
  base::SimpleTestTickClock* tick_clock_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<NetworkTimeTracker> tracker_;
  std::unique_ptr<net::EmbeddedTestServer> test_server_;
  scoped_refptr<network::TestSharedURLLoaderFactory> shared_url_loader_factory_;
};

TEST_F(NetworkTimeTrackerTest, Uninitialized) {
  base::Time network_time;
  base::TimeDelta uncertainty;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_NO_SYNC_ATTEMPT,
            tracker_->GetNetworkTime(&network_time, &uncertainty));
}

TEST_F(NetworkTimeTrackerTest, LongPostingDelay) {
  // The request arrives at the server, which records the time.  Advance the
  // clock to simulate the latency of sending the reply, which we'll say for
  // convenience is half the total latency.
  base::Time in_network_time = clock_->Now();
  AdvanceBoth(latency_ / 2);

  // Record the tick counter at the time the reply is received.  At this point,
  // we would post UpdateNetworkTime to be run on the browser thread.
  base::TimeTicks posting_time = tick_clock_->NowTicks();

  // Simulate that it look a long time (1888us) for the browser thread to get
  // around to executing the update.
  AdvanceBoth(base::TimeDelta::FromMicroseconds(1888));
  UpdateNetworkTime(in_network_time, resolution_, latency_, posting_time);

  base::Time out_network_time;
  base::TimeDelta uncertainty;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, &uncertainty));
  EXPECT_EQ(resolution_ + latency_ + adjustment_, uncertainty);
  EXPECT_EQ(clock_->Now(), out_network_time);
}

TEST_F(NetworkTimeTrackerTest, LopsidedLatency) {
  // Simulate that the server received the request instantaneously, and that all
  // of the latency was in sending the reply.  (This contradicts the assumption
  // in the code.)
  base::Time in_network_time = clock_->Now();
  AdvanceBoth(latency_);
  UpdateNetworkTime(in_network_time, resolution_, latency_,
                    tick_clock_->NowTicks());

  // But, the answer is still within the uncertainty bounds!
  base::Time out_network_time;
  base::TimeDelta uncertainty;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, &uncertainty));
  EXPECT_LT(out_network_time - uncertainty / 2, clock_->Now());
  EXPECT_GT(out_network_time + uncertainty / 2, clock_->Now());
}

TEST_F(NetworkTimeTrackerTest, ClockIsWack) {
  // Now let's assume the system clock is completely wrong.
  base::Time in_network_time = clock_->Now() - base::TimeDelta::FromDays(90);
  UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());

  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  EXPECT_EQ(in_network_time, out_network_time);
}

TEST_F(NetworkTimeTrackerTest, ClocksDivergeSlightly) {
  // The two clocks are allowed to diverge a little bit.
  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kClockDivergencePositiveHistogram, 0);
  histograms.ExpectTotalCount(kClockDivergenceNegativeHistogram, 0);
  histograms.ExpectTotalCount(kWallClockBackwardsHistogram, 0);
  base::Time in_network_time = clock_->Now();
  UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());

  base::TimeDelta small = base::TimeDelta::FromSeconds(30);
  tick_clock_->Advance(small);
  base::Time out_network_time;
  base::TimeDelta out_uncertainty;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, &out_uncertainty));
  EXPECT_EQ(in_network_time + small, out_network_time);
  // The clock divergence should show up in the uncertainty.
  EXPECT_EQ(resolution_ + latency_ + adjustment_ + small, out_uncertainty);
  histograms.ExpectTotalCount(kClockDivergencePositiveHistogram, 0);
  histograms.ExpectTotalCount(kClockDivergenceNegativeHistogram, 0);
  histograms.ExpectTotalCount(kWallClockBackwardsHistogram, 0);
}

TEST_F(NetworkTimeTrackerTest, NetworkTimeUpdates) {
  // Verify that the the tracker receives and properly handles updates to the
  // network time.
  base::Time out_network_time;
  base::TimeDelta uncertainty;

  UpdateNetworkTime(clock_->Now() - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, &uncertainty));
  EXPECT_EQ(clock_->Now(), out_network_time);
  EXPECT_EQ(resolution_ + latency_ + adjustment_, uncertainty);

  // Fake a wait to make sure we keep tracking.
  AdvanceBoth(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, &uncertainty));
  EXPECT_EQ(clock_->Now(), out_network_time);
  EXPECT_EQ(resolution_ + latency_ + adjustment_, uncertainty);

  // And one more time.
  UpdateNetworkTime(clock_->Now() - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());
  AdvanceBoth(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, &uncertainty));
  EXPECT_EQ(clock_->Now(), out_network_time);
  EXPECT_EQ(resolution_ + latency_ + adjustment_, uncertainty);
}

TEST_F(NetworkTimeTrackerTest, SpringForward) {
  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kClockDivergencePositiveHistogram, 0);
  histograms.ExpectTotalCount(kClockDivergenceNegativeHistogram, 0);
  histograms.ExpectTotalCount(kWallClockBackwardsHistogram, 0);
  // Simulate the wall clock advancing faster than the tick clock.
  UpdateNetworkTime(clock_->Now(), resolution_, latency_,
                    tick_clock_->NowTicks());
  tick_clock_->Advance(base::TimeDelta::FromSeconds(1));
  clock_->Advance(base::TimeDelta::FromDays(1));
  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_SYNC_LOST,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  histograms.ExpectTotalCount(kClockDivergencePositiveHistogram, 0);
  histograms.ExpectTotalCount(kClockDivergenceNegativeHistogram, 1);
  histograms.ExpectTotalCount(kWallClockBackwardsHistogram, 0);
  // The recorded clock divergence should be 1 second - 1 day in seconds.
  histograms.ExpectBucketCount(
      kClockDivergenceNegativeHistogram,
      base::TimeDelta::FromSeconds(kOneDayInSeconds - 1).InMilliseconds(), 1);
}

TEST_F(NetworkTimeTrackerTest, TickClockSpringsForward) {
  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kClockDivergencePositiveHistogram, 0);
  histograms.ExpectTotalCount(kClockDivergenceNegativeHistogram, 0);
  histograms.ExpectTotalCount(kWallClockBackwardsHistogram, 0);
  // Simulate the tick clock advancing faster than the wall clock.
  UpdateNetworkTime(clock_->Now(), resolution_, latency_,
                    tick_clock_->NowTicks());
  tick_clock_->Advance(base::TimeDelta::FromDays(1));
  clock_->Advance(base::TimeDelta::FromSeconds(1));
  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_SYNC_LOST,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  histograms.ExpectTotalCount(kClockDivergencePositiveHistogram, 1);
  histograms.ExpectTotalCount(kClockDivergenceNegativeHistogram, 0);
  histograms.ExpectTotalCount(kWallClockBackwardsHistogram, 0);
  // The recorded clock divergence should be 1 day - 1 second.
  histograms.ExpectBucketCount(
      kClockDivergencePositiveHistogram,
      base::TimeDelta::FromSeconds(kOneDayInSeconds - 1).InMilliseconds(), 1);
}

TEST_F(NetworkTimeTrackerTest, FallBack) {
  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kClockDivergencePositiveHistogram, 0);
  histograms.ExpectTotalCount(kClockDivergenceNegativeHistogram, 0);
  histograms.ExpectTotalCount(kWallClockBackwardsHistogram, 0);
  // Simulate the wall clock running backward.
  UpdateNetworkTime(clock_->Now(), resolution_, latency_,
                    tick_clock_->NowTicks());
  tick_clock_->Advance(base::TimeDelta::FromSeconds(1));
  clock_->Advance(base::TimeDelta::FromDays(-1));
  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_SYNC_LOST,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  histograms.ExpectTotalCount(kClockDivergencePositiveHistogram, 0);
  histograms.ExpectTotalCount(kClockDivergenceNegativeHistogram, 0);
  histograms.ExpectTotalCount(kWallClockBackwardsHistogram, 1);
  histograms.ExpectBucketCount(
      kWallClockBackwardsHistogram,
      base::TimeDelta::FromSeconds(kOneDayInSeconds - 1).InMilliseconds(), 1);
}

TEST_F(NetworkTimeTrackerTest, SuspendAndResume) {
  // Simulate the wall clock advancing while the tick clock stands still, as
  // would happen in a suspend+resume cycle.
  UpdateNetworkTime(clock_->Now(), resolution_, latency_,
                    tick_clock_->NowTicks());
  clock_->Advance(base::TimeDelta::FromHours(1));
  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_SYNC_LOST,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
}

TEST_F(NetworkTimeTrackerTest, Serialize) {
  // Test that we can serialize and deserialize state and get consistent
  // results.
  base::Time in_network_time = clock_->Now() - base::TimeDelta::FromDays(90);
  UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());
  base::Time out_network_time;
  base::TimeDelta out_uncertainty;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, &out_uncertainty));
  EXPECT_EQ(in_network_time, out_network_time);
  EXPECT_EQ(resolution_ + latency_ + adjustment_, out_uncertainty);

  // 6 days is just under the threshold for discarding data.
  base::TimeDelta delta = base::TimeDelta::FromDays(6);
  AdvanceBoth(delta);
  Reset();
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, &out_uncertainty));
  EXPECT_EQ(in_network_time + delta, out_network_time);
  EXPECT_EQ(resolution_ + latency_ + adjustment_, out_uncertainty);
}

TEST_F(NetworkTimeTrackerTest, DeserializeOldFormat) {
  // Test that deserializing old data (which do not record the uncertainty and
  // tick clock) causes the serialized data to be ignored.
  base::Time in_network_time = clock_->Now() - base::TimeDelta::FromDays(90);
  UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());

  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  double local, network;
  const base::DictionaryValue* saved_prefs =
      pref_service_.GetDictionary(prefs::kNetworkTimeMapping);
  saved_prefs->GetDouble("local", &local);
  saved_prefs->GetDouble("network", &network);
  base::DictionaryValue prefs;
  prefs.SetDouble("local", local);
  prefs.SetDouble("network", network);
  pref_service_.Set(prefs::kNetworkTimeMapping, prefs);
  Reset();
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_NO_SYNC_ATTEMPT,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
}

TEST_F(NetworkTimeTrackerTest, SerializeWithLongDelay) {
  // Test that if the serialized data are more than a week old, they are
  // discarded.
  base::Time in_network_time = clock_->Now() - base::TimeDelta::FromDays(90);
  UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());
  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  AdvanceBoth(base::TimeDelta::FromDays(8));
  Reset();
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_NO_SYNC_ATTEMPT,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
}

TEST_F(NetworkTimeTrackerTest, SerializeWithTickClockAdvance) {
  // Test that serialized data are discarded if the wall clock and tick clock
  // have not advanced consistently since data were serialized.
  base::Time in_network_time = clock_->Now() - base::TimeDelta::FromDays(90);
  UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());
  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  tick_clock_->Advance(base::TimeDelta::FromDays(1));
  Reset();
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_SYNC_LOST,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
}

TEST_F(NetworkTimeTrackerTest, SerializeWithWallClockAdvance) {
  // Test that serialized data are discarded if the wall clock and tick clock
  // have not advanced consistently since data were serialized.
  base::Time in_network_time = clock_->Now() - base::TimeDelta::FromDays(90);
  UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());

  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  clock_->Advance(base::TimeDelta::FromDays(1));
  Reset();
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_SYNC_LOST,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
}

TEST_F(NetworkTimeTrackerTest, UpdateFromNetwork) {
  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kFetchFailedHistogram, 0);
  histograms.ExpectTotalCount(kFetchValidHistogram, 0);

  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_NO_SYNC_ATTEMPT,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  // First query should happen soon.
  EXPECT_EQ(base::TimeDelta::FromMinutes(0),
            tracker_->GetTimerDelayForTesting());

  test_server_->RegisterRequestHandler(
      base::BindRepeating(&GoodTimeResponseHandler));
  EXPECT_TRUE(test_server_->Start());
  tracker_->SetTimeServerURLForTesting(test_server_->GetURL("/"));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);

  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  EXPECT_EQ(base::Time::UnixEpoch() +
                base::TimeDelta::FromMilliseconds(
                    (uint64_t)kGoodTimeResponseHandlerJsTime[0]),
            out_network_time);
  // Should see no backoff in the success case.
  EXPECT_EQ(base::TimeDelta::FromMinutes(60),
            tracker_->GetTimerDelayForTesting());

  histograms.ExpectTotalCount(kFetchFailedHistogram, 0);
  histograms.ExpectTotalCount(kFetchValidHistogram, 1);
  histograms.ExpectBucketCount(kFetchValidHistogram, true, 1);
}

TEST_F(NetworkTimeTrackerTest, StartTimeFetch) {
  test_server_->RegisterRequestHandler(
      base::BindRepeating(&GoodTimeResponseHandler));
  EXPECT_TRUE(test_server_->Start());
  tracker_->SetTimeServerURLForTesting(test_server_->GetURL("/"));

  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_NO_SYNC_ATTEMPT,
            tracker_->GetNetworkTime(&out_network_time, nullptr));

  base::RunLoop run_loop;
  EXPECT_TRUE(tracker_->StartTimeFetch(run_loop.QuitClosure()));
  tracker_->WaitForFetchForTesting(123123123);
  run_loop.Run();

  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  EXPECT_EQ(base::Time::UnixEpoch() +
                base::TimeDelta::FromMilliseconds(
                    (uint64_t)kGoodTimeResponseHandlerJsTime[0]),
            out_network_time);
  // Should see no backoff in the success case.
  EXPECT_EQ(base::TimeDelta::FromMinutes(60),
            tracker_->GetTimerDelayForTesting());
}

// Tests that when StartTimeFetch() is called with a query already in
// progress, it calls the callback when that query completes.
TEST_F(NetworkTimeTrackerTest, StartTimeFetchWithQueryInProgress) {
  test_server_->RegisterRequestHandler(
      base::BindRepeating(&GoodTimeResponseHandler));
  EXPECT_TRUE(test_server_->Start());
  tracker_->SetTimeServerURLForTesting(test_server_->GetURL("/"));

  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_NO_SYNC_ATTEMPT,
            tracker_->GetNetworkTime(&out_network_time, nullptr));

  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());

  base::RunLoop run_loop;
  EXPECT_TRUE(tracker_->StartTimeFetch(run_loop.QuitClosure()));
  tracker_->WaitForFetchForTesting(123123123);
  run_loop.Run();

  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  EXPECT_EQ(base::Time::UnixEpoch() +
                base::TimeDelta::FromMilliseconds(
                    (uint64_t)kGoodTimeResponseHandlerJsTime[0]),
            out_network_time);
  // Should see no backoff in the success case.
  EXPECT_EQ(base::TimeDelta::FromMinutes(60),
            tracker_->GetTimerDelayForTesting());
}

// Tests that StartTimeFetch() returns false if called while network
// time is available.
TEST_F(NetworkTimeTrackerTest, StartTimeFetchWhileSynced) {
  test_server_->RegisterRequestHandler(
      base::BindRepeating(&GoodTimeResponseHandler));
  EXPECT_TRUE(test_server_->Start());
  tracker_->SetTimeServerURLForTesting(test_server_->GetURL("/"));

  base::Time in_network_time = clock_->Now();
  UpdateNetworkTime(in_network_time, resolution_, latency_,
                    tick_clock_->NowTicks());

  // No query should be started so long as NetworkTimeTracker is synced.
  base::RunLoop run_loop;
  EXPECT_FALSE(tracker_->StartTimeFetch(run_loop.QuitClosure()));
}

// Tests that StartTimeFetch() returns false if the field trial
// is not configured to allow on-demand time fetches.
TEST_F(NetworkTimeTrackerTest, StartTimeFetchWithoutVariationsParam) {
  field_trial_test_->SetNetworkQueriesWithVariationsService(
      true, 0.0, NetworkTimeTracker::FETCHES_IN_BACKGROUND_ONLY);
  test_server_->RegisterRequestHandler(
      base::BindRepeating(&GoodTimeResponseHandler));
  EXPECT_TRUE(test_server_->Start());
  tracker_->SetTimeServerURLForTesting(test_server_->GetURL("/"));

  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_NO_SYNC_ATTEMPT,
            tracker_->GetNetworkTime(&out_network_time, nullptr));

  base::RunLoop run_loop;
  EXPECT_FALSE(tracker_->StartTimeFetch(run_loop.QuitClosure()));
}

TEST_F(NetworkTimeTrackerTest, NoNetworkQueryWhileSynced) {
  test_server_->RegisterRequestHandler(
      base::BindRepeating(&GoodTimeResponseHandler));
  EXPECT_TRUE(test_server_->Start());
  tracker_->SetTimeServerURLForTesting(test_server_->GetURL("/"));

  field_trial_test_->SetNetworkQueriesWithVariationsService(
      true, 0.0, NetworkTimeTracker::FETCHES_IN_BACKGROUND_AND_ON_DEMAND);
  base::Time in_network_time = clock_->Now();
  UpdateNetworkTime(in_network_time, resolution_, latency_,
                    tick_clock_->NowTicks());

  // No query should be started so long as NetworkTimeTracker is synced, but the
  // next check should happen soon.
  EXPECT_FALSE(tracker_->QueryTimeServiceForTesting());
  EXPECT_EQ(base::TimeDelta::FromMinutes(6),
            tracker_->GetTimerDelayForTesting());

  field_trial_test_->SetNetworkQueriesWithVariationsService(
      true, 1.0, NetworkTimeTracker::FETCHES_IN_BACKGROUND_AND_ON_DEMAND);
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);
  EXPECT_EQ(base::TimeDelta::FromMinutes(60),
            tracker_->GetTimerDelayForTesting());
}

TEST_F(NetworkTimeTrackerTest, NoNetworkQueryWhileFeatureDisabled) {
  // Disable network time queries and check that a query is not sent.
  field_trial_test_->SetNetworkQueriesWithVariationsService(
      false, 0.0, NetworkTimeTracker::FETCHES_IN_BACKGROUND_AND_ON_DEMAND);
  EXPECT_FALSE(tracker_->QueryTimeServiceForTesting());
  // The timer is not started when the feature is disabled.
  EXPECT_EQ(base::TimeDelta::FromMinutes(0),
            tracker_->GetTimerDelayForTesting());

  // Enable time queries and check that a query is sent.
  field_trial_test_->SetNetworkQueriesWithVariationsService(
      true, 0.0, NetworkTimeTracker::FETCHES_IN_BACKGROUND_AND_ON_DEMAND);
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);
}

TEST_F(NetworkTimeTrackerTest, UpdateFromNetworkBadSignature) {
  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kFetchFailedHistogram, 0);
  histograms.ExpectTotalCount(kFetchValidHistogram, 0);

  test_server_->RegisterRequestHandler(base::BindRepeating(
      &NetworkTimeTrackerTest::BadSignatureResponseHandler));
  EXPECT_TRUE(test_server_->Start());
  tracker_->SetTimeServerURLForTesting(test_server_->GetURL("/"));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);

  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_NO_SUCCESSFUL_SYNC,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  EXPECT_EQ(base::TimeDelta::FromMinutes(120),
            tracker_->GetTimerDelayForTesting());

  histograms.ExpectTotalCount(kFetchFailedHistogram, 0);
  histograms.ExpectTotalCount(kFetchValidHistogram, 1);
  histograms.ExpectBucketCount(kFetchValidHistogram, false, 1);
}

static const uint8_t kDevKeyPubBytes[] = {
    0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02,
    0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03,
    0x42, 0x00, 0x04, 0xe0, 0x6b, 0x0d, 0x76, 0x75, 0xa3, 0x99, 0x7d, 0x7c,
    0x1b, 0xd6, 0x3c, 0x73, 0xbb, 0x4b, 0xfe, 0x0a, 0xe7, 0x2f, 0x61, 0x3d,
    0x77, 0x0a, 0xaa, 0x14, 0xd8, 0x5a, 0xbf, 0x14, 0x60, 0xec, 0xf6, 0x32,
    0x77, 0xb5, 0xa7, 0xe6, 0x35, 0xa5, 0x61, 0xaf, 0xdc, 0xdf, 0x91, 0xce,
    0x45, 0x34, 0x5f, 0x36, 0x85, 0x2f, 0xb9, 0x53, 0x00, 0x5d, 0x86, 0xe7,
    0x04, 0x16, 0xe2, 0x3d, 0x21, 0x76, 0x2b};

TEST_F(NetworkTimeTrackerTest, UpdateFromNetworkBadData) {
  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kFetchFailedHistogram, 0);
  histograms.ExpectTotalCount(kFetchValidHistogram, 0);

  test_server_->RegisterRequestHandler(
      base::BindRepeating(&NetworkTimeTrackerTest::BadDataResponseHandler));
  EXPECT_TRUE(test_server_->Start());
  base::StringPiece key = {reinterpret_cast<const char*>(kDevKeyPubBytes),
                           sizeof(kDevKeyPubBytes)};
  tracker_->SetPublicKeyForTesting(key);
  tracker_->SetTimeServerURLForTesting(test_server_->GetURL("/"));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);
  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_NO_SUCCESSFUL_SYNC,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  EXPECT_EQ(base::TimeDelta::FromMinutes(120),
            tracker_->GetTimerDelayForTesting());

  histograms.ExpectTotalCount(kFetchFailedHistogram, 0);
  histograms.ExpectTotalCount(kFetchValidHistogram, 1);
  histograms.ExpectBucketCount(kFetchValidHistogram, false, 1);
}

TEST_F(NetworkTimeTrackerTest, UpdateFromNetworkServerError) {
  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kFetchFailedHistogram, 0);
  histograms.ExpectTotalCount(kFetchValidHistogram, 0);

  test_server_->RegisterRequestHandler(
      base::BindRepeating(&NetworkTimeTrackerTest::ServerErrorResponseHandler));
  EXPECT_TRUE(test_server_->Start());
  tracker_->SetTimeServerURLForTesting(test_server_->GetURL("/"));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);

  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_NO_SUCCESSFUL_SYNC,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  // Should see backoff in the error case.
  EXPECT_EQ(base::TimeDelta::FromMinutes(120),
            tracker_->GetTimerDelayForTesting());

  histograms.ExpectTotalCount(kFetchFailedHistogram, 1);
  // There was no network error, so the histogram is recorded as
  // net::OK, indicating that the connection succeeded but there was a
  // non-200 HTTP status code.
  histograms.ExpectBucketCount(kFetchFailedHistogram, net::OK, 1);
  histograms.ExpectTotalCount(kFetchValidHistogram, 0);
}

#if defined(OS_IOS)
// http://crbug.com/658619
#define MAYBE_UpdateFromNetworkNetworkError     \
    DISABLED_UpdateFromNetworkNetworkError
#else
#define MAYBE_UpdateFromNetworkNetworkError UpdateFromNetworkNetworkError
#endif
TEST_F(NetworkTimeTrackerTest, MAYBE_UpdateFromNetworkNetworkError) {
  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kFetchFailedHistogram, 0);
  histograms.ExpectTotalCount(kFetchValidHistogram, 0);

  test_server_->RegisterRequestHandler(base::BindRepeating(
      &NetworkTimeTrackerTest::NetworkErrorResponseHandler));
  EXPECT_TRUE(test_server_->Start());
  tracker_->SetTimeServerURLForTesting(test_server_->GetURL("/"));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);

  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_NO_SUCCESSFUL_SYNC,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  // Should see backoff in the error case.
  EXPECT_EQ(base::TimeDelta::FromMinutes(120),
            tracker_->GetTimerDelayForTesting());

  histograms.ExpectTotalCount(kFetchFailedHistogram, 1);
  histograms.ExpectBucketCount(kFetchFailedHistogram, -net::ERR_EMPTY_RESPONSE,
                               1);
  histograms.ExpectTotalCount(kFetchValidHistogram, 0);
}

TEST_F(NetworkTimeTrackerTest, UpdateFromNetworkLargeResponse) {
  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kFetchFailedHistogram, 0);
  histograms.ExpectTotalCount(kFetchValidHistogram, 0);

  test_server_->RegisterRequestHandler(
      base::BindRepeating(&GoodTimeResponseHandler));
  EXPECT_TRUE(test_server_->Start());
  tracker_->SetTimeServerURLForTesting(test_server_->GetURL("/"));

  base::Time out_network_time;

  tracker_->SetMaxResponseSizeForTesting(3);
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_NO_SUCCESSFUL_SYNC,
            tracker_->GetNetworkTime(&out_network_time, nullptr));

  histograms.ExpectTotalCount(kFetchFailedHistogram, 1);
  histograms.ExpectTotalCount(kFetchValidHistogram, 0);

  tracker_->SetMaxResponseSizeForTesting(1024);
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, nullptr));

  histograms.ExpectTotalCount(kFetchFailedHistogram, 1);
  histograms.ExpectTotalCount(kFetchValidHistogram, 1);
  histograms.ExpectBucketCount(kFetchValidHistogram, true, 1);
}

TEST_F(NetworkTimeTrackerTest, UpdateFromNetworkFirstSyncPending) {
  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kFetchFailedHistogram, 0);
  histograms.ExpectTotalCount(kFetchValidHistogram, 0);

  test_server_->RegisterRequestHandler(
      base::BindRepeating(&NetworkTimeTrackerTest::BadDataResponseHandler));
  EXPECT_TRUE(test_server_->Start());
  base::StringPiece key = {reinterpret_cast<const char*>(kDevKeyPubBytes),
                           sizeof(kDevKeyPubBytes)};
  tracker_->SetPublicKeyForTesting(key);
  tracker_->SetTimeServerURLForTesting(test_server_->GetURL("/"));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());

  // Do not wait for the fetch to complete; ask for the network time
  // immediately while the request is still pending.
  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_FIRST_SYNC_PENDING,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  histograms.ExpectTotalCount(kFetchFailedHistogram, 0);
  histograms.ExpectTotalCount(kFetchValidHistogram, 0);

  tracker_->WaitForFetchForTesting(123123123);
}

TEST_F(NetworkTimeTrackerTest, UpdateFromNetworkSubseqeuntSyncPending) {
  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kFetchFailedHistogram, 0);
  histograms.ExpectTotalCount(kFetchValidHistogram, 0);

  test_server_->RegisterRequestHandler(
      base::BindRepeating(&NetworkTimeTrackerTest::BadDataResponseHandler));
  EXPECT_TRUE(test_server_->Start());
  base::StringPiece key = {reinterpret_cast<const char*>(kDevKeyPubBytes),
                           sizeof(kDevKeyPubBytes)};
  tracker_->SetPublicKeyForTesting(key);
  tracker_->SetTimeServerURLForTesting(test_server_->GetURL("/"));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);

  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_NO_SUCCESSFUL_SYNC,
            tracker_->GetNetworkTime(&out_network_time, nullptr));

  // After one sync attempt failed, kick off another one, and ask for
  // the network time while it is still pending.
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_SUBSEQUENT_SYNC_PENDING,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  histograms.ExpectTotalCount(kFetchFailedHistogram, 0);
  histograms.ExpectTotalCount(kFetchValidHistogram, 1);
  histograms.ExpectBucketCount(kFetchValidHistogram, false, 1);

  tracker_->WaitForFetchForTesting(123123123);
}

namespace {

// NetworkTimeTrackerTest.TimeBetweenFetchesHistogram needs to make several time
// queries that return different times. MultipleGoodTimeResponseHandler is like
// GoodTimeResponseHandler, but returning different times on each of three
// requests that happen in sequence.
//
// See comments inline for how to update the times that are returned.
class MultipleGoodTimeResponseHandler {
 public:
  MultipleGoodTimeResponseHandler() {}
  ~MultipleGoodTimeResponseHandler() {}

  std::unique_ptr<net::test_server::HttpResponse> ResponseHandler(
      const net::test_server::HttpRequest& request);

  // Returns the time that is returned in the (i-1)'th response handled by
  // ResponseHandler(), or null base::Time() if too many responses have been
  // handled.
  base::Time GetTimeAtIndex(unsigned int i);

 private:
  // The index into |kGoodTimeResponseHandlerJsTime|, |kGoodTimeResponseBody|,
  // and |kGoodTimeResponseServerProofHeaders| that will be used in the
  // response in the next ResponseHandler() call.
  unsigned int next_time_index_ = 0;

  DISALLOW_COPY_AND_ASSIGN(MultipleGoodTimeResponseHandler);
};

std::unique_ptr<net::test_server::HttpResponse>
MultipleGoodTimeResponseHandler::ResponseHandler(
    const net::test_server::HttpRequest& request) {
  net::test_server::BasicHttpResponse* response =
      new net::test_server::BasicHttpResponse();

  if (next_time_index_ >= base::size(kGoodTimeResponseBody)) {
    response->set_code(net::HTTP_BAD_REQUEST);
    return std::unique_ptr<net::test_server::HttpResponse>(response);
  }

  response->set_code(net::HTTP_OK);
  response->set_content(kGoodTimeResponseBody[next_time_index_]);
  response->AddCustomHeader(
      "x-cup-server-proof",
      kGoodTimeResponseServerProofHeader[next_time_index_]);
  next_time_index_++;
  return std::unique_ptr<net::test_server::HttpResponse>(response);
}

base::Time MultipleGoodTimeResponseHandler::GetTimeAtIndex(unsigned int i) {
  if (i >= base::size(kGoodTimeResponseHandlerJsTime))
    return base::Time();
  return base::Time::FromJsTime(kGoodTimeResponseHandlerJsTime[i]);
}

}  // namespace

TEST_F(NetworkTimeTrackerTest, TimeBetweenFetchesHistogram) {
  MultipleGoodTimeResponseHandler response_handler;
  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kTimeBetweenFetchesHistogram, 0);

  test_server_->RegisterRequestHandler(
      base::BindRepeating(&MultipleGoodTimeResponseHandler::ResponseHandler,
                          base::Unretained(&response_handler)));
  EXPECT_TRUE(test_server_->Start());
  tracker_->SetTimeServerURLForTesting(test_server_->GetURL("/"));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);

  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  // After the first query, there should be no histogram value because
  // there was no delta to record.
  histograms.ExpectTotalCount(kTimeBetweenFetchesHistogram, 0);

  // Trigger a second query, which should cause the delta from the first
  // query to be recorded.
  clock_->Advance(base::TimeDelta::FromHours(1));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  histograms.ExpectTotalCount(kTimeBetweenFetchesHistogram, 1);
  histograms.ExpectBucketCount(
      kTimeBetweenFetchesHistogram,
      (response_handler.GetTimeAtIndex(1) - response_handler.GetTimeAtIndex(0))
          .InMilliseconds(),
      1);
}

}  // namespace network_time
