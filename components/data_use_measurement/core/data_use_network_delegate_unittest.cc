// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_use_measurement/core/data_use_network_delegate.h"

#include <memory>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/data_use_measurement/core/data_use_ascriber.h"
#include "components/data_use_measurement/core/url_request_classifier.h"
#include "components/metrics/data_use_tracker.h"
#include "net/socket/socket_test_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace data_use_measurement {

namespace {

class TestURLRequestClassifier : public base::SupportsUserData::Data,
                                 public URLRequestClassifier {
 public:
  static const void* const kUserDataKey;

  DataUseUserData::DataUseContentType GetContentType(
      const net::URLRequest& request,
      const net::HttpResponseHeaders& response_headers) const override {
    return DataUseUserData::OTHER;
  }

  void RecordPageTransitionUMA(uint64_t page_transition,
                               int64_t received_bytes) const override {}

  bool IsFavIconRequest(const net::URLRequest& request) const override {
    return false;
  }
};

class TestDataUseAscriber : public DataUseAscriber {
 public:
  TestDataUseAscriber() {}

  DataUseRecorder* GetOrCreateDataUseRecorder(
      net::URLRequest* request) override {
    return nullptr;
  }
  DataUseRecorder* GetDataUseRecorder(const net::URLRequest& request) override {
    return nullptr;
  }

  std::unique_ptr<net::NetworkDelegate> CreateNetworkDelegate(
      std::unique_ptr<net::NetworkDelegate> wrapped_network_delegate) override {
    return nullptr;
  }

  std::unique_ptr<URLRequestClassifier> CreateURLRequestClassifier()
      const override {
    return nullptr;
  }
};

class TestDataUseMeasurement : public DataUseMeasurement {
 public:
  TestDataUseMeasurement(
      std::unique_ptr<URLRequestClassifier> url_request_classifier,
      DataUseAscriber* ascriber)
      : DataUseMeasurement(std::move(url_request_classifier),
                           ascriber,
                           nullptr) {}

  void UpdateDataUseToMetricsService(int64_t total_bytes,
                                     bool is_cellular,
                                     bool is_metrics_service_usage) override {}
};

// static
const void* const TestURLRequestClassifier::kUserDataKey =
    &TestURLRequestClassifier::kUserDataKey;

// This function requests a URL, and makes it return a known response. If
// |from_user| is true, it attaches a ResourceRequestInfo to the URLRequest,
// because requests from users have this info. If |from_user| is false, the
// request is presumed to be from a service, and the service name is set in the
// request's user data. (As an example suggestions service tag is attached). if
// |redirect| is true, it adds necessary socket data to have it follow redirect
// before getting the final response.
std::unique_ptr<net::URLRequest> RequestURL(
    net::URLRequestContext* context,
    net::MockClientSocketFactory* socket_factory,
    bool from_user,
    bool redirect) {
  net::MockRead redirect_mock_reads[] = {
      net::MockRead("HTTP/1.1 302 Found\r\n"
                    "Location: http://bar.com/\r\n\r\n"),
      net::MockRead(net::SYNCHRONOUS, net::OK),
  };
  net::StaticSocketDataProvider redirect_socket_data_provider(
      redirect_mock_reads, base::span<net::MockWrite>());

  if (redirect)
    socket_factory->AddSocketDataProvider(&redirect_socket_data_provider);
  net::MockRead response_mock_reads[] = {
      net::MockRead("HTTP/1.1 200 OK\r\n\r\n"), net::MockRead("response body"),
      net::MockRead(net::SYNCHRONOUS, net::OK),
  };
  const auto traffic_annotation =
      from_user ? net::DefineNetworkTrafficAnnotation("blink_resource_loader",
                                                      "blink resource loaded "
                                                      "will be treated as "
                                                      "user-initiated request")
                : TRAFFIC_ANNOTATION_FOR_TESTS;
  net::StaticSocketDataProvider response_socket_data_provider(
      response_mock_reads, base::span<net::MockWrite>());
  socket_factory->AddSocketDataProvider(&response_socket_data_provider);
  net::TestDelegate test_delegate;
  std::unique_ptr<net::URLRequest> request(
      context->CreateRequest(GURL("http://example.com"), net::DEFAULT_PRIORITY,
                             &test_delegate, traffic_annotation));

  if (!from_user) {
    request->SetUserData(
        data_use_measurement::DataUseUserData::kUserDataKey,
        std::make_unique<data_use_measurement::DataUseUserData>(
            data_use_measurement::DataUseUserData::FOREGROUND));
  }
  request->Start();
  test_delegate.RunUntilComplete();
  return request;
}

class DataUseNetworkDelegateTest : public testing::Test {
 public:
  DataUseNetworkDelegateTest()
      : context_(true),
        data_use_network_delegate_(
            std::make_unique<net::TestNetworkDelegate>(),
            &test_data_use_ascriber_,
            std::make_unique<TestDataUseMeasurement>(
                std::make_unique<TestURLRequestClassifier>(),
                &test_data_use_ascriber_)) {
    context_.set_client_socket_factory(&mock_socket_factory_);
    context_.set_network_delegate(&data_use_network_delegate_);
    context_.Init();
  }

  net::TestURLRequestContext* context() { return &context_; }
  net::MockClientSocketFactory* socket_factory() {
    return &mock_socket_factory_;
  }

 private:
  base::MessageLoopForIO message_loop_;
  net::MockClientSocketFactory mock_socket_factory_;
  net::TestURLRequestContext context_;
  TestDataUseAscriber test_data_use_ascriber_;
  DataUseNetworkDelegate data_use_network_delegate_;
};

// This function tests data use measurement for requests by services. it makes a
// query which is similar to a query of a service, so it should affect
// DataUse.TrafficSize.System.Dimensions histogram. AppState and ConnectionType
// dimensions are always Foreground and NotCellular respectively.
TEST_F(DataUseNetworkDelegateTest, DataUseMeasurementServiceTest) {
  base::HistogramTester histogram_tester;

  // A query from a service without redirection.
  RequestURL(context(), socket_factory(), false, false);
  EXPECT_FALSE(
      histogram_tester
          .GetTotalCountsForPrefix(
              "DataUse.TrafficSize.System.Downstream.Foreground.NotCellular")
          .empty());
  histogram_tester.ExpectTotalCount(
      "DataUse.TrafficSize.System.Upstream.Foreground.NotCellular", 1);
  histogram_tester.ExpectTotalCount(
      "DataUse.TrafficSize.User.Downstream.Foreground.NotCellular", 0);
  histogram_tester.ExpectTotalCount(
      "DataUse.TrafficSize.User.Upstream.Foreground.NotCellular", 0);
}

// This function tests data use measurement for requests by user.The query from
// a user should affect DataUse.TrafficSize.User.Dimensions histogram. AppState
// and ConnectionType dimensions are always Foreground and NotCellular
// respectively.
TEST_F(DataUseNetworkDelegateTest, DataUseMeasurementUserTest) {
  base::HistogramTester histogram_tester;

  // A query from user without redirection.
  RequestURL(context(), socket_factory(), true, false);
  EXPECT_FALSE(
      histogram_tester
          .GetTotalCountsForPrefix(
              "DataUse.TrafficSize.User.Downstream.Foreground.NotCellular")
          .empty());
  histogram_tester.ExpectTotalCount(
      "DataUse.TrafficSize.User.Upstream.Foreground.NotCellular", 1);
  histogram_tester.ExpectTotalCount(
      "DataUse.TrafficSize.System.Downstream.Foreground.NotCellular", 0);
  histogram_tester.ExpectTotalCount(
      "DataUse.TrafficSize.System.Upstream.Foreground.NotCellular", 0);
}

// This function tests data use measurement for requests by services in case the
// request is redirected once. it makes a query which is similar to a query of a
// service, so it should affect DataUse.TrafficSize.System.Dimensions and
// histogram. AppState and ConnectionType dimensions are always Foreground and
// NotCellular respectively.
TEST_F(DataUseNetworkDelegateTest, DataUseMeasurementServiceTestWithRedirect) {
  base::HistogramTester histogram_tester;

  // A query from user with one redirection.
  RequestURL(context(), socket_factory(), false, true);
  EXPECT_FALSE(
      histogram_tester
          .GetTotalCountsForPrefix(
              "DataUse.TrafficSize.System.Downstream.Foreground.NotCellular")
          .empty());
  histogram_tester.ExpectTotalCount(
      "DataUse.TrafficSize.System.Upstream.Foreground.NotCellular", 2);
  // Two uploads and two downloads message, so totalCount should be 4.
  histogram_tester.ExpectTotalCount(
      "DataUse.TrafficSize.User.Downstream.Foreground.NotCellular", 0);
  histogram_tester.ExpectTotalCount(
      "DataUse.TrafficSize.User.Upstream.Foreground.NotCellular", 0);
}

// This function tests data use measurement for requests by user in case the
// request is redirected once.The query from a user should affect
// DataUse.TrafficSize.User.Dimensions histogram. AppState and ConnectionType
// dimensions are always Foreground and NotCellular respectively.
TEST_F(DataUseNetworkDelegateTest, DataUseMeasurementUserTestWithRedirect) {
  base::HistogramTester histogram_tester;

  // A query from user with one redirection.
  RequestURL(context(), socket_factory(), true, true);

  EXPECT_FALSE(
      histogram_tester
          .GetTotalCountsForPrefix(
              "DataUse.TrafficSize.User.Downstream.Foreground.NotCellular")
          .empty());
  histogram_tester.ExpectTotalCount(
      "DataUse.TrafficSize.User.Upstream.Foreground.NotCellular", 2);
  histogram_tester.ExpectTotalCount(
      "DataUse.TrafficSize.System.Downstream.Foreground.NotCellular", 0);
  histogram_tester.ExpectTotalCount(
      "DataUse.TrafficSize.System.Upstream.Foreground.NotCellular", 0);
}

}  // namespace

}  // namespace data_use_measurement
