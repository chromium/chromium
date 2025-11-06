// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atomic>
#include <memory>
#include <tuple>

#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "components/cronet/native/test/test_request_finished_info_listener.h"
#include "components/cronet/native/test/test_upload_data_provider.h"
#include "components/cronet/native/test/test_url_request_callback.h"
#include "components/cronet/native/test/test_util.h"
#include "cronet_c.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using cronet::test::TestRequestFinishedInfoListener;
using cronet::test::TestUploadDataProvider;
using cronet::test::TestUrlRequestCallback;
using ::testing::HasSubstr;

namespace {

// A Cronet_UrlRequestStatusListener impl that waits for OnStatus callback.
class StatusListener {
 public:
  // |callback| is verified to not yet have reached a final state when
  // OnStatus() is called back.
  explicit StatusListener(TestUrlRequestCallback* callback)
      : status_listener_(Cronet_UrlRequestStatusListener_CreateWith(
            StatusListener::OnStatus)),
        callback_(callback),
        expect_request_not_done_(false) {
    Cronet_UrlRequestStatusListener_SetClientContext(status_listener_, this);
  }

  StatusListener(const StatusListener&) = delete;
  StatusListener& operator=(const StatusListener&) = delete;

  ~StatusListener() {
    Cronet_UrlRequestStatusListener_Destroy(status_listener_);
  }

  // Wait for and return request status.
  Cronet_UrlRequestStatusListener_Status GetStatus(
      Cronet_UrlRequestPtr request) {
    Cronet_UrlRequest_GetStatus(request, status_listener_);
    // NOTE(pauljensen): There's no guarantee this line will get executed
    // before OnStatus() reads |expect_request_not_done_|.  It's very unlikely
    // it will get read before this write, but if it does it just means
    // OnStatus() won't check that the final callback has not been issued yet.
    expect_request_not_done_ = !Cronet_UrlRequest_IsDone(request);
    awaiting_status_.Wait();
    return status_;
  }

 private:
  // Cronet_UrlRequestStatusListener OnStatus impl.
  static void OnStatus(Cronet_UrlRequestStatusListenerPtr self,
                       Cronet_UrlRequestStatusListener_Status status) {
    StatusListener* listener = static_cast<StatusListener*>(
        Cronet_UrlRequestStatusListener_GetClientContext(self));

    // Enforce we call OnStatus() before OnSucceeded/OnFailed/OnCanceled().
    if (listener->expect_request_not_done_)
      EXPECT_FALSE(listener->callback_->IsDone());

    listener->status_ = status;
    listener->awaiting_status_.Signal();
  }

  Cronet_UrlRequestStatusListenerPtr const status_listener_;
  const raw_ptr<TestUrlRequestCallback> callback_;

  Cronet_UrlRequestStatusListener_Status status_ =
      Cronet_UrlRequestStatusListener_Status_INVALID;
  base::WaitableEvent awaiting_status_;

  // Indicates if GetStatus() was called before request finished, indicating
  // that OnStatus() should be called before request finishes. The writing of
  // this variable races the reading of it, but it's initialized to a safe
  // value.
  std::atomic_bool expect_request_not_done_;
};

enum class RequestFinishedListenerType {
  kNoListener,          // Don't add a request finished listener.
  kUrlRequestListener,  // Add a request finished listener to the UrlRequest.
  kEngineListener,      // Add a request finished listener to the Engine.
};

// Converts a Cronet_DateTimePtr into the int64 number of milliseconds since
// the UNIX epoch.
//
// Returns -1 if |date_time| is nullptr.
int64_t DateToMillis(Cronet_DateTimePtr date_time) {
  if (date_time == nullptr) {
    return -1;
  }
  int64_t value = Cronet_DateTime_value_get(date_time);
  // Cronet_DateTime fields shouldn't be before the UNIX epoch.
  //
  // While DateToMillis() callers can easily check this themselves (and
  // produce more descriptive errors showing which field is violating), they
  // can't easily distinguish a nullptr vs -1 value, so we check for -1 here.
  EXPECT_NE(-1, value);
  return value;
}

// Verification check that the date isn't wildly off, somehow (perhaps due to
// read of used memory, wild pointer, etc.).
//
// Interpreted as milliseconds after the UNIX timestamp, this timestamp occurs
// at 37,648 C.E.
constexpr int64_t kDateOverrunThreshold = 1LL << 50;

// Basic verification checking of all Cronet_Metrics fields. For optional
// fields, we allow the field to be non-present. Start/end pairs should be
// monotonic (end not less than start).
//
// Ordering of events is also checked.
void VerifyRequestMetrics(Cronet_MetricsPtr metrics) {
  EXPECT_GE(DateToMillis(Cronet_Metrics_request_start_get(metrics)), 0);
  EXPECT_LT(DateToMillis(Cronet_Metrics_request_start_get(metrics)),
            kDateOverrunThreshold);
  EXPECT_GE(DateToMillis(Cronet_Metrics_request_end_get(metrics)),
            DateToMillis(Cronet_Metrics_request_start_get(metrics)));
  EXPECT_LT(DateToMillis(Cronet_Metrics_request_end_get(metrics)),
            kDateOverrunThreshold);

  EXPECT_GE(DateToMillis(Cronet_Metrics_dns_start_get(metrics)), -1);
  EXPECT_LT(DateToMillis(Cronet_Metrics_dns_start_get(metrics)),
            kDateOverrunThreshold);
  EXPECT_GE(DateToMillis(Cronet_Metrics_dns_end_get(metrics)),
            DateToMillis(Cronet_Metrics_dns_start_get(metrics)));
  EXPECT_LT(DateToMillis(Cronet_Metrics_dns_end_get(metrics)),
            kDateOverrunThreshold);

  EXPECT_GE(DateToMillis(Cronet_Metrics_connect_start_get(metrics)), -1);
  EXPECT_LT(DateToMillis(Cronet_Metrics_connect_start_get(metrics)),
            kDateOverrunThreshold);
  EXPECT_GE(DateToMillis(Cronet_Metrics_connect_end_get(metrics)),
            DateToMillis(Cronet_Metrics_connect_start_get(metrics)));
  EXPECT_LT(DateToMillis(Cronet_Metrics_connect_end_get(metrics)),
            kDateOverrunThreshold);

  EXPECT_GE(DateToMillis(Cronet_Metrics_ssl_start_get(metrics)), -1);
  EXPECT_LT(DateToMillis(Cronet_Metrics_ssl_start_get(metrics)),
            kDateOverrunThreshold);
  EXPECT_GE(DateToMillis(Cronet_Metrics_ssl_end_get(metrics)),
            DateToMillis(Cronet_Metrics_ssl_start_get(metrics)));
  EXPECT_LT(DateToMillis(Cronet_Metrics_ssl_end_get(metrics)),
            kDateOverrunThreshold);

  EXPECT_GE(DateToMillis(Cronet_Metrics_sending_start_get(metrics)), -1);
  EXPECT_LT(DateToMillis(Cronet_Metrics_sending_start_get(metrics)),
            kDateOverrunThreshold);
  EXPECT_GE(DateToMillis(Cronet_Metrics_sending_end_get(metrics)),
            DateToMillis(Cronet_Metrics_sending_start_get(metrics)));
  EXPECT_LT(DateToMillis(Cronet_Metrics_sending_end_get(metrics)),
            kDateOverrunThreshold);

  EXPECT_GE(DateToMillis(Cronet_Metrics_push_start_get(metrics)), -1);
  EXPECT_LT(DateToMillis(Cronet_Metrics_push_start_get(metrics)),
            kDateOverrunThreshold);
  EXPECT_GE(DateToMillis(Cronet_Metrics_push_end_get(metrics)),
            DateToMillis(Cronet_Metrics_push_start_get(metrics)));
  EXPECT_LT(DateToMillis(Cronet_Metrics_push_end_get(metrics)),
            kDateOverrunThreshold);

  EXPECT_GE(DateToMillis(Cronet_Metrics_response_start_get(metrics)), -1);
  EXPECT_LT(DateToMillis(Cronet_Metrics_response_start_get(metrics)),
            kDateOverrunThreshold);

  EXPECT_GE(Cronet_Metrics_sent_byte_count_get(metrics), -1);
  EXPECT_GE(Cronet_Metrics_received_byte_count_get(metrics), -1);

  // Verify order of events.
  if (Cronet_Metrics_dns_start_get(metrics) != nullptr) {
    EXPECT_GE(DateToMillis(Cronet_Metrics_dns_start_get(metrics)),
              DateToMillis(Cronet_Metrics_request_start_get(metrics)));
  }

  if (Cronet_Metrics_connect_start_get(metrics) != nullptr &&
      Cronet_Metrics_dns_end_get(metrics) != nullptr) {
    EXPECT_GE(DateToMillis(Cronet_Metrics_connect_start_get(metrics)),
              DateToMillis(Cronet_Metrics_dns_end_get(metrics)));
  }

  if (Cronet_Metrics_ssl_start_get(metrics) != nullptr &&
      Cronet_Metrics_connect_start_get(metrics) != nullptr) {
    EXPECT_GE(DateToMillis(Cronet_Metrics_ssl_start_get(metrics)),
              DateToMillis(Cronet_Metrics_connect_start_get(metrics)));
  }

  if (Cronet_Metrics_connect_end_get(metrics) != nullptr &&
      Cronet_Metrics_ssl_end_get(metrics) != nullptr) {
    EXPECT_GE(DateToMillis(Cronet_Metrics_connect_end_get(metrics)),
              DateToMillis(Cronet_Metrics_ssl_end_get(metrics)));
  }

  if (Cronet_Metrics_sending_start_get(metrics) != nullptr &&
      Cronet_Metrics_connect_end_get(metrics) != nullptr) {
    EXPECT_GE(DateToMillis(Cronet_Metrics_sending_start_get(metrics)),
              DateToMillis(Cronet_Metrics_connect_end_get(metrics)));
  }

  if (Cronet_Metrics_response_start_get(metrics) != nullptr &&
      Cronet_Metrics_sending_end_get(metrics) != nullptr) {
    EXPECT_GE(DateToMillis(Cronet_Metrics_response_start_get(metrics)),
              DateToMillis(Cronet_Metrics_sending_end_get(metrics)));
  }

  if (Cronet_Metrics_response_start_get(metrics) != nullptr) {
    EXPECT_GE(DateToMillis(Cronet_Metrics_request_end_get(metrics)),
              DateToMillis(Cronet_Metrics_response_start_get(metrics)));
  }
}

// Convert a TestUrlRequestCallback::ResponseStep into the equivalent
// RequestFinishedInfo.FINISHED_REASON.
Cronet_RequestFinishedInfo_FINISHED_REASON MapFinishedReason(
    TestUrlRequestCallback::ResponseStep response_step) {
  switch (response_step) {
    case TestUrlRequestCallback::ON_SUCCEEDED:
      return Cronet_RequestFinishedInfo_FINISHED_REASON_SUCCEEDED;
    case TestUrlRequestCallback::ON_FAILED:
      return Cronet_RequestFinishedInfo_FINISHED_REASON_FAILED;
    case TestUrlRequestCallback::ON_CANCELED:
      return Cronet_RequestFinishedInfo_FINISHED_REASON_CANCELED;
    default:
      NOTREACHED() << "Unknown TestUrlRequestCallback::ResponseStep: "
                   << response_step;
  }
}

// Basic verification checking of all Cronet_RequestFinishedInfo,
// Cronet_UrlResponseInfoPtr, and Cronet_ErrorPtr fields passed to
// RequestFinishedInfoListener.OnRequestFinished().
//
// All fields are checked except for |annotations|.
//
// |test_request_finished_info_listener| Test listener -- will verify all fields
//     of this listener.
// |callback| Callback associated with the UrlRequest associated with
//     |request_info|.
void VerifyRequestFinishedInfoListener(
    TestRequestFinishedInfoListener* test_request_finished_info_listener,
    const TestUrlRequestCallback& callback) {
  Cronet_RequestFinishedInfoPtr request_info =
      test_request_finished_info_listener->request_finished_info();
  VerifyRequestMetrics(Cronet_RequestFinishedInfo_metrics_get(request_info));
  auto finished_reason =
      Cronet_RequestFinishedInfo_finished_reason_get(request_info);
  EXPECT_EQ(MapFinishedReason(callback.response_step()), finished_reason);
  EXPECT_EQ(callback.original_response_info(),
            test_request_finished_info_listener->url_response_info());
  EXPECT_EQ(callback.last_error(),
            test_request_finished_info_listener->error());
}

// Parameterized off whether to use a direct executor, and whether (if so, how)
// to add a RequestFinishedInfoListener.
class UrlRequestTest : public ::testing::TestWithParam<
                           std::tuple<bool, RequestFinishedListenerType>> {
 public:
  UrlRequestTest(const UrlRequestTest&) = delete;
  UrlRequestTest& operator=(const UrlRequestTest&) = delete;

 protected:
  UrlRequestTest() = default;
  ~UrlRequestTest() override = default;

  void SetUp() override {}

  void TearDown() override {}

  bool GetDirectExecutorParam() { return std::get<0>(GetParam()); }

  RequestFinishedListenerType GetRequestFinishedListenerTypeParam() {
    return std::get<1>(GetParam());
  }

  std::unique_ptr<TestUrlRequestCallback> StartAndWaitForComplete(
      const std::string& url,
      std::unique_ptr<TestUrlRequestCallback> test_callback,
      const std::string& http_method,
      TestUploadDataProvider* test_upload_data_provider,
      int remapped_port) {
    Cronet_EnginePtr engine = cronet::test::CreateTestEngine(remapped_port);
    Cronet_UrlRequestPtr request = Cronet_UrlRequest_Create();
    Cronet_UrlRequestParamsPtr request_params =
        Cronet_UrlRequestParams_Create();
    Cronet_UrlRequestParams_http_method_set(request_params,
                                            http_method.c_str());
    Cronet_UploadDataProviderPtr upload_data_provider = nullptr;

    // Add upload data provider and set content type required for upload.
    if (test_upload_data_provider != nullptr) {
      test_upload_data_provider->set_url_request(request);
      upload_data_provider =
          test_upload_data_provider->CreateUploadDataProvider();
      Cronet_UrlRequestParams_upload_data_provider_set(request_params,
                                                       upload_data_provider);
      Cronet_UrlRequestParams_upload_data_provider_executor_set(
          request_params, test_upload_data_provider->executor());
      Cronet_HttpHeaderPtr header = Cronet_HttpHeader_Create();
      Cronet_HttpHeader_name_set(header, "Content-Type");
      Cronet_HttpHeader_value_set(header, "Useless/string");
      Cronet_UrlRequestParams_request_headers_add(request_params, header);
      Cronet_HttpHeader_Destroy(header);
    }

    // Executor provided by the application is owned by |test_callback|.
    Cronet_ExecutorPtr executor = test_callback->GetExecutor();
    // Callback provided by the application.
    Cronet_UrlRequestCallbackPtr callback =
        test_callback->CreateUrlRequestCallback();

    TestRequestFinishedInfoListener test_request_finished_info_listener;
    MaybeAddRequestFinishedListener(request_params, engine, executor,
                                    &test_request_finished_info_listener);

    Cronet_UrlRequest_InitWithParams(request, engine, url.c_str(),
                                     request_params, callback, executor);

    Cronet_UrlRequest_Start(request);
    test_callback->WaitForDone();
    MaybeVerifyRequestFinishedInfo(&test_request_finished_info_listener,
                                   *test_callback);
    CleanupRequestFinishedListener(request_params, engine);
    // Wait for all posted tasks to be executed to ensure there is no unhandled
    // exception.
    test_callback->ShutdownExecutor();
    EXPECT_TRUE(test_callback->IsDone());
    EXPECT_TRUE(Cronet_UrlRequest_IsDone(request));
    if (upload_data_provider != nullptr)
      Cronet_UploadDataProvider_Destroy(upload_data_provider);
    Cronet_UrlRequestParams_Destroy(request_params);
    Cronet_UrlRequest_Destroy(request);
    Cronet_UrlRequestCallback_Destroy(callback);
    Cronet_Engine_Destroy(engine);
    return test_callback;
  }

  std::unique_ptr<TestUrlRequestCallback> StartAndWaitForComplete(
      const std::string& url,
      std::unique_ptr<TestUrlRequestCallback> test_callback,
      const std::string& http_method,
      TestUploadDataProvider* test_upload_data_provider) {
    return StartAndWaitForComplete(url, std::move(test_callback), http_method,
                                   test_upload_data_provider,
                                   /* remapped_port = */ 0);
  }

  std::unique_ptr<TestUrlRequestCallback> StartAndWaitForComplete(
      const std::string& url,
      std::unique_ptr<TestUrlRequestCallback> test_callback) {
    return StartAndWaitForComplete(url, std::move(test_callback),
                                   /* http_method =  */ std::string(),
                                   /* upload_data_provider =  */ nullptr);
  }

  std::unique_ptr<TestUrlRequestCallback> StartAndWaitForComplete(
      const std::string& url) {
    return StartAndWaitForComplete(
        url,
        std::make_unique<TestUrlRequestCallback>(GetDirectExecutorParam()));
  }

  void CheckResponseInfo(
      const TestUrlRequestCallback::UrlResponseInfo& response_info,
      const std::string& expected_url,
      int expected_http_status_code,
      const std::string& expected_http_status_text) {
    EXPECT_EQ(expected_url, response_info.url);
    EXPECT_EQ(expected_url, response_info.url_chain.back());
    EXPECT_EQ(expected_http_status_code, response_info.http_status_code);
    EXPECT_EQ(expected_http_status_text, response_info.http_status_text);
    EXPECT_FALSE(response_info.was_cached);
  }

  void ExpectResponseInfoEquals(
      const TestUrlRequestCallback::UrlResponseInfo& expected,
      const TestUrlRequestCallback::UrlResponseInfo& actual) {
    EXPECT_EQ(expected.url, actual.url);
    EXPECT_EQ(expected.url_chain, actual.url_chain);
    EXPECT_EQ(expected.http_status_code, actual.http_status_code);
    EXPECT_EQ(expected.http_status_text, actual.http_status_text);
    EXPECT_EQ(expected.all_headers, actual.all_headers);
    EXPECT_EQ(expected.was_cached, actual.was_cached);
    EXPECT_EQ(expected.negotiated_protocol, actual.negotiated_protocol);
    EXPECT_EQ(expected.proxy_server, actual.proxy_server);
    EXPECT_EQ(expected.received_byte_count, actual.received_byte_count);
  }

  // Depending on the test parameterization, adds a RequestFinishedInfoListener
  // to the Engine or UrlRequest, or does nothing.
  //
  // This method should be called before the call to
  // Cronet_UrlRequest_InitWithParams().
  void MaybeAddRequestFinishedListener(
      Cronet_UrlRequestParamsPtr url_request_params,
      Cronet_EnginePtr engine,
      Cronet_ExecutorPtr executor,
      TestRequestFinishedInfoListener* test_request_finished_info_listener) {
    auto request_finished_listener_type = GetRequestFinishedListenerTypeParam();

    if (request_finished_listener_type ==
        RequestFinishedListenerType::kNoListener)
      return;

    request_finished_listener_ =
        test_request_finished_info_listener->CreateRequestFinishedListener();

    switch (request_finished_listener_type) {
      case RequestFinishedListenerType::kUrlRequestListener:
        Cronet_UrlRequestParams_request_finished_listener_set(
            url_request_params, request_finished_listener_);
        Cronet_UrlRequestParams_request_finished_executor_set(
            url_request_params, executor);
        break;
      case RequestFinishedListenerType::kEngineListener:
        Cronet_Engine_AddRequestFinishedListener(
            engine, request_finished_listener_, executor);
        break;
      default:
        NOTREACHED();
    }
  }

  // Cleans up any leftover resources from MaybeAddRequestFinishedListener().
  //
  // NOTE: It's only necessary to call this method if
  // MaybeAddRequestFinishedListener() is called multiple times in a test case
  // (like in a loop).
  void CleanupRequestFinishedListener(
      Cronet_UrlRequestParamsPtr url_request_params,
      Cronet_EnginePtr engine) {
    auto request_finished_listener_type = GetRequestFinishedListenerTypeParam();
    if (request_finished_listener_type ==
        RequestFinishedListenerType::kEngineListener) {
      Cronet_Engine_RemoveRequestFinishedListener(engine,
                                                  request_finished_listener_);
    }
    Cronet_UrlRequestParams_request_finished_listener_set(url_request_params,
                                                          nullptr);
    Cronet_UrlRequestParams_request_finished_executor_set(url_request_params,
                                                          nullptr);
  }

  // TestRequestFinishedInfoListener.WaitForDone() is called and checks are
  // performed only if a RequestFinishedInfoListener is registered.
  //
  // This method should be called after TestUrlRequestCallback.WaitForDone().
  void MaybeVerifyRequestFinishedInfo(
      TestRequestFinishedInfoListener* test_request_finished_info_listener,
      const TestUrlRequestCallback& callback) {
    if (GetRequestFinishedListenerTypeParam() ==
        RequestFinishedListenerType::kNoListener)
      return;
    test_request_finished_info_listener->WaitForDone();
    VerifyRequestFinishedInfoListener(test_request_finished_info_listener,
                                      callback);
  }

  void TestCancel(TestUrlRequestCallback::FailureType failure_type,
                  TestUrlRequestCallback::ResponseStep failure_step,
                  bool expect_response_info,
                  bool expect_error);

 protected:
  // Provide a task environment for use by TestExecutor instances. Do not
  // initialize the ThreadPool as this is done by the Cronet_Engine
  base::test::SingleThreadTaskEnvironment task_environment_;

  // Not owned, |request_finished_listener_| destroys itself when run. This
  // pointer is only needed to unregister the listener from the Engine in
  // CleanupRequestFinishedListener() and to allow tests that never run the
  // |request_finished_listener_| to be able to destroy it.
  Cronet_RequestFinishedInfoListenerPtr request_finished_listener_ = nullptr;
};

const bool kDirectExecutorEnabled[]{true, false};
INSTANTIATE_TEST_SUITE_P(
    NoRequestFinishedListener,
    UrlRequestTest,
    testing::Combine(
        testing::ValuesIn(kDirectExecutorEnabled),
        testing::Values(RequestFinishedListenerType::kNoListener)));
INSTANTIATE_TEST_SUITE_P(
    RequestFinishedListenerOnUrlRequest,
    UrlRequestTest,
    testing::Combine(
        testing::ValuesIn(kDirectExecutorEnabled),
        testing::Values(RequestFinishedListenerType::kUrlRequestListener)));
INSTANTIATE_TEST_SUITE_P(
    RequestFinishedListenerOnEngine,
    UrlRequestTest,
    testing::Combine(
        testing::ValuesIn(kDirectExecutorEnabled),
        testing::Values(RequestFinishedListenerType::kEngineListener)));

TEST_P(UrlRequestTest, SSLCertificateError) {
  net::EmbeddedTestServer ssl_server(net::EmbeddedTestServer::TYPE_HTTPS);
  ssl_server.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  ASSERT_TRUE(ssl_server.Start());

  const std::string url = ssl_server.GetURL("/").spec();
  TestUploadDataProvider data_provider(TestUploadDataProvider::SYNC,
                                       /* executor = */ nullptr);
  data_provider.AddRead("Test");
  auto callback =
      std::make_unique<TestUrlRequestCallback>(GetDirectExecutorParam());
  callback = StartAndWaitForComplete(url, std::move(callback), std::string(),
                                     &data_provider);
  data_provider.AssertClosed();
  EXPECT_EQ(4, data_provider.GetUploadedLength());
  EXPECT_EQ(0, data_provider.num_read_calls());
  EXPECT_EQ(0, data_provider.num_rewind_calls());
  EXPECT_EQ(nullptr, callback->response_info());
  EXPECT_EQ("", callback->response_as_string());
  EXPECT_EQ("net::ERR_CERT_INVALID", callback->last_error_message());
}

TEST_P(UrlRequestTest, SSLUpload) {
  net::EmbeddedTestServer ssl_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::test_server::RegisterDefaultHandlers(&ssl_server);
  ASSERT_TRUE(ssl_server.Start());

  constexpr char kUrl[] = "https://test.example.com/echoall";
  constexpr char kUploadString[] =
      "The quick brown fox jumps over the lazy dog.";
  TestUploadDataProvider data_provider(TestUploadDataProvider::SYNC,
                                       /* executor = */ nullptr);
  data_provider.AddRead(kUploadString);
  auto callback =
      std::make_unique<TestUrlRequestCallback>(GetDirectExecutorParam());
  callback = StartAndWaitForComplete(kUrl, std::move(callback), std::string(),
                                     &data_provider, ssl_server.port());
  data_provider.AssertClosed();
  EXPECT_NE(nullptr, callback->response_info());
  EXPECT_EQ("", callback->last_error_message());
  EXPECT_EQ(200, callback->response_info()->http_status_code);
  EXPECT_THAT(callback->response_as_string(), HasSubstr(kUploadString));
}

// Test where an upload fails without ever initializing the
// UploadDataStream, because it can't connect to the server.
TEST_P(UrlRequestTest, UploadFailsWithoutInitializingStream) {
  // The port for PTP will always refuse a TCP connection
  const std::string url = "http://127.0.0.1:319";
  TestUploadDataProvider data_provider(TestUploadDataProvider::SYNC,
                                       /* executor = */ nullptr);
  data_provider.AddRead("Test");
  auto callback =
      std::make_unique<TestUrlRequestCallback>(GetDirectExecutorParam());
  callback = StartAndWaitForComplete(url, std::move(callback), std::string(),
                                     &data_provider);
  data_provider.AssertClosed();
  EXPECT_EQ(0, data_provider.num_read_calls());
  EXPECT_EQ(0, data_provider.num_rewind_calls());
  EXPECT_EQ(nullptr, callback->response_info());
  EXPECT_EQ("", callback->response_as_string());
  EXPECT_TRUE(callback->on_error_called());
}

TEST_P(UrlRequestTest, FailedRequestHostNotFound) {
  Cronet_EnginePtr engine = cronet::test::CreateTestEngine(0);
  Cronet_UrlRequestPtr request = Cronet_UrlRequest_Create();
  Cronet_UrlRequestParamsPtr request_params = Cronet_UrlRequestParams_Create();
  std::string url = "https://notfound.example.com";

  TestUrlRequestCallback test_callback(GetDirectExecutorParam());
  // Executor provided by the application is owned by |test_callback|.
  Cronet_ExecutorPtr executor = test_callback.GetExecutor();
  // Callback provided by the application.
  Cronet_UrlRequestCallbackPtr callback =
      test_callback.CreateUrlRequestCallback();
  TestRequestFinishedInfoListener test_request_finished_info_listener;
  MaybeAddRequestFinishedListener(request_params, engine, executor,
                                  &test_request_finished_info_listener);

  Cronet_UrlRequest_InitWithParams(request, engine, url.c_str(), request_params,
                                   callback, executor);

  Cronet_UrlRequest_Start(request);

  test_callback.WaitForDone();
  MaybeVerifyRequestFinishedInfo(&test_request_finished_info_listener,
                                 test_callback);
  EXPECT_TRUE(test_callback.IsDone());
  EXPECT_TRUE(test_callback.on_error_called());
  EXPECT_FALSE(test_callback.on_canceled_called());

  EXPECT_TRUE(test_callback.response_as_string().empty());
  EXPECT_EQ(nullptr, test_callback.response_info());
  EXPECT_NE(nullptr, test_callback.last_error());

  EXPECT_EQ(Cronet_Error_ERROR_CODE_ERROR_HOSTNAME_NOT_RESOLVED,
            Cronet_Error_error_code_get(test_callback.last_error()));
  EXPECT_FALSE(
      Cronet_Error_immediately_retryable_get(test_callback.last_error()));
  EXPECT_STREQ("net::ERR_NAME_NOT_RESOLVED",
               Cronet_Error_message_get(test_callback.last_error()));
  EXPECT_EQ(-105,
            Cronet_Error_internal_error_code_get(test_callback.last_error()));
  EXPECT_EQ(
      0, Cronet_Error_quic_detailed_error_code_get(test_callback.last_error()));

  Cronet_UrlRequestParams_Destroy(request_params);
  Cronet_UrlRequest_Destroy(request);
  Cronet_UrlRequestCallback_Destroy(callback);
  Cronet_Engine_Destroy(engine);
}

class UrlRequestTestNoParam : public ::testing::Test {
  void SetUp() override {}

  void TearDown() override {}
};

TEST_F(UrlRequestTestNoParam,
       RequestFinishedListenerWithoutExecutorReturnsError) {
  Cronet_EngineParamsPtr engine_params = Cronet_EngineParams_Create();
  Cronet_EnginePtr engine = Cronet_Engine_Create();
  // Disable runtime CHECK of the result, so it could be verified.
  Cronet_EngineParams_enable_check_result_set(engine_params, false);
  EXPECT_EQ(Cronet_RESULT_SUCCESS,
            Cronet_Engine_StartWithParams(engine, engine_params));
  Cronet_EngineParams_Destroy(engine_params);
  Cronet_UrlRequestPtr request = Cronet_UrlRequest_Create();
  Cronet_UrlRequestParamsPtr request_params = Cronet_UrlRequestParams_Create();
  TestRequestFinishedInfoListener test_request_finished_info_listener;
  Cronet_RequestFinishedInfoListenerPtr request_finished_listener =
      test_request_finished_info_listener.CreateRequestFinishedListener();
  // Executor type doesn't matter for this test.
  TestUrlRequestCallback test_callback(/*direct_executor=*/true);
  // Executor provided by the application is owned by |test_callback|.
  Cronet_ExecutorPtr executor = test_callback.GetExecutor();
  // Callback provided by the application.
  Cronet_UrlRequestCallbackPtr callback =
      test_callback.CreateUrlRequestCallback();

  Cronet_UrlRequestParams_request_finished_listener_set(
      request_params, request_finished_listener);

  EXPECT_EQ(Cronet_RESULT_NULL_POINTER_REQUEST_FINISHED_INFO_LISTENER_EXECUTOR,
            Cronet_UrlRequest_InitWithParams(
                request, engine, "http://fakeurl.example.com", request_params,
                callback, executor));

  // This test never actually runs |request_finished_listener|, so we delete
  // it here.
  Cronet_RequestFinishedInfoListener_Destroy(request_finished_listener);
  Cronet_UrlRequestParams_Destroy(request_params);
  Cronet_UrlRequest_Destroy(request);
  Cronet_UrlRequestCallback_Destroy(callback);
  Cronet_Engine_Destroy(engine);
}

TEST_F(UrlRequestTestNoParam,
       UseRequestFinishedInfoAfterUrlRequestDestructionFailure) {
  Cronet_EnginePtr engine = cronet::test::CreateTestEngine(0);
  Cronet_UrlRequestPtr request = Cronet_UrlRequest_Create();
  Cronet_UrlRequestParamsPtr request_params = Cronet_UrlRequestParams_Create();
  std::string url = "https://notfound.example.com";

  // The UrlRequest executor type doesn't matter, but the
  // RequestFinishedInfoListener executor type can't be direct.
  TestUrlRequestCallback test_callback(/* direct_executor= */ false);
  // Executor provided by the application is owned by |test_callback|.
  Cronet_ExecutorPtr executor = test_callback.GetExecutor();
  // Callback provided by the application.
  Cronet_UrlRequestCallbackPtr callback =
      test_callback.CreateUrlRequestCallback();

  base::WaitableEvent done_event;
  struct ListenerContext {
    raw_ptr<TestUrlRequestCallback> test_callback;
    Cronet_UrlRequestPtr url_request;
    raw_ptr<base::WaitableEvent> done_event;
  };
  ListenerContext listener_context = {&test_callback, request, &done_event};

  auto* request_finished_listener =
      Cronet_RequestFinishedInfoListener_CreateWith(
          +[](Cronet_RequestFinishedInfoListenerPtr self,
              Cronet_RequestFinishedInfoPtr request_finished_info,
              Cronet_UrlResponseInfoPtr response_info, Cronet_ErrorPtr error) {
            auto* listener_context = static_cast<ListenerContext*>(
                Cronet_RequestFinishedInfoListener_GetClientContext(self));
            listener_context->test_callback->WaitForDone();
            Cronet_UrlRequest_Destroy(listener_context->url_request);
            // The next few get methods shouldn't use-after-free on
            // |request_finished_info| or |error|.
            EXPECT_NE(nullptr, Cronet_RequestFinishedInfo_metrics_get(
                                   request_finished_info));
            EXPECT_NE(nullptr, Cronet_Error_message_get(error));
            Cronet_RequestFinishedInfoListener_Destroy(self);
            listener_context->done_event->Signal();
          });
  Cronet_RequestFinishedInfoListener_SetClientContext(request_finished_listener,
                                                      &listener_context);

  Cronet_UrlRequestParams_request_finished_listener_set(
      request_params, request_finished_listener);
  Cronet_UrlRequestParams_request_finished_executor_set(request_params,
                                                        executor);
  Cronet_UrlRequest_InitWithParams(request, engine, url.c_str(), request_params,
                                   callback, executor);
  Cronet_UrlRequest_Start(request);

  done_event.Wait();
  EXPECT_TRUE(test_callback.IsDone());

  Cronet_UrlRequestParams_Destroy(request_params);
  Cronet_UrlRequestCallback_Destroy(callback);
  Cronet_Engine_Destroy(engine);
}

}  // namespace
