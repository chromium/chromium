// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/cloud_content_scanning/network_request_handler.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_request.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/mock_content_analysis_info.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/network_request_analysis_request.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/enterprise/connectors/core/reporting_event_router.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/mojom/chunked_data_pipe_getter.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace enterprise_connectors {

namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::ReturnRef;
using ::testing::StrictMock;

constexpr char kRequestUrl[] = "https://request.com/upload";
constexpr char kTabUrl[] = "https://tab.com/";
constexpr std::string_view kRequestBody = "sensitive=data";
constexpr char kRequestToken[] = "request-token";

constexpr char kDurationHistogram[] =
    "SafeBrowsing.DeepScan.NetworkRequest.Duration";
constexpr char kSuccessDurationHistogram[] =
    "SafeBrowsing.DeepScan.NetworkRequest.Success.Duration";
constexpr char kUploadFailureDurationHistogram[] =
    "SafeBrowsing.DeepScan.NetworkRequest.UploadFailure.Duration";
constexpr char kBytesPerSecondsHistogram[] =
    "SafeBrowsing.DeepScan.NetworkRequest.BytesPerSeconds";

// Captures the request that would be uploaded instead of sending it to a
// `BinaryUploadService`, so that tests can complete it with any verdict.
class TestNetworkRequestHandler : public NetworkRequestHandler {
 public:
  using NetworkRequestHandler::NetworkRequestHandler;

  void UploadForDeepScanning(
      std::unique_ptr<NetworkRequestAnalysisRequest> request) override {
    analysis_request_ = std::move(request);
  }

  NetworkRequestAnalysisRequest* analysis_request() {
    return analysis_request_.get();
  }

 private:
  std::unique_ptr<NetworkRequestAnalysisRequest> analysis_request_;
};

class MockReportingEventRouter : public ReportingEventRouter {
 public:
  MockReportingEventRouter()
      : ReportingEventRouter(/*reporting_client=*/nullptr) {}
  ~MockReportingEventRouter() override = default;

  MOCK_METHOD(void,
              OnSensitiveDataEvent,
              (const SensitiveDataEvent& event),
              (override));
};

scoped_refptr<network::ResourceRequestBody> CreateBytesBody() {
  return network::ResourceRequestBody::CreateFromCopyOfBytes(
      base::as_byte_span(kRequestBody));
}

BinaryUploadRequest::BrowserPolicyConnectorGetter NullPolicyConnectorGetter() {
  return base::BindRepeating(
      []() -> policy::BrowserPolicyConnector* { return nullptr; });
}

ContentAnalysisResponse CreateDlpResponse(
    std::optional<TriggeredRule::Action> triggered_rule_action) {
  ContentAnalysisResponse response;
  response.set_request_token(kRequestToken);

  auto* result = response.add_results();
  result->set_tag("dlp");
  result->set_status(ContentAnalysisResponse::Result::SUCCESS);
  if (triggered_rule_action) {
    auto* rule = result->add_triggered_rules();
    rule->set_action(*triggered_rule_action);
    rule->set_rule_name("rule_name");
    rule->set_rule_id("rule_id");
  }

  return response;
}

class NetworkRequestHandlerTest : public testing::Test {
 public:
  NetworkRequestHandlerTest() {
    settings_.cloud_or_local_settings =
        CloudOrLocalAnalysisSettings(CloudAnalysisSettings());
    settings_.tags = {{"dlp", TagSettings()}};
    settings_.block_until_verdict = BlockUntilVerdict::kNoBlock;
    settings_.default_action = DefaultAction::kAllow;

    ON_CALL(content_analysis_info_, settings())
        .WillByDefault(ReturnRef(settings_));
    ON_CALL(content_analysis_info_, url())
        .WillByDefault(ReturnRef(request_url_));
    ON_CALL(content_analysis_info_, tab_url())
        .WillByDefault(ReturnRef(tab_url_));
  }

  std::unique_ptr<TestNetworkRequestHandler> CreateHandler(
      ReportingEventRouter* router,
      scoped_refptr<network::ResourceRequestBody> body = CreateBytesBody()) {
    return std::make_unique<TestNetworkRequestHandler>(
        &content_analysis_info_, /*upload_service=*/nullptr, router,
        request_url_, std::move(body), future_.GetCallback(),
        NullPolicyConnectorGetter());
  }

  // Sets an expectation for a single sensitive data event to be reported to
  // `router`, and stores it in `event` once it's reported.
  void ExpectSensitiveDataEvent(
      MockReportingEventRouter& router,
      std::optional<ReportingEventRouter::SensitiveDataEvent>& event) {
    EXPECT_CALL(router, OnSensitiveDataEvent)
        .WillOnce([&event](const ReportingEventRouter::SensitiveDataEvent& e) {
          event.emplace(e);
        });
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  NiceMock<MockContentAnalysisInfoBase> content_analysis_info_;
  AnalysisSettings settings_;
  GURL request_url_{kRequestUrl};
  GURL tab_url_{kTabUrl};
  base::test::TestFuture<RequestHandlerResult> future_;
};

}  // namespace

TEST_F(NetworkRequestHandlerTest, UploadedRequest) {
  auto handler = CreateHandler(/*router=*/nullptr);

  EXPECT_CALL(content_analysis_info_,
              InitializeRequest(_, /*include_enterprise_only_fields=*/true))
      .Times(1);
  EXPECT_TRUE(handler->UploadData());

  NetworkRequestAnalysisRequest* request = handler->analysis_request();
  ASSERT_TRUE(request);
  EXPECT_EQ(request->analysis_connector(), AnalysisConnector::NETWORK_REQUEST);
  EXPECT_EQ(request->content_analysis_request().request_data().destination(),
            kRequestUrl);

  base::test::TestFuture<ScanRequestUploadResult, BinaryUploadRequest::Data>
      data_future;
  request->GetRequestData(data_future.GetCallback());
  EXPECT_EQ(data_future.Get<ScanRequestUploadResult>(),
            ScanRequestUploadResult::kSuccess);
  EXPECT_EQ(data_future.Get<BinaryUploadRequest::Data>().size,
            kRequestBody.size());

  // The handler doesn't complete until a verdict is received.
  EXPECT_FALSE(future_.IsReady());
}

TEST_F(NetworkRequestHandlerTest, AllowedVerdict) {
  base::HistogramTester histograms;
  StrictMock<MockReportingEventRouter> router;
  auto handler = CreateHandler(&router);

  ASSERT_TRUE(handler->UploadData());
  ASSERT_TRUE(handler->analysis_request());

  task_environment_.FastForwardBy(base::Seconds(1));

  // No event is reported when no rule is triggered.
  EXPECT_CALL(router, OnSensitiveDataEvent).Times(0);
  handler->analysis_request()->FinishRequest(
      ScanRequestUploadResult::kSuccess,
      CreateDlpResponse(/*triggered_rule_action=*/std::nullopt));

  ASSERT_TRUE(future_.IsReady());
  RequestHandlerResult result = future_.Take();
  EXPECT_TRUE(result.complies);
  EXPECT_EQ(result.final_result, FinalContentAnalysisResult::SUCCESS);
  EXPECT_EQ(result.request_token, kRequestToken);

  histograms.ExpectUniqueTimeSample(kDurationHistogram, base::Seconds(1), 1);
  histograms.ExpectUniqueTimeSample(kSuccessDurationHistogram, base::Seconds(1),
                                    1);
  histograms.ExpectUniqueSample(kBytesPerSecondsHistogram, kRequestBody.size(),
                                1);
}

TEST_F(NetworkRequestHandlerTest, DlpVerdictReported) {
  StrictMock<MockReportingEventRouter> router;
  auto handler = CreateHandler(&router);

  ASSERT_TRUE(handler->UploadData());
  ASSERT_TRUE(handler->analysis_request());

  std::optional<ReportingEventRouter::SensitiveDataEvent> event;
  ExpectSensitiveDataEvent(router, event);
  handler->analysis_request()->FinishRequest(
      ScanRequestUploadResult::kSuccess,
      CreateDlpResponse(TriggeredRule::REPORT_ONLY));

  ASSERT_TRUE(future_.IsReady());
  RequestHandlerResult result = future_.Take();
  EXPECT_TRUE(result.complies);
  EXPECT_EQ(result.final_result, FinalContentAnalysisResult::SUCCESS);
  EXPECT_EQ(result.request_token, kRequestToken);
  EXPECT_EQ(result.tag, "dlp");

  ASSERT_TRUE(event);
  EXPECT_EQ(event->url, GURL(kRequestUrl));
  EXPECT_EQ(event->tab_url, GURL(kTabUrl));
  EXPECT_EQ(event->source, "");
  EXPECT_EQ(event->destination, kRequestUrl);
  EXPECT_EQ(event->file_name, "");
  EXPECT_EQ(event->mime_type, "");
  EXPECT_EQ(event->trigger, kNetworkRequestDataTransferEventTrigger);
  EXPECT_EQ(event->scan_id, kRequestToken);
  EXPECT_EQ(event->content_transfer_method, "");
  EXPECT_EQ(event->content_size, static_cast<int64_t>(kRequestBody.size()));
  EXPECT_EQ(event->event_result, EventResult::ALLOWED);
  EXPECT_EQ(event->result.tag(), "dlp");
  ASSERT_EQ(event->result.triggered_rules_size(), 1);
  EXPECT_EQ(event->result.triggered_rules(0).action(),
            TriggeredRule::REPORT_ONLY);
}

// Since scans are audit-only, the network request is never blocked. Even if a
// blocking rule is triggered, the event should report that the request was
// allowed.
TEST_F(NetworkRequestHandlerTest, BlockVerdictReportedAsAllowed) {
  StrictMock<MockReportingEventRouter> router;
  auto handler = CreateHandler(&router);

  ASSERT_TRUE(handler->UploadData());
  ASSERT_TRUE(handler->analysis_request());

  std::optional<ReportingEventRouter::SensitiveDataEvent> event;
  ExpectSensitiveDataEvent(router, event);
  handler->analysis_request()->FinishRequest(
      ScanRequestUploadResult::kSuccess,
      CreateDlpResponse(TriggeredRule::BLOCK));

  ASSERT_TRUE(future_.IsReady());
  RequestHandlerResult result = future_.Take();
  EXPECT_FALSE(result.complies);
  EXPECT_EQ(result.final_result, FinalContentAnalysisResult::FAILURE);

  ASSERT_TRUE(event);
  EXPECT_EQ(event->destination, kRequestUrl);
  EXPECT_EQ(event->trigger, kNetworkRequestDataTransferEventTrigger);
  EXPECT_EQ(event->event_result, EventResult::ALLOWED);
  ASSERT_EQ(event->result.triggered_rules_size(), 1);
  EXPECT_EQ(event->result.triggered_rules(0).action(), TriggeredRule::BLOCK);
}

TEST_F(NetworkRequestHandlerTest, UploadFailure) {
  base::HistogramTester histograms;
  StrictMock<MockReportingEventRouter> router;
  auto handler = CreateHandler(&router);

  ASSERT_TRUE(handler->UploadData());
  ASSERT_TRUE(handler->analysis_request());

  task_environment_.FastForwardBy(base::Seconds(1));

  EXPECT_CALL(router, OnSensitiveDataEvent).Times(0);
  handler->analysis_request()->FinishRequest(
      ScanRequestUploadResult::kUploadFailure, ContentAnalysisResponse());

  // The default action is to allow, so the request complies even if it
  // couldn't be scanned.
  ASSERT_TRUE(future_.IsReady());
  RequestHandlerResult result = future_.Take();
  EXPECT_TRUE(result.complies);
  EXPECT_EQ(result.final_result, FinalContentAnalysisResult::SUCCESS);

  histograms.ExpectUniqueTimeSample(kDurationHistogram, base::Seconds(1), 1);
  histograms.ExpectUniqueTimeSample(kUploadFailureDurationHistogram,
                                    base::Seconds(1), 1);
  histograms.ExpectTotalCount(kBytesPerSecondsHistogram, 0);
}

// The size of a chunked body can't be computed ahead of time, so it should be
// reported as unknown instead of as an empty body.
TEST_F(NetworkRequestHandlerTest, UnknownBodySize) {
  mojo::PendingRemote<network::mojom::ChunkedDataPipeGetter> chunked_getter;
  std::ignore = chunked_getter.InitWithNewPipeAndPassReceiver();
  auto body = base::MakeRefCounted<network::ResourceRequestBody>();
  body->SetToChunkedDataPipe(std::move(chunked_getter),
                             network::ResourceRequestBody::ReadOnlyOnce(false));

  StrictMock<MockReportingEventRouter> router;
  auto handler = CreateHandler(&router, std::move(body));

  ASSERT_TRUE(handler->UploadData());
  ASSERT_TRUE(handler->analysis_request());

  std::optional<ReportingEventRouter::SensitiveDataEvent> event;
  ExpectSensitiveDataEvent(router, event);
  handler->analysis_request()->FinishRequest(
      ScanRequestUploadResult::kSuccess,
      CreateDlpResponse(TriggeredRule::REPORT_ONLY));

  ASSERT_TRUE(future_.IsReady());
  ASSERT_TRUE(event);
  EXPECT_EQ(event->content_size, -1);
}

// The size of a file body is computed asynchronously, and the request should
// only be uploaded once that size is known.
TEST_F(NetworkRequestHandlerTest, AsyncBodySize) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("body.txt");
  ASSERT_TRUE(base::WriteFile(file_path, kRequestBody));
  auto body = base::MakeRefCounted<network::ResourceRequestBody>();
  body->AppendFileRange(file_path, /*offset=*/0,
                        std::numeric_limits<uint64_t>::max(), base::Time());

  StrictMock<MockReportingEventRouter> router;
  auto handler = CreateHandler(&router, std::move(body));

  ASSERT_TRUE(handler->UploadData());
  EXPECT_FALSE(handler->analysis_request());
  ASSERT_TRUE(base::test::RunUntil(
      [&handler]() { return handler->analysis_request() != nullptr; }));

  std::optional<ReportingEventRouter::SensitiveDataEvent> event;
  ExpectSensitiveDataEvent(router, event);
  handler->analysis_request()->FinishRequest(
      ScanRequestUploadResult::kSuccess,
      CreateDlpResponse(TriggeredRule::REPORT_ONLY));

  ASSERT_TRUE(future_.IsReady());
  ASSERT_TRUE(event);
  EXPECT_EQ(event->content_size, static_cast<int64_t>(kRequestBody.size()));
}

// Completing a scan without a `ReportingEventRouter` should still run the
// callback, without reporting anything.
TEST_F(NetworkRequestHandlerTest, NullRouter) {
  auto handler = CreateHandler(/*router=*/nullptr);

  ASSERT_TRUE(handler->UploadData());
  ASSERT_TRUE(handler->analysis_request());

  handler->analysis_request()->FinishRequest(
      ScanRequestUploadResult::kSuccess,
      CreateDlpResponse(TriggeredRule::REPORT_ONLY));

  ASSERT_TRUE(future_.IsReady());
  RequestHandlerResult result = future_.Take();
  EXPECT_TRUE(result.complies);
  EXPECT_EQ(result.request_token, kRequestToken);
  EXPECT_EQ(result.tag, "dlp");
}

// Without a `BinaryUploadService`, the request can't be scanned. The handler
// should still complete asynchronously so that its owner isn't left waiting.
TEST_F(NetworkRequestHandlerTest, NoUploadService) {
  StrictMock<MockReportingEventRouter> router;
  NetworkRequestHandler handler(
      &content_analysis_info_, /*upload_service=*/nullptr, &router,
      request_url_, CreateBytesBody(), future_.GetCallback(),
      NullPolicyConnectorGetter());

  EXPECT_CALL(router, OnSensitiveDataEvent).Times(0);
  ASSERT_TRUE(handler.UploadData());
  EXPECT_FALSE(future_.IsReady());

  // The default action is to allow, so the request complies even if it
  // couldn't be scanned.
  RequestHandlerResult result = future_.Take();
  EXPECT_TRUE(result.complies);
  EXPECT_EQ(result.final_result, FinalContentAnalysisResult::SUCCESS);
}

// Only audit-only settings are supported for network requests.
TEST_F(NetworkRequestHandlerTest, BlockingSettingsNotSupported) {
  settings_.block_until_verdict = BlockUntilVerdict::kBlock;
  auto handler = CreateHandler(/*router=*/nullptr);

  EXPECT_CHECK_DEATH(handler->UploadData());
}

}  // namespace enterprise_connectors
