// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_time/network_time_tracker.h"

#include <memory>
#include <string>
#include <string_view>
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

struct MockedResponse {
  network::mojom::URLResponseHeadPtr head;
  std::string body;
  network::URLLoaderCompletionStatus status;
};

}  // namespace

class NetworkTimeTrackerTest : public ::testing::Test {
 public:
  class NetworkTimeTestObserver
      : public NetworkTimeTracker::NetworkTimeObserver {
   public:
    NetworkTimeTestObserver() = default;
    ~NetworkTimeTestObserver() override = default;

    void OnNetworkTimeChanged(TimeTracker::TimeTrackerState state) override {
      times_called_++;
      last_state_ = state;
    }
    int times_called_ = 0;
    TimeTracker::TimeTrackerState last_state_;
  };

  ~NetworkTimeTrackerTest() override = default;

  NetworkTimeTrackerTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::IO),
        field_trial_test_(new FieldTrialTest()),
        clock_(new base::SimpleTestClock),
        tick_clock_(new base::SimpleTestTickClock) {
    NetworkTimeTracker::RegisterPrefs(pref_service_.registry());

    field_trial_test_->SetFeatureParams(
        true, 0.0 /* query probability */,
        NetworkTimeTracker::FETCHES_IN_BACKGROUND_AND_ON_DEMAND);

    url_loader_factory_.SetInterceptor(base::BindRepeating(
        &NetworkTimeTrackerTest::Intercept, weak_ptr_factory_.GetWeakPtr()));

    tracker_ = std::make_unique<NetworkTimeTracker>(
        std::unique_ptr<base::Clock>(clock_),
        std::unique_ptr<const base::TickClock>(tick_clock_), &pref_service_,
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_),
        std::nullopt);

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
  void Reset(std::optional<NetworkTimeTracker::FetchBehavior> behavior) {
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
            &url_loader_factory_),
        behavior);
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
  Reset(std::nullopt);
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
  std::optional<double> local, network;
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
  Reset(std::nullopt);
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
  Reset(std::nullopt);
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
  Reset(std::nullopt);
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
  Reset(std::nullopt);
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
      true, 0.0, NetworkTimeTracker::FETCHES_IN_BACKGROUND_ONLY);
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
      true, 0.0, NetworkTimeTracker::FETCHES_IN_BACKGROUND_AND_ON_DEMAND);
  base::Time in_network_time = clock_->Now();
  UpdateNetworkTime(in_network_time, resolution_, latency_,
                    tick_clock_->NowTicks());

  // No query should be started so long as NetworkTimeTracker is synced, but the
  // next check should happen soon.
  EXPECT_FALSE(tracker_->QueryTimeServiceForTesting());
  EXPECT_EQ(base::Minutes(6), tracker_->GetTimerDelayForTesting());

  field_trial_test_->SetFeatureParams(
      true, 1.0, NetworkTimeTracker::FETCHES_IN_BACKGROUND_AND_ON_DEMAND);
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);
  EXPECT_EQ(base::Minutes(60), tracker_->GetTimerDelayForTesting());
}

TEST_F(NetworkTimeTrackerTest, NoNetworkQueryWhileFeatureDisabled) {
  // Disable network time queries and check that a query is not sent.
  field_trial_test_->SetFeatureParams(
      false, 0.0, NetworkTimeTracker::FETCHES_IN_BACKGROUND_AND_ON_DEMAND);
  EXPECT_FALSE(tracker_->QueryTimeServiceForTesting());
  // The timer is not started when the feature is disabled.
  EXPECT_EQ(base::Minutes(0), tracker_->GetTimerDelayForTesting());

  // Enable time queries and check that a query is sent.
  field_trial_test_->SetFeatureParams(
      true, 0.0, NetworkTimeTracker::FETCHES_IN_BACKGROUND_AND_ON_DEMAND);
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
  std::string_view key = {reinterpret_cast<const char*>(kDevKeyPubBytes),
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
  std::string_view key = {reinterpret_cast<const char*>(kDevKeyPubBytes),
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
  std::string_view key = {reinterpret_cast<const char*>(kDevKeyPubBytes),
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

TEST_F(NetworkTimeTrackerTest, CustomFetchBehaviorTest) {
  // On creation, the test is configured as if the feature param is set to
  // FETCHES_IN_BACKGROUND_AND_ON_DEMAND.
  EXPECT_EQ(
      NetworkTimeTracker::FetchBehavior::FETCHES_IN_BACKGROUND_AND_ON_DEMAND,
      tracker_->GetFetchBehavior());
  // When created with a parameter, the tracker should ignore the feature param,
  // and instead use the parameter.
  Reset(NetworkTimeTracker::FetchBehavior::FETCHES_IN_BACKGROUND_ONLY);
  EXPECT_EQ(NetworkTimeTracker::FetchBehavior::FETCHES_IN_BACKGROUND_ONLY,
            tracker_->GetFetchBehavior());
}

TEST_F(NetworkTimeTrackerTest, ObserverTest) {
  NetworkTimeTestObserver observer;
  base::Time now = clock_->Now();
  base::TimeTicks now_ticks = tick_clock_->NowTicks();
  base::Time in_network_time = now;
  tracker_->AddObserver(&observer);
  UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                    now_ticks);
  base::TimeDelta expected_offset = latency_ / 2;

  EXPECT_EQ(observer.times_called_, 1);
  EXPECT_EQ(observer.last_state_.known_time, in_network_time - latency_ / 2);
  EXPECT_EQ(observer.last_state_.system_time, now - expected_offset);
  EXPECT_EQ(observer.last_state_.system_ticks, now_ticks - expected_offset);
  EXPECT_EQ(observer.last_state_.uncertainty,
            resolution_ + latency_ + adjustment_);
}

}  // namespace network_time
