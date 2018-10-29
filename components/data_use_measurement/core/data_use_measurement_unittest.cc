// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_use_measurement/core/data_use_measurement.h"

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "components/data_use_measurement/core/data_use_ascriber.h"
#include "components/data_use_measurement/core/data_use_recorder.h"
#include "components/data_use_measurement/core/url_request_classifier.h"
#include "net/base/network_change_notifier.h"
#include "net/base/request_priority.h"
#include "net/socket/socket_test_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "base/android/application_status_listener.h"
#endif

namespace data_use_measurement {

class TestURLRequestClassifier : public base::SupportsUserData::Data,
                                 public URLRequestClassifier {
 public:
  static const void* const kUserDataKey;

  TestURLRequestClassifier() : content_type_(DataUseUserData::OTHER) {}

  DataUseUserData::DataUseContentType GetContentType(
      const net::URLRequest& request,
      const net::HttpResponseHeaders& response_headers) const override {
    return content_type_;
  }

  void set_content_type(DataUseUserData::DataUseContentType content_type) {
    content_type_ = content_type;
  }

  void RecordPageTransitionUMA(uint64_t page_transition,
                               int64_t received_bytes) const override {}

  bool IsFavIconRequest(const net::URLRequest& request) const override {
    return false;
  }

 private:
  DataUseUserData::DataUseContentType content_type_;
};

class TestDataUseAscriber : public DataUseAscriber {
 public:
  TestDataUseAscriber() : recorder_(DataUse::TrafficType::USER_TRAFFIC) {}

  DataUseRecorder* GetOrCreateDataUseRecorder(
      net::URLRequest* request) override {
    return &recorder_;
  }

  DataUseRecorder* GetDataUseRecorder(const net::URLRequest& request) override {
    return &recorder_;
  }

  std::unique_ptr<net::NetworkDelegate> CreateNetworkDelegate(
      std::unique_ptr<net::NetworkDelegate> wrapped_network_delegate) override {
    return nullptr;
  }

  std::unique_ptr<URLRequestClassifier> CreateURLRequestClassifier()
      const override {
    return nullptr;
  }

  void SetTabVisibility(bool visible) { recorder_.set_is_visible(visible); }

 private:
  DataUseRecorder recorder_;
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
                                     bool is_metrics_service_usage) override {
    is_data_use_forwarder_called_ = true;
  }

  bool is_data_use_forwarder_called_ = false;
};

// The more usual initialization of kUserDataKey would be along the lines of
//     const void* const TestURLRequestClassifier::kUserDataKey =
//         &TestURLRequestClassifier::kUserDataKey;
// but lld's identical constant folding then folds that with
// DataUseUserData::kUserDataKey which is initialized like that as well, and
// then TestURLRequestClassifier::IsUserRequest() starts classifying service
// requests as user requests.  To work around this, make
// TestURLRequestClassifier::kUserDataKey point to an arbitrary integer
// instead.
// TODO(thakis): If we changed lld to only ICF over code and not over data,
// we could undo this again.
const int kICFBuster = 12345634;

// static
const void* const TestURLRequestClassifier::kUserDataKey = &kICFBuster;

class DataUseMeasurementTest : public testing::Test {
 public:
  DataUseMeasurementTest()
      : url_request_classifier_(new TestURLRequestClassifier()),
        data_use_measurement_(
            std::unique_ptr<URLRequestClassifier>(url_request_classifier_),
            &ascriber_) {
    // During the test it is expected to not have cellular connection.
    DCHECK(!net::NetworkChangeNotifier::IsConnectionCellular(
        net::NetworkChangeNotifier::GetConnectionType()));
  }

  // Creates a test request.
  enum RequestKind { kServiceRequest, kUserRequest };
  std::unique_ptr<net::URLRequest> CreateTestRequest(
      RequestKind is_user_request) {
    net::TestDelegate test_delegate;
    InitializeContext();
    net::MockRead reads[] = {net::MockRead("HTTP/1.1 200 OK\r\n"
                                           "Content-Length: 12\r\n\r\n"),
                             net::MockRead("Test Content")};
    net::StaticSocketDataProvider socket_data(reads,
                                              base::span<net::MockWrite>());
    socket_factory_->AddSocketDataProvider(&socket_data);

    const auto traffic_annotation =
        (is_user_request == kServiceRequest)
            ? TRAFFIC_ANNOTATION_FOR_TESTS
            : net::DefineNetworkTrafficAnnotation("blink_resource_loader",
                                                  "blink resource loaded will "
                                                  "be treated as "
                                                  "user-initiated request");

    std::unique_ptr<net::URLRequest> request(
        context_->CreateRequest(GURL("http://foo.com"), net::DEFAULT_PRIORITY,
                                &test_delegate, traffic_annotation));
    if (is_user_request == kServiceRequest) {
      request->SetUserData(
          data_use_measurement::DataUseUserData::kUserDataKey,
          std::make_unique<data_use_measurement::DataUseUserData>(
              data_use_measurement_.CurrentAppState()));
    }

    request->Start();
    base::RunLoop().RunUntilIdle();
    return request;
  }

  // Sends a request and reports data use attaching either user data or service
  // data based on |is_user_request|.
  void SendRequest(RequestKind is_user_request) {
    std::unique_ptr<net::URLRequest> request =
        CreateTestRequest(is_user_request);
    data_use_measurement_.OnBeforeURLRequest(request.get());
    data_use_measurement_.OnNetworkBytesReceived(*request, 1000);
    data_use_measurement_.OnNetworkBytesSent(*request, 100);
    data_use_measurement_.OnCompleted(*request, true);
  }

  // This function makes a user request and confirms that its effect is
  // reflected in proper histograms.
  void TestForAUserRequest(const std::string& target_dimension) {
    base::HistogramTester histogram_tester;
    SendRequest(kUserRequest);
    histogram_tester.ExpectTotalCount("DataUse.TrafficSize.User.Downstream." +
                                          target_dimension + kConnectionType,
                                      1);
    histogram_tester.ExpectTotalCount("DataUse.TrafficSize.User.Upstream." +
                                          target_dimension + kConnectionType,
                                      1);
    histogram_tester.ExpectTotalCount(
        "DataUse.MessageSize.AllServices.Upstream." + target_dimension +
            kConnectionType,
        0);
    histogram_tester.ExpectTotalCount(
        "DataUse.MessageSize.AllServices.Downstream." + target_dimension +
            kConnectionType,
        0);
  }

  // This function makes a service request and confirms that its effect is
  // reflected in proper histograms.
  void TestForAServiceRequest(const std::string& target_dimension) {
    base::HistogramTester histogram_tester;
    SendRequest(kServiceRequest);
    histogram_tester.ExpectTotalCount("DataUse.TrafficSize.System.Downstream." +
                                          target_dimension + kConnectionType,
                                      1);
    histogram_tester.ExpectTotalCount("DataUse.TrafficSize.System.Upstream." +
                                          target_dimension + kConnectionType,
                                      1);
  }

  DataUseMeasurement* data_use_measurement() { return &data_use_measurement_; }

  bool IsDataUseForwarderCalled() {
    return data_use_measurement_.is_data_use_forwarder_called_;
  }

 protected:
  void InitializeContext() {
    context_.reset(new net::TestURLRequestContext(true));
    socket_factory_.reset(new net::MockClientSocketFactory());
    context_->set_client_socket_factory(socket_factory_.get());
    context_->Init();
  }

  base::MessageLoopForIO loop_;

  TestDataUseAscriber ascriber_;
  TestURLRequestClassifier* url_request_classifier_;
  TestDataUseMeasurement data_use_measurement_;

  std::unique_ptr<net::MockClientSocketFactory> socket_factory_;
  std::unique_ptr<net::TestURLRequestContext> context_;
  const std::string kConnectionType = "NotCellular";

  DISALLOW_COPY_AND_ASSIGN(DataUseMeasurementTest);
};

// This test function tests recording of data use information in UMA histogram
// when packet is originated from user or services when the app is in the
// foreground or the OS is not Android.
// TODO(amohammadkhan): Add tests for Cellular/non-cellular connection types
// when support for testing is provided in its class.
TEST_F(DataUseMeasurementTest, UserNotUserTest) {
#if defined(OS_ANDROID)
  data_use_measurement()->OnApplicationStateChangeForTesting(
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
#endif
  TestForAServiceRequest("Foreground.");
  TestForAUserRequest("Foreground.");
}

#if defined(OS_ANDROID)
// This test function tests recording of data use information in UMA histogram
// when packet is originated from user or services when the app is in the
// background and OS is Android.
TEST_F(DataUseMeasurementTest, ApplicationStateTest) {
  data_use_measurement()->OnApplicationStateChangeForTesting(
      base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES);
  TestForAServiceRequest("Background.");
  TestForAUserRequest("Background.");
}
#endif

TEST_F(DataUseMeasurementTest, DataUseForwarderIsCalled) {
  EXPECT_FALSE(IsDataUseForwarderCalled());
  SendRequest(kUserRequest);
  EXPECT_TRUE(IsDataUseForwarderCalled());
}

#if defined(OS_ANDROID)
TEST_F(DataUseMeasurementTest, AppStateUnknown) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<net::URLRequest> request = CreateTestRequest(kUserRequest);
  data_use_measurement_.OnBeforeURLRequest(request.get());

  {
    base::HistogramTester histogram_tester;
    data_use_measurement()->OnApplicationStateChangeForTesting(
        base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
    data_use_measurement_.OnNetworkBytesSent(*request, 100);
    data_use_measurement_.OnNetworkBytesReceived(*request, 1000);
    histogram_tester.ExpectTotalCount(
        "DataUse.TrafficSize.User.Downstream.Foreground." + kConnectionType, 1);
    histogram_tester.ExpectTotalCount(
        "DataUse.TrafficSize.User.Upstream.Foreground." + kConnectionType, 1);
  }

  {
    base::HistogramTester histogram_tester;
    data_use_measurement()->OnApplicationStateChangeForTesting(
        base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES);
    data_use_measurement_.OnNetworkBytesReceived(*request, 1000);
    histogram_tester.ExpectTotalCount(
        "DataUse.TrafficSize.User.Downstream.Unknown." + kConnectionType, 1);
  }

  {
    base::HistogramTester histogram_tester;
    data_use_measurement()->OnApplicationStateChangeForTesting(
        base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES);
    data_use_measurement_.OnNetworkBytesReceived(*request, 1000);
    histogram_tester.ExpectTotalCount(
        "DataUse.TrafficSize.User.Downstream.Background." + kConnectionType, 1);
  }
}

TEST_F(DataUseMeasurementTest, TimeOfBackgroundDownstreamBytes) {
  {
    std::unique_ptr<net::URLRequest> request = CreateTestRequest(kUserRequest);
    data_use_measurement_.OnBeforeURLRequest(request.get());
    base::HistogramTester histogram_tester;
    data_use_measurement()->OnApplicationStateChangeForTesting(
        base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
    data_use_measurement_.OnNetworkBytesSent(*request, 100);
    data_use_measurement_.OnNetworkBytesReceived(*request, 1000);
    data_use_measurement_.OnNetworkBytesSent(*request, 200);
    data_use_measurement_.OnNetworkBytesReceived(*request, 2000);
    histogram_tester.ExpectTotalCount(
        "DataUse.BackgroundToDataRecievedPerByte.User", 0);
    histogram_tester.ExpectTotalCount(
        "DataUse.BackgroundToFirstDownstream.User", 0);
  }

  {
    // Create new request when app is in foreground..
    base::HistogramTester histogram_tester;
    std::unique_ptr<net::URLRequest> request = CreateTestRequest(kUserRequest);
    data_use_measurement_.OnBeforeURLRequest(request.get());
    data_use_measurement_.OnNetworkBytesSent(*request, 100);
    data_use_measurement_.OnNetworkBytesReceived(*request, 1000);
    data_use_measurement_.OnNetworkBytesSent(*request, 200);
    data_use_measurement_.OnNetworkBytesReceived(*request, 2000);
    histogram_tester.ExpectTotalCount(
        "DataUse.BackgroundToDataRecievedPerByte.User", 0);
    histogram_tester.ExpectTotalCount(
        "DataUse.BackgroundToFirstDownstream.User", 0);
  }

  {
    std::unique_ptr<net::URLRequest> request = CreateTestRequest(kUserRequest);
    data_use_measurement_.OnBeforeURLRequest(request.get());
    base::HistogramTester histogram_tester;
    data_use_measurement()->OnApplicationStateChangeForTesting(
        base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES);
    data_use_measurement_.OnNetworkBytesSent(*request, 100);
    data_use_measurement_.OnNetworkBytesReceived(*request, 1000);
    data_use_measurement_.OnNetworkBytesSent(*request, 200);
    data_use_measurement_.OnNetworkBytesReceived(*request, 2000);
    histogram_tester.ExpectTotalCount(
        "DataUse.BackgroundToDataRecievedPerByte.User", 3000);
    histogram_tester.ExpectTotalCount(
        "DataUse.BackgroundToFirstDownstream.User", 1);
  }

  {
    // Create new request when app is in background.
    base::HistogramTester histogram_tester;
    std::unique_ptr<net::URLRequest> request = CreateTestRequest(kUserRequest);
    data_use_measurement_.OnBeforeURLRequest(request.get());
    data_use_measurement_.OnNetworkBytesSent(*request, 100);
    data_use_measurement_.OnNetworkBytesReceived(*request, 1000);
    data_use_measurement_.OnNetworkBytesSent(*request, 200);
    data_use_measurement_.OnNetworkBytesReceived(*request, 2000);
    histogram_tester.ExpectTotalCount(
        "DataUse.BackgroundToDataRecievedPerByte.User", 3000);
    histogram_tester.ExpectTotalCount(
        "DataUse.BackgroundToFirstDownstream.User", 0);
  }

  {
    // Create new request when app is in background.
    base::HistogramTester histogram_tester;
    std::unique_ptr<net::URLRequest> request =
        CreateTestRequest(kServiceRequest);
    data_use_measurement_.OnBeforeURLRequest(request.get());
    data_use_measurement_.OnNetworkBytesSent(*request, 100);
    data_use_measurement_.OnNetworkBytesReceived(*request, 1000);
    data_use_measurement_.OnNetworkBytesSent(*request, 200);
    data_use_measurement_.OnNetworkBytesReceived(*request, 2000);
    histogram_tester.ExpectTotalCount(
        "DataUse.BackgroundToDataRecievedPerByte.System", 3000);
    histogram_tester.ExpectTotalCount(
        "DataUse.BackgroundToDataRecievedPerByte.User", 0);
    histogram_tester.ExpectTotalCount(
        "DataUse.BackgroundToFirstDownstream.System", 0);
    histogram_tester.ExpectTotalCount(
        "DataUse.BackgroundToFirstDownstream.User", 0);
  }

  {
    std::unique_ptr<net::URLRequest> request = CreateTestRequest(kUserRequest);
    data_use_measurement_.OnBeforeURLRequest(request.get());
    base::HistogramTester histogram_tester;
    data_use_measurement()->OnApplicationStateChangeForTesting(
        base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
    data_use_measurement_.OnNetworkBytesSent(*request, 100);
    data_use_measurement_.OnNetworkBytesReceived(*request, 1000);
    data_use_measurement_.OnNetworkBytesSent(*request, 200);
    data_use_measurement_.OnNetworkBytesReceived(*request, 2000);
    histogram_tester.ExpectTotalCount(
        "DataUse.BackgroundToDataRecievedPerByte.User", 0);
    histogram_tester.ExpectTotalCount(
        "DataUse.BackgroundToFirstDownstream.User", 0);
  }
}

TEST_F(DataUseMeasurementTest, AppTabState) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<net::URLRequest> request = CreateTestRequest(kUserRequest);

  // App in foreground, Tab in background.
  data_use_measurement_.OnBeforeURLRequest(request.get());
  data_use_measurement_.OnNetworkBytesReceived(*request, 1000);
  data_use_measurement_.OnNetworkBytesSent(*request, 100);

  histogram_tester.ExpectTotalCount(
      "DataUse.AppTabState.Upstream.AppForeground.TabBackground", 1);
  histogram_tester.ExpectTotalCount(
      "DataUse.AppTabState.Downstream.AppForeground.TabBackground", 1);

  // App and Tab in foreground.
  ascriber_.SetTabVisibility(true);
  data_use_measurement_.OnBeforeURLRequest(request.get());
  data_use_measurement_.OnNetworkBytesReceived(*request, 1000);
  data_use_measurement_.OnNetworkBytesSent(*request, 100);

  histogram_tester.ExpectTotalCount(
      "DataUse.AppTabState.Upstream.AppForeground.TabForeground", 1);
  histogram_tester.ExpectTotalCount(
      "DataUse.AppTabState.Downstream.AppForeground.TabForeground", 1);

  // App and Tab in background.
  data_use_measurement()->OnApplicationStateChangeForTesting(
      base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES);
  ascriber_.SetTabVisibility(false);
  data_use_measurement_.OnBeforeURLRequest(request.get());
  // First network access changes the app state to UNKNOWN and the next nextwork
  // access changes to BACKGROUND.
  data_use_measurement_.OnNetworkBytesReceived(*request, 1000);
  data_use_measurement_.OnNetworkBytesSent(*request, 100);

  histogram_tester.ExpectTotalCount(
      "DataUse.AppTabState.Upstream.AppBackground", 1);
  histogram_tester.ExpectTotalCount(
      "DataUse.AppTabState.Downstream.AppBackground", 1);
}

TEST_F(DataUseMeasurementTest, ContentType) {
  {
    base::HistogramTester histogram_tester;
    std::unique_ptr<net::URLRequest> request = CreateTestRequest(kUserRequest);
    data_use_measurement_.OnBeforeURLRequest(request.get());
    data_use_measurement_.OnNetworkBytesReceived(*request, 10 * 1024);
    histogram_tester.ExpectUniqueSample("DataUse.ContentType.UserTrafficKB",
                                        DataUseUserData::OTHER, 10);
  }

  {
    base::HistogramTester histogram_tester;
    std::unique_ptr<net::URLRequest> request =
        CreateTestRequest(kServiceRequest);
    data_use_measurement_.OnBeforeURLRequest(request.get());
    data_use_measurement_.OnNetworkBytesReceived(*request, 1024);
    histogram_tester.ExpectUniqueSample("DataUse.ContentType.ServicesKB",
                                        DataUseUserData::OTHER, 1);
  }

  // Video request in foreground.
  {
    base::HistogramTester histogram_tester;
    std::unique_ptr<net::URLRequest> request = CreateTestRequest(kUserRequest);

    ascriber_.SetTabVisibility(true);
    url_request_classifier_->set_content_type(DataUseUserData::VIDEO);
    data_use_measurement_.OnBeforeURLRequest(request.get());
    data_use_measurement_.OnHeadersReceived(request.get(), nullptr);
    data_use_measurement_.OnNetworkBytesReceived(*request, 10 * 1024);

    histogram_tester.ExpectUniqueSample("DataUse.ContentType.UserTrafficKB",
                                        DataUseUserData::VIDEO, 10);
  }

  // Audio request from background tab.
  {
    base::HistogramTester histogram_tester;
    std::unique_ptr<net::URLRequest> request = CreateTestRequest(kUserRequest);
    ascriber_.SetTabVisibility(false);
    url_request_classifier_->set_content_type(DataUseUserData::AUDIO);
    data_use_measurement_.OnBeforeURLRequest(request.get());
    data_use_measurement_.OnHeadersReceived(request.get(), nullptr);
    data_use_measurement_.OnNetworkBytesReceived(*request, 10 * 1024);

    histogram_tester.ExpectUniqueSample("DataUse.ContentType.UserTrafficKB",
                                        DataUseUserData::AUDIO_TABBACKGROUND,
                                        10);
  }

  // Video request from background app.
  {
    base::HistogramTester histogram_tester;
    std::unique_ptr<net::URLRequest> request = CreateTestRequest(kUserRequest);
    url_request_classifier_->set_content_type(DataUseUserData::VIDEO);
    data_use_measurement()->OnApplicationStateChangeForTesting(
        base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES);
    ascriber_.SetTabVisibility(false);
    data_use_measurement_.OnBeforeURLRequest(request.get());
    data_use_measurement_.OnHeadersReceived(request.get(), nullptr);
    data_use_measurement_.OnNetworkBytesReceived(*request, 10 * 1024);

    histogram_tester.ExpectUniqueSample("DataUse.ContentType.UserTrafficKB",
                                        DataUseUserData::VIDEO_APPBACKGROUND,
                                        10);
  }
}

TEST_F(DataUseMeasurementTest, ContentTypeInKB) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<net::URLRequest> request = CreateTestRequest(kUserRequest);
  url_request_classifier_->set_content_type(DataUseUserData::VIDEO);
  data_use_measurement()->OnApplicationStateChangeForTesting(
      base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES);
  ascriber_.SetTabVisibility(false);
  data_use_measurement_.OnBeforeURLRequest(request.get());
  data_use_measurement_.OnHeadersReceived(request.get(), nullptr);
  data_use_measurement_.OnNetworkBytesReceived(*request, 1024);

  // UserTrafficKB metric is recorded for the first 1KB of data use.
  histogram_tester.ExpectTotalCount("DataUse.ContentType.UserTrafficKB", 1);

  data_use_measurement_.OnNetworkBytesReceived(*request, 3 * 1024);

  // UserTrafficKB recorded for the total 4KB.
  histogram_tester.ExpectUniqueSample("DataUse.ContentType.UserTrafficKB",
                                      DataUseUserData::VIDEO_APPBACKGROUND, 4);
}

#endif

}  // namespace data_use_measurement
