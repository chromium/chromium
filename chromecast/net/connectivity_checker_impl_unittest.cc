// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/net/connectivity_checker_impl.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromecast/base/metrics/mock_cast_metrics_helper.h"
#include "chromecast/net/fake_shared_url_loader_factory.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
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

class FakeNetworkConnectionTracker : public network::NetworkConnectionTracker {
 public:
  // Spoof a valid connection type.
  bool GetConnectionType(network::mojom::ConnectionType* type,
                         ConnectionTypeCallback callback) override {
    *type = network::mojom::ConnectionType::CONNECTION_UNKNOWN;
    return true;
  }
};

class ConnectivityCheckerImplTest : public ::testing::Test {
 public:
  ConnectivityCheckerImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        network_connection_tracker_(
            std::make_unique<FakeNetworkConnectionTracker>()) {
    // Create the PendingSharedURLLoaderFactory first to grab a reference to its
    // underlying SharedURLLoaderFactory.
    auto pending_factory =
        std::make_unique<FakePendingSharedURLLoaderFactory>();
    fake_shared_url_loader_factory_ =
        pending_factory->fake_shared_url_loader_factory();

    checker_ = ConnectivityCheckerImpl::Create(
        task_environment_.GetMainThreadTaskRunner(), std::move(pending_factory),
        network_connection_tracker_.get(), /*time_sync_tracker=*/nullptr);
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

  const ConnectivityCheckerImpl& checker() const { return *checker_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  const std::unique_ptr<FakeNetworkConnectionTracker>
      network_connection_tracker_;
  scoped_refptr<FakeSharedURLLoaderFactory> fake_shared_url_loader_factory_;
  NiceMock<metrics::MockCastMetricsHelper> cast_metrics_helper_;
  scoped_refptr<ConnectivityCheckerImpl> checker_;
};

class ConnectivityCheckerImplTestParameterized
    : public ConnectivityCheckerImplTest,
      public ::testing::WithParamInterface<net::HttpStatusCode> {};

TEST_F(ConnectivityCheckerImplTest, StartsDisconnected) {
  EXPECT_FALSE(checker().Connected());
}

TEST_F(ConnectivityCheckerImplTest, DetectsConnected) {
  ConnectAndCheck();
  EXPECT_TRUE(checker().Connected());
}

TEST_P(ConnectivityCheckerImplTestParameterized,
       RecordsDisconnectDueToBadHttpStatus) {
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

TEST_F(ConnectivityCheckerImplTest, RecordsDisconnectDueToRequestTimeout) {
  ConnectAndCheck();

  // Don't send a response for the request.
  test_url_loader_factory().ClearResponses();
  CheckAndExpectRecordedError(
      ConnectivityCheckerImpl::ErrorType::REQUEST_TIMEOUT);
}

TEST_F(ConnectivityCheckerImplTest, RecordsDisconnectDueToNetError) {
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

}  // namespace chromecast
