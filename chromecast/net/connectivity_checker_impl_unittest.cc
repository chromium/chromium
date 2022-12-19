// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/net/connectivity_checker_impl.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromecast/base/metrics/mock_cast_metrics_helper.h"
#include "chromecast/net/fake_shared_url_loader_factory.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/network_change_manager.mojom-shared.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chromecast {

using ::testing::InvokeWithoutArgs;
using ::testing::NiceMock;

constexpr const char* kDefaultConnectivityCheckUrls[] = {
    kDefaultConnectivityCheckUrl,
    kHttpConnectivityCheckUrl,
};

// Number of consecutive connectivity check errors before status is changed
// to offline.
const unsigned int kNumErrorsToNotifyOffline = 3;

// Most tests use the TestNetworkConnectionChecker from
// services/network/test/test_network_connection_tracker.h, but some tests
// require testing how many times GetConnectionType() is called by
// ConnectivityCheckerImpl. For these tests, we use a fake that records the
// number of invocations and otherwise has the default behavior.
class FakeNetworkConnectionTracker : public network::NetworkConnectionTracker {
 public:
  // Spoof a valid connection type.
  bool GetConnectionType(network::mojom::ConnectionType* type,
                         ConnectionTypeCallback callback) override {
    check_counter_++;
    *type = network::mojom::ConnectionType::CONNECTION_UNKNOWN;
    return true;
  }

  void NotifyNetworkTypeChanged(network::mojom::ConnectionType type) {
    OnNetworkChanged(type);
  }

  unsigned int check_counter() const { return check_counter_; }

 private:
  // To memorize how many times GetConnectionType() called by checker
  unsigned int check_counter_ = 0;
};

class ConnectivityCheckPeriods {
 public:
  ConnectivityCheckPeriods(base::TimeDelta disconnected_check_period,
                           base::TimeDelta connected_check_period)
      : disconnected_check_period_(disconnected_check_period),
        connected_check_period_(connected_check_period) {}

  static const ConnectivityCheckPeriods Empty() { return empty_; }

  bool IsEmpty() {
    return disconnected_check_period_ == empty_.disconnected_check_period_ &&
           connected_check_period_ == empty_.connected_check_period_;
  }

  const base::TimeDelta disconnected_check_period_;
  const base::TimeDelta connected_check_period_;

 private:
  // empty object: use minimum and negative TimeDeltas for empty object.
  static const ConnectivityCheckPeriods empty_;
};

const ConnectivityCheckPeriods ConnectivityCheckPeriods::empty_ =
    ConnectivityCheckPeriods(base::TimeDelta::Min(), base::TimeDelta::Min());

std::ostream& operator<<(std::ostream& out, const ConnectivityCheckPeriods& x) {
  return out << "{disconnected_check_period: " << x.disconnected_check_period_
             << ", connected_check_period: " << x.connected_check_period_
             << "}";
}

template <typename NetworkConnectivityCheckerT>
class ConnectivityCheckerImplBaseTest : public ::testing::Test {
 public:
  explicit ConnectivityCheckerImplBaseTest(
      std::unique_ptr<NetworkConnectivityCheckerT> tracker,
      ConnectivityCheckPeriods check_periods =
          ConnectivityCheckPeriods::Empty(),
      std::unique_ptr<FakePendingSharedURLLoaderFactory> pending_factory =
          std::make_unique<FakePendingSharedURLLoaderFactory>())
      : tracker_(std::move(tracker)),
        fake_shared_url_loader_factory_(
            pending_factory->fake_shared_url_loader_factory()),
        checker_(check_periods.IsEmpty()
                     ? ConnectivityCheckerImpl::Create(
                           task_environment_.GetMainThreadTaskRunner(),
                           std::move(pending_factory),
                           tracker_.get(),
                           /* time_sync_tracker */ nullptr)
                     : ConnectivityCheckerImpl::Create(
                           task_environment_.GetMainThreadTaskRunner(),
                           std::move(pending_factory),
                           tracker_.get(),
                           check_periods.disconnected_check_period_,
                           check_periods.connected_check_period_,
                           /* time_sync_tracker */ nullptr)) {
    checker_->SetCastMetricsHelperForTesting(&cast_metrics_helper_);
  }

  void SetUp() final {
    // Run pending initialization tasks.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() final { test_url_loader_factory().ClearResponses(); }

 protected:
  void SetResponsesWithStatusCode(net::HttpStatusCode status) {
    for (const char* url : kDefaultConnectivityCheckUrls) {
      test_url_loader_factory().AddResponse(url, /*content=*/"", status);
    }
  }

  void ConnectAndCheck() {
    SetResponsesWithStatusCode(kConnectivitySuccessStatusCode);
    checker_->Check();
    base::RunLoop().RunUntilIdle();
    test_url_loader_factory().ClearResponses();
  }

  void DisconnectAndCheck() {
    SetResponsesWithStatusCode(net::HTTP_INTERNAL_SERVER_ERROR);
    checker_->Check();
    base::RunLoop().RunUntilIdle();
    test_url_loader_factory().ClearResponses();
  }

  void CheckAndExpectRecordedError(
      ConnectivityCheckerImpl::ErrorType error_type) {
    base::RunLoop run_loop;
    EXPECT_CALL(cast_metrics_helper_,
                RecordEventWithValue("Network.ConnectivityChecking.ErrorType",
                                     static_cast<int>(error_type)))
        .WillOnce(
            InvokeWithoutArgs([quit = run_loop.QuitClosure()] { quit.Run(); }));
    checker_->Check();
    run_loop.Run();
  }

  network::TestURLLoaderFactory& test_url_loader_factory() {
    return fake_shared_url_loader_factory_->test_url_loader_factory();
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
  };
  std::unique_ptr<NetworkConnectivityCheckerT> tracker_;
  scoped_refptr<FakeSharedURLLoaderFactory> fake_shared_url_loader_factory_;
  NiceMock<metrics::MockCastMetricsHelper> cast_metrics_helper_;
  scoped_refptr<ConnectivityCheckerImpl> checker_;
};

class ConnectivityCheckerImplTest : public ConnectivityCheckerImplBaseTest<
                                        network::TestNetworkConnectionTracker> {
 public:
  ConnectivityCheckerImplTest()
      : ConnectivityCheckerImplBaseTest<network::TestNetworkConnectionTracker>(
            network::TestNetworkConnectionTracker::CreateInstance()) {}
};

TEST_F(ConnectivityCheckerImplTest, StartsDisconnected) {
  EXPECT_FALSE(checker_->Connected());
}

TEST_F(ConnectivityCheckerImplTest, DetectsConnected) {
  tracker_->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_UNKNOWN);
  ConnectAndCheck();
  EXPECT_TRUE(checker_->Connected());
}

class ConnectivityCheckerImplTestParameterized
    : public ConnectivityCheckerImplTest,
      public ::testing::WithParamInterface<net::HttpStatusCode> {};

TEST_P(ConnectivityCheckerImplTestParameterized,
       RecordsDisconnectDueToBadHttpStatus) {
  tracker_->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_UNKNOWN);
  ConnectAndCheck();
  SetResponsesWithStatusCode(GetParam());
  CheckAndExpectRecordedError(
      ConnectivityCheckerImpl::ErrorType::BAD_HTTP_STATUS);
}

// Test 3xx, 4xx, 5xx responses.
INSTANTIATE_TEST_SUITE_P(ConnectivityCheckerImplTestBadHttpStatus,
                         ConnectivityCheckerImplTestParameterized,
                         ::testing::Values(net::HTTP_TEMPORARY_REDIRECT,
                                           net::HTTP_BAD_REQUEST,
                                           net::HTTP_INTERNAL_SERVER_ERROR));

class ConnectivityCheckerImplTestPeriodParameterized
    : public ConnectivityCheckerImplBaseTest<
          network::TestNetworkConnectionTracker>,
      // disconnected probe period
      public ::testing::WithParamInterface<ConnectivityCheckPeriods> {
 public:
  ConnectivityCheckerImplTestPeriodParameterized()
      : ConnectivityCheckerImplBaseTest<network::TestNetworkConnectionTracker>(
            network::TestNetworkConnectionTracker::CreateInstance(),
            GetParam()) {}
};

TEST_P(ConnectivityCheckerImplTestPeriodParameterized,
       CheckWithCustomizedPeriodsConnected) {
  const ConnectivityCheckPeriods periods = GetParam();
  const base::TimeDelta margin = base::Milliseconds(100);
  tracker_->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_UNKNOWN);

  // Initial: disconnected. First Check.
  // Next check is scheduled in disconnected_check_period_.
  DisconnectAndCheck();
  // Connect.
  SetResponsesWithStatusCode(kConnectivitySuccessStatusCode);

  // Jump to right before the next Check. Result is still not connected.
  task_environment_.FastForwardBy(periods.disconnected_check_period_ - margin);
  EXPECT_FALSE(checker_->Connected());
  // After the Check --> connected.
  // Next check is scheduled in connected_check_period_.
  task_environment_.FastForwardBy(margin * 2);
  EXPECT_TRUE(checker_->Connected());
}

TEST_P(ConnectivityCheckerImplTestPeriodParameterized,
       CheckWithCustomizedPeriodsDisconnected) {
  const ConnectivityCheckPeriods periods = GetParam();
  const base::TimeDelta margin = base::Milliseconds(100);
  tracker_->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_UNKNOWN);

  // Initial: connected. First Check.
  // Next check is scheduled in disconnected_check_period_.
  ConnectAndCheck();
  // Disconnect.
  SetResponsesWithStatusCode(net::HTTP_INTERNAL_SERVER_ERROR);

  // Jump to right before the next Check. Result is still connected.
  task_environment_.FastForwardBy(periods.connected_check_period_ - margin);
  EXPECT_TRUE(checker_->Connected());

  // After the Check, still connected.
  // It retries kNumErrorsToNotifyOffline times to switch to disconnected.
  task_environment_.FastForwardBy(margin * 2);
  // Fast forward by kNumErrorsToNotifyOffline * connected_check_period_.
  for (unsigned int i = 0; i < kNumErrorsToNotifyOffline; i++) {
    EXPECT_TRUE(checker_->Connected());
    // Check again.
    task_environment_.FastForwardBy(periods.disconnected_check_period_);
  }
  // After retries, the result becomes disconnected.
  EXPECT_FALSE(checker_->Connected());
}

// Test various connected/disconnected check periods
INSTANTIATE_TEST_SUITE_P(
    ConnectivityCheckerImplTestCheckPeriods,
    ConnectivityCheckerImplTestPeriodParameterized,
    ::testing::Values(
        ConnectivityCheckPeriods(base::Seconds(1), base::Seconds(1)),
        ConnectivityCheckPeriods(base::Seconds(1), base::Seconds(60)),
        ConnectivityCheckPeriods(base::Seconds(60), base::Seconds(1)),
        ConnectivityCheckPeriods(base::Seconds(10), base::Seconds(120)),
        ConnectivityCheckPeriods(base::Seconds(50), base::Seconds(200))));

TEST_F(ConnectivityCheckerImplTest, RecordsDisconnectDueToRequestTimeout) {
  tracker_->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_UNKNOWN);
  ConnectAndCheck();

  // Don't send a response for the request.
  test_url_loader_factory().ClearResponses();
  CheckAndExpectRecordedError(
      ConnectivityCheckerImpl::ErrorType::REQUEST_TIMEOUT);
}

TEST_F(ConnectivityCheckerImplTest, RecordsDisconnectDueToNetError) {
  tracker_->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_UNKNOWN);
  ConnectAndCheck();

  // Set up a generic failure
  network::URLLoaderCompletionStatus status;
  status.error_code = net::ERR_FAILED;

  // Simulate network responses using the configured network error.
  for (const char* url : kDefaultConnectivityCheckUrls) {
    test_url_loader_factory().AddResponse(
        GURL(url),
        network::CreateURLResponseHead(kConnectivitySuccessStatusCode),
        /*content=*/"", status, /*redirects=*/{},
        network::TestURLLoaderFactory::kSendHeadersOnNetworkError);
  }

  CheckAndExpectRecordedError(ConnectivityCheckerImpl::ErrorType::NET_ERROR);
}

TEST_F(ConnectivityCheckerImplTest, InitialCheckIsNotDelayed) {
  // Do not set an initial connection type. This causes the first check to be
  // delayed.
  EXPECT_FALSE(checker_->Connected());
  // Notify that a network connection is established after the checker is
  // constructed. A check should be automatically started when the connection
  // is established.
  SetResponsesWithStatusCode(kConnectivitySuccessStatusCode);
  tracker_->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_UNKNOWN);
  // Poke the task runner, but don't fast forward the clock, to prove that the
  // check is not a delayed task. We don't want the first check to be delayed
  // after the network is initially connected because a lot of initialization of
  // the Cast shell is bottlenecked on establishing network connectivity, and
  // any delay of the initial check shows up as a user-visible delay in the
  // Cast receiver becoming discoverable after boot.
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_TRUE(checker_->Connected());
}

TEST_F(ConnectivityCheckerImplTest, InitialCheck_NoNetwork) {
  // Do not set an initial connection type. This causes the first check to be
  // delayed.
  EXPECT_FALSE(checker_->Connected());
  // Initialize the network tracker to indicate that there is no connection at
  // first.
  SetResponsesWithStatusCode(kConnectivitySuccessStatusCode);
  tracker_->SetConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);
  // Network connection still isn't established after flushing tasks.
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_FALSE(checker_->Connected());
  // Establish a connection.
  SetResponsesWithStatusCode(kConnectivitySuccessStatusCode);
  tracker_->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_UNKNOWN);
  // Subsequent network checks are delayed due to rate limiting, so the
  // connectivity status doesn't update until delay elapses.
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_FALSE(checker_->Connected());
  task_environment_.FastForwardBy(kNetworkChangedDelay);
  EXPECT_TRUE(checker_->Connected());
}

class ConnectivityCheckerImplTestPeriodicCheck
    : public ConnectivityCheckerImplBaseTest<FakeNetworkConnectionTracker>,
      public ::testing::WithParamInterface<ConnectivityCheckPeriods> {
 public:
  ConnectivityCheckerImplTestPeriodicCheck()
      : ConnectivityCheckerImplBaseTest<FakeNetworkConnectionTracker>(
            std::make_unique<FakeNetworkConnectionTracker>(),
            GetParam()) {}
};

TEST_P(ConnectivityCheckerImplTestPeriodicCheck, NoDuplicateConnectedCheck) {
  const ConnectivityCheckPeriods periods = GetParam();
  constexpr base::TimeDelta kCheckRequestDelay = base::Milliseconds(100);
  constexpr unsigned int kRounds = 10;
  tracker_->NotifyNetworkTypeChanged(
      network::mojom::ConnectionType::CONNECTION_UNKNOWN);

  // Initial: connected. First Check.
  // A check is scheduled in connected_check_period_.
  ConnectAndCheck();

  // Add a delay to prevent the new check() from being ignored due to the
  // duplicate url loader request
  task_environment_.FastForwardBy(kCheckRequestDelay);
  SetResponsesWithStatusCode(kConnectivitySuccessStatusCode);
  tracker_->NotifyNetworkTypeChanged(
      network::mojom::ConnectionType::CONNECTION_WIFI);

  // Wait for the internal network change delay.
  // A check will be executed and the next check will be schedule in
  // connected_check_period_. The old scheduled check should be removed.
  task_environment_.FastForwardBy(kNetworkChangedDelay);

  // Fast forward and count the times of check()
  unsigned int counter_start = tracker_->check_counter();
  task_environment_.FastForwardBy(periods.connected_check_period_ * kRounds);

  // The check_counter should increase by kRounds.
  EXPECT_EQ(tracker_->check_counter() - counter_start, kRounds);
}

TEST_P(ConnectivityCheckerImplTestPeriodicCheck, NoDuplicateDisconnectedCheck) {
  const ConnectivityCheckPeriods periods = GetParam();
  constexpr base::TimeDelta kCheckRequestDelay = base::Milliseconds(100);
  constexpr unsigned int kRounds = 10;
  tracker_->NotifyNetworkTypeChanged(
      network::mojom::ConnectionType::CONNECTION_UNKNOWN);

  // Initial: disconnected. First Check.
  // A check is scheduled in disconnected_check_period_.
  DisconnectAndCheck();

  // Add a delay to prevent the new check() from being ignored due to the
  // duplicate url loader request
  task_environment_.FastForwardBy(kCheckRequestDelay);
  SetResponsesWithStatusCode(net::HTTP_INTERNAL_SERVER_ERROR);
  tracker_->NotifyNetworkTypeChanged(
      network::mojom::ConnectionType::CONNECTION_WIFI);

  // Wait for the internal network change delay.
  // A check will be executed and the next check will be schedule in
  // disconnected_check_period_. The old scheduled check should be removed.
  task_environment_.FastForwardBy(kNetworkChangedDelay);

  // Fast forward and count the times of check()
  unsigned int counter_start = tracker_->check_counter();
  task_environment_.FastForwardBy(periods.disconnected_check_period_ * kRounds);

  // The check_counter should increase by kRounds.
  EXPECT_EQ(tracker_->check_counter() - counter_start, kRounds);
}

INSTANTIATE_TEST_SUITE_P(
    ConnectivityCheckerImplTestCheckPeriodicCheck,
    ConnectivityCheckerImplTestPeriodicCheck,
    ::testing::Values(
        ConnectivityCheckPeriods(base::Seconds(1), base::Seconds(1)),
        ConnectivityCheckPeriods(base::Seconds(10), base::Seconds(10)),
        ConnectivityCheckPeriods(base::Seconds(1), base::Seconds(60))));

}  // namespace chromecast
