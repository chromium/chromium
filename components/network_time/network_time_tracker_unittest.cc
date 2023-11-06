// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_time/network_time_tracker.h"

#include <memory>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/client_update_protocol/ecdsa.h"
#include "components/network_time/network_time_pref_names.h"
#include "components/network_time/network_time_test_utils.h"
#include "components/prefs/testing_pref_service.h"
#include "net/http/http_response_headers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network_time {

namespace {

// Latencies simulated by the fake network responses for the network times. This
// array should have the same length as `kGoodTimeResponseBody`.
const base::TimeDelta kGoodTimeResponseLatency[] = {
    base::Milliseconds(500), base::Milliseconds(520), base::Milliseconds(450),
    base::Milliseconds(550), base::Milliseconds(480),
};

struct MockedResponse {
  network::mojom::URLResponseHeadPtr head;
  std::string body;
  network::URLLoaderCompletionStatus status;
};

}  // namespace

class NetworkTimeTrackerTest : public ::testing::Test {
 public:
  ~NetworkTimeTrackerTest() override {}

  NetworkTimeTrackerTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::IO),
        field_trial_test_(new FieldTrialTest()),
        clock_(new base::SimpleTestClock),
        tick_clock_(new base::SimpleTestTickClock) {
    NetworkTimeTracker::RegisterPrefs(pref_service_.registry());

    field_trial_test_->SetFeatureParams(
        true, 0.0 /* query probability */,
        NetworkTimeTracker::FETCHES_IN_BACKGROUND_AND_ON_DEMAND,
        NetworkTimeTracker::ClockDriftSamples::NO_SAMPLES);

    url_loader_factory_.SetInterceptor(base::BindRepeating(
        &NetworkTimeTrackerTest::Intercept, weak_ptr_factory_.GetWeakPtr()));

    tracker_ = std::make_unique<NetworkTimeTracker>(
        std::unique_ptr<base::Clock>(clock_),
        std::unique_ptr<const base::TickClock>(tick_clock_), &pref_service_,
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_));

    // Do this to be sure that |is_null| returns false.
    clock_->Advance(base::Days(111));
    tick_clock_->Advance(base::Days(222));

    // Can not be smaller than 15, it's the NowFromSystemTime() resolution.
    resolution_ = base::Milliseconds(17);
    latency_ = base::Milliseconds(50);
    adjustment_ = 7 * base::Milliseconds(kTicksResolutionMs);
  }

  // Sets `response_handler` as handler for all requests made through
  // `url_loader_factory_`.
  void SetResponseHandler(
      base::RepeatingCallback<MockedResponse()> response_handler) {
    response_handler_ = std::move(response_handler);
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
    tracker_ = std::make_unique<NetworkTimeTracker>(
        std::unique_ptr<base::Clock>(clock_),
        std::unique_ptr<const base::TickClock>(tick_clock_), &pref_service_,
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_));
  }

  // Good signature over invalid data, though made with a non-production key.
  static MockedResponse BadDataResponseHandler() {
    network::mojom::URLResponseHeadPtr head =
        network::CreateURLResponseHead(net::HTTP_OK);
    std::string body =
        ")]}'\n"
        "{\"current_time_millis\":NaN,\"server_nonce\":9.420921002039447E182}";
    head->headers->AddHeader(
        "x-cup-server-proof",
        "3046022100a07aa437b24f1f6bb7ff6f6d1e004dd4bcb717c93e21d6bae5ef8d6d984c"
        "86a7022100e423419ff49fae37b421ef6cdeab348b45c63b236ab365f36f4cd3b4d4d6"
        "d852:"
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b85"
        "5");
    return MockedResponse{std::move(head), std::move(body)};
  }

  static MockedResponse GoodTimeResponseHandler() {
    network::mojom::URLResponseHeadPtr head =
        network::CreateURLResponseHead(net::HTTP_OK);
    head->headers->AddHeader("x-cup-server-proof",
                             kGoodTimeResponseServerProofHeader[0]);
    return MockedResponse{std::move(head), kGoodTimeResponseBody[0]};
  }

  static MockedResponse BadSignatureResponseHandler() {
    network::mojom::URLResponseHeadPtr head =
        network::CreateURLResponseHead(net::HTTP_OK);
    std::string body =
        ")]}'\n"
        "{\"current_time_millis\":1461621971825,\"server_nonce\":-6."
        "006853099049523E85}";
    head->headers->AddHeader("x-cup-server-proof", "dead:beef");
    return MockedResponse{std::move(head), std::move(body)};
  }

  static MockedResponse ServerErrorResponseHandler() {
    network::mojom::URLResponseHeadPtr head =
        network::CreateURLResponseHead(net::HTTP_INTERNAL_SERVER_ERROR);
    return MockedResponse{std::move(head), ""};
  }

  static MockedResponse NetworkErrorResponseHandler() {
    network::mojom::URLResponseHeadPtr head =
        network::mojom::URLResponseHead::New();
    return MockedResponse{
        std::move(head), "",
        network::URLLoaderCompletionStatus(net::ERR_EMPTY_RESPONSE)};
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
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<NetworkTimeTracker> tracker_;
  raw_ptr<base::SimpleTestClock> clock_;
  raw_ptr<base::SimpleTestTickClock> tick_clock_;
  network::TestURLLoaderFactory url_loader_factory_;
  base::RepeatingCallback<MockedResponse()> response_handler_;

 private:
  void Intercept(const network::ResourceRequest& request) {
    CHECK(response_handler_);
    MockedResponse response = response_handler_.Run();
    // status.decoded_body_length = response.body.size();
    url_loader_factory_.AddResponse(request.url, std::move(response.head),
                                    std::move(response.body),
                                    std::move(response.status));
  }

  base::WeakPtrFactory<NetworkTimeTrackerTest> weak_ptr_factory_{this};
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
  AdvanceBoth(base::Microseconds(1888));
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
  base::Time in_network_time = clock_->Now() - base::Days(90);
  UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());

  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  EXPECT_EQ(in_network_time, out_network_time);
}

TEST_F(NetworkTimeTrackerTest, ClocksDivergeSlightly) {
  // The two clocks are allowed to diverge a little bit.
  base::Time in_network_time = clock_->Now();
  UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());

  base::TimeDelta small = base::Seconds(30);
  tick_clock_->Advance(small);
  base::Time out_network_time;
  base::TimeDelta out_uncertainty;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, &out_uncertainty));
  EXPECT_EQ(in_network_time + small, out_network_time);
  // The clock divergence should show up in the uncertainty.
  EXPECT_EQ(resolution_ + latency_ + adjustment_ + small, out_uncertainty);
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
  AdvanceBoth(base::Seconds(1));
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, &uncertainty));
  EXPECT_EQ(clock_->Now(), out_network_time);
  EXPECT_EQ(resolution_ + latency_ + adjustment_, uncertainty);

  // And one more time.
  UpdateNetworkTime(clock_->Now() - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());
  AdvanceBoth(base::Seconds(1));
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, &uncertainty));
  EXPECT_EQ(clock_->Now(), out_network_time);
  EXPECT_EQ(resolution_ + latency_ + adjustment_, uncertainty);
}

TEST_F(NetworkTimeTrackerTest, SpringForward) {
  // Simulate the wall clock advancing faster than the tick clock.
  UpdateNetworkTime(clock_->Now(), resolution_, latency_,
                    tick_clock_->NowTicks());
  tick_clock_->Advance(base::Seconds(1));
  clock_->Advance(base::Days(1));
  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_SYNC_LOST,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
}

TEST_F(NetworkTimeTrackerTest, TickClockSpringsForward) {
  // Simulate the tick clock advancing faster than the wall clock.
  UpdateNetworkTime(clock_->Now(), resolution_, latency_,
                    tick_clock_->NowTicks());
  tick_clock_->Advance(base::Days(1));
  clock_->Advance(base::Seconds(1));
  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_SYNC_LOST,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
}

TEST_F(NetworkTimeTrackerTest, FallBack) {
  // Simulate the wall clock running backward.
  UpdateNetworkTime(clock_->Now(), resolution_, latency_,
                    tick_clock_->NowTicks());
  tick_clock_->Advance(base::Seconds(1));
  clock_->Advance(base::Days(-1));
  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_SYNC_LOST,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
}

TEST_F(NetworkTimeTrackerTest, SuspendAndResume) {
  // Simulate the wall clock advancing while the tick clock stands still, as
  // would happen in a suspend+resume cycle.
  UpdateNetworkTime(clock_->Now(), resolution_, latency_,
                    tick_clock_->NowTicks());
  clock_->Advance(base::Hours(1));
  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_SYNC_LOST,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
}

TEST_F(NetworkTimeTrackerTest, Serialize) {
  // Test that we can serialize and deserialize state and get consistent
  // results.
  base::Time in_network_time = clock_->Now() - base::Days(90);
  UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());
  base::Time out_network_time;
  base::TimeDelta out_uncertainty;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, &out_uncertainty));
  EXPECT_EQ(in_network_time, out_network_time);
  EXPECT_EQ(resolution_ + latency_ + adjustment_, out_uncertainty);

  // 6 days is just under the threshold for discarding data.
  base::TimeDelta delta = base::Days(6);
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
  base::Time in_network_time = clock_->Now() - base::Days(90);
  UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());

  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  absl::optional<double> local, network;
  const base::Value::Dict& saved_prefs =
      pref_service_.GetDict(prefs::kNetworkTimeMapping);
  local = saved_prefs.FindDouble("local");
  network = saved_prefs.FindDouble("network");
  ASSERT_TRUE(local);
  ASSERT_TRUE(network);
  base::Value::Dict prefs;
  prefs.Set("local", *local);
  prefs.Set("network", *network);
  pref_service_.Set(prefs::kNetworkTimeMapping, base::Value(std::move(prefs)));
  Reset();
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_NO_SYNC_ATTEMPT,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
}

TEST_F(NetworkTimeTrackerTest, SerializeWithLongDelay) {
  // Test that if the serialized data are more than a week old, they are
  // discarded.
  base::Time in_network_time = clock_->Now() - base::Days(90);
  UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());
  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  AdvanceBoth(base::Days(8));
  Reset();
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_NO_SYNC_ATTEMPT,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
}

TEST_F(NetworkTimeTrackerTest, SerializeWithTickClockAdvance) {
  // Test that serialized data are discarded if the wall clock and tick clock
  // have not advanced consistently since data were serialized.
  base::Time in_network_time = clock_->Now() - base::Days(90);
  UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());
  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  tick_clock_->Advance(base::Days(1));
  Reset();
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_SYNC_LOST,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
}

TEST_F(NetworkTimeTrackerTest, SerializeWithWallClockAdvance) {
  // Test that serialized data are discarded if the wall clock and tick clock
  // have not advanced consistently since data were serialized.
  base::Time in_network_time = clock_->Now() - base::Days(90);
  UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());

  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  clock_->Advance(base::Days(1));
  Reset();
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_SYNC_LOST,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
}

TEST_F(NetworkTimeTrackerTest, UpdateFromNetwork) {
  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_NO_SYNC_ATTEMPT,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  // First query should happen soon.
  EXPECT_EQ(base::Minutes(0), tracker_->GetTimerDelayForTesting());

  SetResponseHandler(base::BindRepeating(&GoodTimeResponseHandler));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);

  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, nullptr));

  // Enabling load timing for the resource requests seems to increase accuracy
  // beyond milliseconds. Accuracy of GoodTimeResponseHandler is
  // milliseconds, any difference below 1 ms can therefore be ignored.
  EXPECT_LT(base::Time::FromMillisecondsSinceUnixEpoch(
                kGoodTimeResponseHandlerJsTime[0]) -
                out_network_time,
            base::Milliseconds(1));
  // Should see no backoff in the success case.
  EXPECT_EQ(base::Minutes(60), tracker_->GetTimerDelayForTesting());
}

TEST_F(NetworkTimeTrackerTest, StartTimeFetch) {
  SetResponseHandler(base::BindRepeating(&GoodTimeResponseHandler));

  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_NO_SYNC_ATTEMPT,
            tracker_->GetNetworkTime(&out_network_time, nullptr));

  base::RunLoop run_loop;
  EXPECT_TRUE(tracker_->StartTimeFetch(run_loop.QuitClosure()));
  tracker_->WaitForFetchForTesting(123123123);
  run_loop.Run();

  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  // Enabling load timing for the resource requests seems to increase accuracy
  // beyond milliseconds. Accuracy of GoodTimeResponseHandler is milliseconds,
  // any difference below 1 ms can therefore be ignored.
  EXPECT_LT(base::Time::FromMillisecondsSinceUnixEpoch(
                kGoodTimeResponseHandlerJsTime[0]) -
                out_network_time,
            base::Milliseconds(1));
  // Should see no backoff in the success case.
  EXPECT_EQ(base::Minutes(60), tracker_->GetTimerDelayForTesting());
}

// Tests that when StartTimeFetch() is called with a query already in
// progress, it calls the callback when that query completes.
TEST_F(NetworkTimeTrackerTest, StartTimeFetchWithQueryInProgress) {
  SetResponseHandler(base::BindRepeating(&GoodTimeResponseHandler));

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
  // Enabling load timing for the resource requests seems to increase accuracy
  // beyond milliseconds. Accuracy of GoodTimeResponseHandler is milliseconds,
  // any difference below 1 ms can therefore be ignored.
  EXPECT_LT(base::Time::FromMillisecondsSinceUnixEpoch(
                kGoodTimeResponseHandlerJsTime[0]) -
                out_network_time,
            base::Milliseconds(1));
  // Should see no backoff in the success case.
  EXPECT_EQ(base::Minutes(60), tracker_->GetTimerDelayForTesting());
}

// Tests that StartTimeFetch() returns false if called while network
// time is available.
TEST_F(NetworkTimeTrackerTest, StartTimeFetchWhileSynced) {
  SetResponseHandler(base::BindRepeating(&GoodTimeResponseHandler));

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
  field_trial_test_->SetFeatureParams(
      true, 0.0, NetworkTimeTracker::FETCHES_IN_BACKGROUND_ONLY,
      NetworkTimeTracker::ClockDriftSamples::NO_SAMPLES);
  SetResponseHandler(base::BindRepeating(&GoodTimeResponseHandler));

  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_NO_SYNC_ATTEMPT,
            tracker_->GetNetworkTime(&out_network_time, nullptr));

  base::RunLoop run_loop;
  EXPECT_FALSE(tracker_->StartTimeFetch(run_loop.QuitClosure()));
}

TEST_F(NetworkTimeTrackerTest, NoNetworkQueryWhileSynced) {
  SetResponseHandler(base::BindRepeating(&GoodTimeResponseHandler));

  field_trial_test_->SetFeatureParams(
      true, 0.0, NetworkTimeTracker::FETCHES_IN_BACKGROUND_AND_ON_DEMAND,
      NetworkTimeTracker::ClockDriftSamples::NO_SAMPLES);
  base::Time in_network_time = clock_->Now();
  UpdateNetworkTime(in_network_time, resolution_, latency_,
                    tick_clock_->NowTicks());

  // No query should be started so long as NetworkTimeTracker is synced, but the
  // next check should happen soon.
  EXPECT_FALSE(tracker_->QueryTimeServiceForTesting());
  EXPECT_EQ(base::Minutes(6), tracker_->GetTimerDelayForTesting());

  field_trial_test_->SetFeatureParams(
      true, 1.0, NetworkTimeTracker::FETCHES_IN_BACKGROUND_AND_ON_DEMAND,
      NetworkTimeTracker::ClockDriftSamples::NO_SAMPLES);
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);
  EXPECT_EQ(base::Minutes(60), tracker_->GetTimerDelayForTesting());
}

TEST_F(NetworkTimeTrackerTest, NoNetworkQueryWhileFeatureDisabled) {
  // Disable network time queries and check that a query is not sent.
  field_trial_test_->SetFeatureParams(
      false, 0.0, NetworkTimeTracker::FETCHES_IN_BACKGROUND_AND_ON_DEMAND,
      NetworkTimeTracker::ClockDriftSamples::NO_SAMPLES);
  EXPECT_FALSE(tracker_->QueryTimeServiceForTesting());
  // The timer is not started when the feature is disabled.
  EXPECT_EQ(base::Minutes(0), tracker_->GetTimerDelayForTesting());

  // Enable time queries and check that a query is sent.
  field_trial_test_->SetFeatureParams(
      true, 0.0, NetworkTimeTracker::FETCHES_IN_BACKGROUND_AND_ON_DEMAND,
      NetworkTimeTracker::ClockDriftSamples::NO_SAMPLES);
  SetResponseHandler(base::BindRepeating(&GoodTimeResponseHandler));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);
}

TEST_F(NetworkTimeTrackerTest, UpdateFromNetworkBadSignature) {
  SetResponseHandler(base::BindRepeating(&BadSignatureResponseHandler));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);

  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_NO_SUCCESSFUL_SYNC,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  EXPECT_EQ(base::Minutes(120), tracker_->GetTimerDelayForTesting());
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
  SetResponseHandler(
      base::BindRepeating(&NetworkTimeTrackerTest::BadDataResponseHandler));
  base::StringPiece key = {reinterpret_cast<const char*>(kDevKeyPubBytes),
                           sizeof(kDevKeyPubBytes)};
  tracker_->SetPublicKeyForTesting(key);
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);
  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_NO_SUCCESSFUL_SYNC,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  EXPECT_EQ(base::Minutes(120), tracker_->GetTimerDelayForTesting());
}

TEST_F(NetworkTimeTrackerTest, UpdateFromNetworkServerError) {
  SetResponseHandler(
      base::BindRepeating(&NetworkTimeTrackerTest::ServerErrorResponseHandler));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);

  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_NO_SUCCESSFUL_SYNC,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  // Should see backoff in the error case.
  EXPECT_EQ(base::Minutes(120), tracker_->GetTimerDelayForTesting());
}

#if BUILDFLAG(IS_IOS)
// http://crbug.com/658619
#define MAYBE_UpdateFromNetworkNetworkError     \
    DISABLED_UpdateFromNetworkNetworkError
#else
#define MAYBE_UpdateFromNetworkNetworkError UpdateFromNetworkNetworkError
#endif
TEST_F(NetworkTimeTrackerTest, MAYBE_UpdateFromNetworkNetworkError) {
  SetResponseHandler(base::BindRepeating(
      &NetworkTimeTrackerTest::NetworkErrorResponseHandler));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);

  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_NO_SUCCESSFUL_SYNC,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
  // Should see backoff in the error case.
  EXPECT_EQ(base::Minutes(120), tracker_->GetTimerDelayForTesting());
}

TEST_F(NetworkTimeTrackerTest, UpdateFromNetworkLargeResponse) {
  SetResponseHandler(base::BindRepeating(&GoodTimeResponseHandler));

  base::Time out_network_time;

  tracker_->SetMaxResponseSizeForTesting(3);
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_NO_SUCCESSFUL_SYNC,
            tracker_->GetNetworkTime(&out_network_time, nullptr));

  tracker_->SetMaxResponseSizeForTesting(1024);
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_AVAILABLE,
            tracker_->GetNetworkTime(&out_network_time, nullptr));
}

TEST_F(NetworkTimeTrackerTest, UpdateFromNetworkFirstSyncPending) {
  SetResponseHandler(
      base::BindRepeating(&NetworkTimeTrackerTest::BadDataResponseHandler));
  base::StringPiece key = {reinterpret_cast<const char*>(kDevKeyPubBytes),
                           sizeof(kDevKeyPubBytes)};
  tracker_->SetPublicKeyForTesting(key);
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());

  // Do not wait for the fetch to complete; ask for the network time
  // immediately while the request is still pending.
  base::Time out_network_time;
  EXPECT_EQ(NetworkTimeTracker::NETWORK_TIME_FIRST_SYNC_PENDING,
            tracker_->GetNetworkTime(&out_network_time, nullptr));

  tracker_->WaitForFetchForTesting(123123123);
}

TEST_F(NetworkTimeTrackerTest, UpdateFromNetworkSubseqeuntSyncPending) {
  SetResponseHandler(
      base::BindRepeating(&NetworkTimeTrackerTest::BadDataResponseHandler));
  base::StringPiece key = {reinterpret_cast<const char*>(kDevKeyPubBytes),
                           sizeof(kDevKeyPubBytes)};
  tracker_->SetPublicKeyForTesting(key);
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

  tracker_->WaitForFetchForTesting(123123123);
}

namespace {

// NetworkTimeTrackerTest.TimeBetweenFetchesHistogram needs to make several time
// queries that return different times.
// MultipleGoodTimeResponseHandler::ResponseHandler is like
// GoodTimeResponseHandler, but returning different times on each of three
// requests that happen in sequence.
//
// See comments inline for how to update the times that are returned.
class MultipleGoodTimeResponseHandler {
 public:
  MultipleGoodTimeResponseHandler() {}

  MultipleGoodTimeResponseHandler(const MultipleGoodTimeResponseHandler&) =
      delete;
  MultipleGoodTimeResponseHandler& operator=(
      const MultipleGoodTimeResponseHandler&) = delete;

  ~MultipleGoodTimeResponseHandler() {}

  MockedResponse ResponseHandler();

  // Returns the time that is returned in the (i-1)'th response handled by
  // ResponseHandler(), or null base::Time() if too many responses have been
  // handled.
  base::Time GetTimeAtIndex(unsigned int i);

 private:
  // The index into |kGoodTimeResponseHandlerJsTime|, |kGoodTimeResponseBody|,
  // and |kGoodTimeResponseServerProofHeaders| that will be used in the
  // response in the next ResponseHandler() call.
  unsigned int next_time_index_ = 0;
};

MockedResponse MultipleGoodTimeResponseHandler::ResponseHandler() {
  if (next_time_index_ >= std::size(kGoodTimeResponseBody)) {
    return MockedResponse{network::CreateURLResponseHead(net::HTTP_BAD_REQUEST),
                          ""};
  }

  network::mojom::URLResponseHeadPtr head =
      network::CreateURLResponseHead(net::HTTP_OK);
  head->headers->AddHeader(
      "x-cup-server-proof",
      kGoodTimeResponseServerProofHeader[next_time_index_]);

  // Simulate response latency.
  head->load_timing.send_end = base::TimeTicks::Now();
  head->load_timing.receive_headers_start =
      head->load_timing.send_end + kGoodTimeResponseLatency[next_time_index_];
  MockedResponse response{std::move(head),
                          kGoodTimeResponseBody[next_time_index_]};
  next_time_index_++;
  return response;
}

base::Time MultipleGoodTimeResponseHandler::GetTimeAtIndex(unsigned int i) {
  if (i >= std::size(kGoodTimeResponseHandlerJsTime))
    return base::Time();
  return base::Time::FromMillisecondsSinceUnixEpoch(
      kGoodTimeResponseHandlerJsTime[i]);
}

}  // namespace

TEST_F(NetworkTimeTrackerTest, ClockSkewHistograms) {
  field_trial_test_->SetFeatureParams(
      true, 1.0 /* query probability */,
      NetworkTimeTracker::FETCHES_IN_BACKGROUND_AND_ON_DEMAND,
      NetworkTimeTracker::ClockDriftSamples::NO_SAMPLES);

  MultipleGoodTimeResponseHandler response_handler;
  base::HistogramTester histograms_first;
  base::TimeDelta mean_latency = base::Seconds(0);

  SetResponseHandler(
      base::BindRepeating(&MultipleGoodTimeResponseHandler::ResponseHandler,
                          base::Unretained(&response_handler)));

  clock_->SetNow(base::Time::FromMillisecondsSinceUnixEpoch(
      kGoodTimeResponseHandlerJsTime[0] + 3500));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting(/*on_demand=*/false));
  tracker_->WaitForFetchForTesting(123123123);
  base::TimeDelta latency1 = kGoodTimeResponseLatency[0];
  mean_latency += latency1;

  std::unique_ptr<base::HistogramSamples> samples_positive(
      histograms_first.GetHistogramSamplesSinceCreation(
          "PrivacyBudget.ClockSkew.Magnitude.Positive"));
  EXPECT_EQ(1, samples_positive->GetCount(
                   (base::Seconds(3.5) - latency1 / 2).InMilliseconds()));
  EXPECT_EQ(1, samples_positive->TotalCount());

  std::unique_ptr<base::HistogramSamples> samples_negative(
      histograms_first.GetHistogramSamplesSinceCreation(
          "PrivacyBudget.ClockSkew.Magnitude.Negative"));
  EXPECT_EQ(0, samples_negative->TotalCount());

  std::unique_ptr<base::HistogramSamples> samples_latency(
      histograms_first.GetHistogramSamplesSinceCreation(
          "PrivacyBudget.ClockSkew.FetchLatency"));
  EXPECT_EQ(1, samples_latency->TotalCount());
  EXPECT_EQ(1, samples_latency->GetCount(latency1.InMilliseconds()));

  std::unique_ptr<base::HistogramSamples> samples_latency_jitter(
      histograms_first.GetHistogramSamplesSinceCreation(
          "PrivacyBudget.ClockSkew.FetchLatencyJitter"));
  EXPECT_EQ(0, samples_latency_jitter->TotalCount());

  base::HistogramTester histograms_second;

  clock_->SetNow(base::Time::FromMillisecondsSinceUnixEpoch(
      kGoodTimeResponseHandlerJsTime[1] + 3500));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting(/*on_demand=*/false));
  tracker_->WaitForFetchForTesting(123123123);
  base::TimeDelta latency2 = kGoodTimeResponseLatency[1];
  mean_latency += latency2;

  samples_positive = histograms_second.GetHistogramSamplesSinceCreation(
      "PrivacyBudget.ClockSkew.Magnitude.Positive");
  EXPECT_EQ(1, samples_positive->GetCount(
                   (base::Seconds(3.5) - latency2 / 2).InMilliseconds()));
  EXPECT_EQ(1, samples_positive->TotalCount());

  samples_negative = histograms_second.GetHistogramSamplesSinceCreation(
      "PrivacyBudget.ClockSkew.Magnitude.Negative");
  EXPECT_EQ(0, samples_negative->TotalCount());

  samples_latency = histograms_second.GetHistogramSamplesSinceCreation(
      "PrivacyBudget.ClockSkew.FetchLatency");
  EXPECT_EQ(1, samples_latency->TotalCount());
  EXPECT_EQ(1, samples_latency->GetCount(latency2.InMilliseconds()));

  samples_latency_jitter = histograms_second.GetHistogramSamplesSinceCreation(
      "PrivacyBudget.ClockSkew.FetchLatencyJitter");
  EXPECT_EQ(0, samples_latency_jitter->TotalCount());

  base::HistogramTester histograms_third;
  clock_->SetNow(base::Time::FromMillisecondsSinceUnixEpoch(
      kGoodTimeResponseHandlerJsTime[2] - 2500));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting(/*on_demand=*/false));
  tracker_->WaitForFetchForTesting(123123123);
  base::TimeDelta latency3 = kGoodTimeResponseLatency[2];
  mean_latency += latency3;

  samples_positive = histograms_third.GetHistogramSamplesSinceCreation(
      "PrivacyBudget.ClockSkew.Magnitude.Positive");
  EXPECT_EQ(0, samples_positive->TotalCount());

  samples_negative = histograms_third.GetHistogramSamplesSinceCreation(
      "PrivacyBudget.ClockSkew.Magnitude.Negative");
  EXPECT_EQ(1, samples_negative->TotalCount());
  EXPECT_EQ(1, samples_negative->GetCount(
                   (base::Seconds(2.5) + latency3 / 2).InMilliseconds()));
  samples_latency = histograms_third.GetHistogramSamplesSinceCreation(
      "PrivacyBudget.ClockSkew.FetchLatency");
  EXPECT_EQ(1, samples_latency->TotalCount());
  EXPECT_EQ(1, samples_latency->GetCount(latency3.InMilliseconds()));
  // After three fetches, the FetchLatencyJitter should be reported.
  samples_latency_jitter = histograms_third.GetHistogramSamplesSinceCreation(
      "PrivacyBudget.ClockSkew.FetchLatencyJitter");
  EXPECT_EQ(1, samples_latency_jitter->TotalCount());
  mean_latency /= 3.0;
  int64_t stddev = (mean_latency - latency1).InMicroseconds() *
                       (mean_latency - latency1).InMicroseconds() +
                   (mean_latency - latency2).InMicroseconds() *
                       (mean_latency - latency2).InMicroseconds() +
                   (mean_latency - latency3).InMicroseconds() *
                       (mean_latency - latency3).InMicroseconds();
  stddev = std::lround(std::sqrt(base::strict_cast<double>(stddev)));
  EXPECT_EQ(1, samples_latency_jitter->GetCount(
                   base::Microseconds(stddev).InMilliseconds()));
}

TEST_F(NetworkTimeTrackerTest, ClockSkewHistogramsEmptyForOnDemandChecks) {
  field_trial_test_->SetFeatureParams(
      true, 1.0 /* query probability */,
      NetworkTimeTracker::FETCHES_IN_BACKGROUND_AND_ON_DEMAND,
      NetworkTimeTracker::ClockDriftSamples::NO_SAMPLES);
  MultipleGoodTimeResponseHandler response_handler;
  base::HistogramTester histograms;

  SetResponseHandler(
      base::BindRepeating(&MultipleGoodTimeResponseHandler::ResponseHandler,
                          base::Unretained(&response_handler)));

  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting(/*on_demand=*/true));
  tracker_->WaitForFetchForTesting(123123123);

  histograms.ExpectTotalCount("PrivacyBudget.ClockSkew.Magnitude.Positive", 0);
  histograms.ExpectTotalCount("PrivacyBudget.ClockSkew.Magnitude.Negative", 0);
  histograms.ExpectTotalCount("PrivacyBudget.ClockSkew.FetchLatency", 0);
  histograms.ExpectTotalCount("PrivacyBudget.ClockSkew.FetchLatencyJitter", 0);
}

TEST_F(NetworkTimeTrackerTest, ClockDriftHistogramsEmptyForOnDemandChecks) {
  MultipleGoodTimeResponseHandler response_handler;
  base::HistogramTester histograms;

  field_trial_test_->SetFeatureParams(
      true, 1.0 /* query probability */,
      NetworkTimeTracker::FETCHES_IN_BACKGROUND_AND_ON_DEMAND,
      NetworkTimeTracker::ClockDriftSamples::TWO_SAMPLES);

  SetResponseHandler(
      base::BindRepeating(&MultipleGoodTimeResponseHandler::ResponseHandler,
                          base::Unretained(&response_handler)));
  clock_->SetNow(base::Time::FromMillisecondsSinceUnixEpoch(
      kGoodTimeResponseHandlerJsTime[0]));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting(/*on_demand=*/true));
  tracker_->WaitForFetchForTesting(123123123);
  clock_->SetNow(base::Time::FromMillisecondsSinceUnixEpoch(
      kGoodTimeResponseHandlerJsTime[1]));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting(/*on_demand=*/true));
  tracker_->WaitForFetchForTesting(123123123);
  clock_->SetNow(base::Time::FromMillisecondsSinceUnixEpoch(
      kGoodTimeResponseHandlerJsTime[2]));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting(/*on_demand=*/true));
  tracker_->WaitForFetchForTesting(123123123);
  clock_->SetNow(base::Time::FromMillisecondsSinceUnixEpoch(
      kGoodTimeResponseHandlerJsTime[3]));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting(/*on_demand=*/true));
  tracker_->WaitForFetchForTesting(123123123);

  histograms.ExpectTotalCount("PrivacyBudget.ClockDrift.Magnitude.Positive", 0);
  histograms.ExpectTotalCount("PrivacyBudget.ClockDrift.Magnitude.Negative", 0);
  histograms.ExpectTotalCount("PrivacyBudget.ClockDrift.FetchLatencyVariance",
                              0);
}

TEST_F(NetworkTimeTrackerTest, ClockDriftHistogramsPositive) {
  MultipleGoodTimeResponseHandler response_handler;
  base::HistogramTester histograms;

  field_trial_test_->SetFeatureParams(
      true, 1.0 /* query probability */,
      NetworkTimeTracker::FETCHES_IN_BACKGROUND_AND_ON_DEMAND,
      NetworkTimeTracker::ClockDriftSamples::TWO_SAMPLES);

  SetResponseHandler(
      base::BindRepeating(&MultipleGoodTimeResponseHandler::ResponseHandler,
                          base::Unretained(&response_handler)));

  // This part will trigger a skew measurement fetch first, followed by a drift
  // measurement using two samples.
  clock_->SetNow(base::Time::FromMillisecondsSinceUnixEpoch(
      kGoodTimeResponseHandlerJsTime[0]));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting(/*on_demand=*/false));
  tracker_->WaitForFetchForTesting(123123123);

  // The next measurements are used for computing drift.
  clock_->SetNow(base::Time::FromMillisecondsSinceUnixEpoch(
      kGoodTimeResponseHandlerJsTime[1]));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting(/*on_demand=*/false));
  tracker_->WaitForFetchForTesting(123123123);
  base::TimeDelta latency1 = kGoodTimeResponseLatency[1];

  // We add an on demand time query in the middle to check it does not interfere
  // with our samples.
  clock_->SetNow(base::Time::FromMillisecondsSinceUnixEpoch(
      kGoodTimeResponseHandlerJsTime[2]));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting(/*on_demand=*/true));
  tracker_->WaitForFetchForTesting(123123123);

  clock_->SetNow(base::Time::FromMillisecondsSinceUnixEpoch(
      kGoodTimeResponseHandlerJsTime[3]));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting(/*on_demand=*/false));
  tracker_->WaitForFetchForTesting(123123123);

  clock_->SetNow(base::Time::FromMillisecondsSinceUnixEpoch(
      kGoodTimeResponseHandlerJsTime[4] + 150));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting(/*on_demand=*/false));
  tracker_->WaitForFetchForTesting(123123123);
  base::TimeDelta latency3 = kGoodTimeResponseLatency[4];

  double expected_positive_drift =
      (base::Milliseconds(150) - latency3 / 2 + latency1 / 2).InMicroseconds() /
      (base::Time::FromMillisecondsSinceUnixEpoch(
           kGoodTimeResponseHandlerJsTime[4] + 150) -
       base::Time::FromMillisecondsSinceUnixEpoch(
           kGoodTimeResponseHandlerJsTime[1]))
          .InSeconds();
  ASSERT_GT(expected_positive_drift, 0);
  histograms.ExpectTotalCount("PrivacyBudget.ClockDrift.Magnitude.Positive", 1);
  histograms.ExpectUniqueSample("PrivacyBudget.ClockDrift.Magnitude.Positive",
                                expected_positive_drift, 1);

  histograms.ExpectTotalCount("PrivacyBudget.ClockDrift.Magnitude.Negative", 0);

  histograms.ExpectTotalCount("PrivacyBudget.ClockDrift.FetchLatencyVariance",
                              1);

  base::TimeDelta mean = (latency1 + latency3) / 2.0;
  double variance =
      ((latency1 - mean).InMilliseconds() * (latency1 - mean).InMilliseconds() +
       (latency3 - mean).InMilliseconds() *
           (latency3 - mean).InMilliseconds()) /
      2;
  histograms.ExpectUniqueSample("PrivacyBudget.ClockDrift.FetchLatencyVariance",
                                variance, 1);
}

TEST_F(NetworkTimeTrackerTest, ClockDriftHistogramsNegative) {
  MultipleGoodTimeResponseHandler response_handler;
  base::HistogramTester histograms;

  field_trial_test_->SetFeatureParams(
      true, 1.0 /* query probability */,
      NetworkTimeTracker::FETCHES_IN_BACKGROUND_AND_ON_DEMAND,
      NetworkTimeTracker::ClockDriftSamples::TWO_SAMPLES);

  SetResponseHandler(
      base::BindRepeating(&MultipleGoodTimeResponseHandler::ResponseHandler,
                          base::Unretained(&response_handler)));

  // This part will trigger a skew measurement fetch first, followed by a drift
  // measurement using two samples.
  clock_->SetNow(base::Time::FromMillisecondsSinceUnixEpoch(
      kGoodTimeResponseHandlerJsTime[0]));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting(/*on_demand=*/false));
  tracker_->WaitForFetchForTesting(123123123);

  // These are the two measurements used for computing drift.
  clock_->SetNow(base::Time::FromMillisecondsSinceUnixEpoch(
      kGoodTimeResponseHandlerJsTime[1]));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting(/*on_demand=*/false));
  tracker_->WaitForFetchForTesting(123123123);
  base::TimeDelta latency1 = kGoodTimeResponseLatency[1];

  clock_->SetNow(base::Time::FromMillisecondsSinceUnixEpoch(
      kGoodTimeResponseHandlerJsTime[2]));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting(/*on_demand=*/false));
  tracker_->WaitForFetchForTesting(123123123);

  clock_->SetNow(base::Time::FromMillisecondsSinceUnixEpoch(
      kGoodTimeResponseHandlerJsTime[3] - 1));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting(/*on_demand=*/false));
  tracker_->WaitForFetchForTesting(123123123);
  base::TimeDelta latency3 = kGoodTimeResponseLatency[3];

  double expected_negative_drift =
      (base::Milliseconds(1) - latency1 / 2 + latency3 / 2).InMicroseconds() /
      (base::Time::FromMillisecondsSinceUnixEpoch(
           kGoodTimeResponseHandlerJsTime[3] - 1) -
       base::Time::FromMillisecondsSinceUnixEpoch(
           kGoodTimeResponseHandlerJsTime[1]))
          .InSeconds();
  ASSERT_GT(expected_negative_drift, 0);
  histograms.ExpectTotalCount("PrivacyBudget.ClockDrift.Magnitude.Positive", 0);
  histograms.ExpectTotalCount("PrivacyBudget.ClockDrift.Magnitude.Negative", 1);
  histograms.ExpectUniqueSample("PrivacyBudget.ClockDrift.Magnitude.Negative",
                                expected_negative_drift, 1);

  base::TimeDelta mean = (latency1 + latency3) / 2.0;
  double variance =
      ((latency1 - mean).InMilliseconds() * (latency1 - mean).InMilliseconds() +
       (latency3 - mean).InMilliseconds() *
           (latency3 - mean).InMilliseconds()) /
      2;

  histograms.ExpectTotalCount("PrivacyBudget.ClockDrift.FetchLatencyVariance",
                              1);
  histograms.ExpectUniqueSample("PrivacyBudget.ClockDrift.FetchLatencyVariance",
                                variance, 1);
}

}  // namespace network_time
