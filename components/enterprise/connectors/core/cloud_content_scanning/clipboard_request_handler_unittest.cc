// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/cloud_content_scanning/clipboard_request_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_service.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/clipboard_analysis_request.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/mock_content_analysis_info.h"
#include "components/enterprise/connectors/core/content_analysis_info_base.h"
#include "components/enterprise/connectors/core/reporting_event_router.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

constexpr char kTestToken[] = "test-token";
constexpr char kSourceUrl[] = "https://source.com";
constexpr char kUserEmail[] = "user@source.com";
constexpr char kPastedText[] = "pasted text data";
constexpr char kTestUrl[] = "https://example.com";

class TestClipboardRequestHandler : public ClipboardRequestHandler {
 public:
  TestClipboardRequestHandler(
      ContentAnalysisInfoBase* content_analysis_info,
      BinaryUploadService* upload_service,
      ReportingEventRouter* router,
      GURL url,
      Type type,
      DeepScanAccessPoint access_point,
      ContentMetaData::CopiedTextSource clipboard_source,
      std::string source_content_area_email,
      std::string content_transfer_method,
      std::string data,
      CompletionCallback callback,
      BinaryUploadRequest::BrowserPolicyConnectorGetter policy_getter)
      : ClipboardRequestHandler(content_analysis_info,
                                upload_service,
                                router,
                                std::move(url),
                                type,
                                access_point,
                                std::move(clipboard_source),
                                std::move(source_content_area_email),
                                std::move(content_transfer_method),
                                std::move(data),
                                std::move(callback),
                                std::move(policy_getter)) {}

  void UploadForDeepScanning(
      std::unique_ptr<ClipboardAnalysisRequest> request) override {
    analysis_request_ = std::move(request);
  }

  std::unique_ptr<ClipboardAnalysisRequest> analysis_request_;
};

class ClipboardRequestHandlerTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  MockContentAnalysisInfoBase content_analysis_info_;
  AnalysisSettings settings_;
};

}  // namespace

// Tests that a text paste request is correctly populated, triggers deep
// scanning, and completes with a compliant SUCCESS result upon receiving a
// successful scan verdict.
TEST_F(ClipboardRequestHandlerTest, TextPasteRequest) {
  GURL url(kTestUrl);
  ContentMetaData::CopiedTextSource clipboard_source;
  clipboard_source.set_context(ContentMetaData::CopiedTextSource::SAME_PROFILE);
  clipboard_source.set_url(kSourceUrl);

  base::test::TestFuture<RequestHandlerResult> future;

  auto handler = std::make_unique<TestClipboardRequestHandler>(
      &content_analysis_info_, /*upload_service=*/nullptr,
      /*router=*/nullptr, url, ClipboardRequestHandler::Type::kText,
      DeepScanAccessPoint::PASTE, clipboard_source,
      /*source_content_area_email=*/kUserEmail,
      /*content_transfer_method=*/"paste",
      /*data=*/kPastedText, future.GetCallback(),
      base::BindRepeating(
          []() -> policy::BrowserPolicyConnector* { return nullptr; }));

  EXPECT_CALL(content_analysis_info_, settings())
      .WillRepeatedly(testing::ReturnRef(settings_));
  EXPECT_CALL(content_analysis_info_, InitializeRequest(testing::_, testing::_))
      .Times(1);

  // Trigger the upload.
  EXPECT_TRUE(handler->UploadData());

  // Verify the request has correct properties.
  ASSERT_TRUE(handler->analysis_request_);
  auto* request = handler->analysis_request_.get();

  EXPECT_EQ(request->analysis_connector(), BULK_DATA_ENTRY);
  EXPECT_FALSE(request->image_paste());

  const auto& analysis_request = request->content_analysis_request();
  EXPECT_EQ(analysis_request.request_data().destination(), url.spec());
  EXPECT_EQ(analysis_request.request_data().source(), kSourceUrl);
  EXPECT_EQ(analysis_request.request_data().copied_text_source().context(),
            ContentMetaData::CopiedTextSource::SAME_PROFILE);
  EXPECT_EQ(analysis_request.request_data().copied_text_source().url(),
            kSourceUrl);
  EXPECT_EQ(analysis_request.request_data().source_content_area_account_email(),
            kUserEmail);

  // Simulate a successful scan response.
  ContentAnalysisResponse response;
  response.set_request_token(kTestToken);

  request->FinishRequest(ScanRequestUploadResult::kSuccess, response);

  EXPECT_TRUE(future.IsReady());
  RequestHandlerResult callback_result = future.Take();
  EXPECT_TRUE(callback_result.complies);
  EXPECT_EQ(callback_result.request_token, kTestToken);
  EXPECT_EQ(callback_result.final_result, FinalContentAnalysisResult::SUCCESS);
}

// Tests that an image paste request correctly sets the image_paste flag to
// true, does not populate source/destination metadata meant for text pastes,
// and completes successfully upon receiving a scan verdict.
TEST_F(ClipboardRequestHandlerTest, ImagePasteRequest) {
  GURL url(kTestUrl);
  ContentMetaData::CopiedTextSource clipboard_source;

  base::test::TestFuture<RequestHandlerResult> future;

  auto handler = std::make_unique<TestClipboardRequestHandler>(
      &content_analysis_info_, /*upload_service=*/nullptr,
      /*router=*/nullptr, url, ClipboardRequestHandler::Type::kImage,
      DeepScanAccessPoint::PASTE, clipboard_source,
      /*source_content_area_email=*/"",
      /*content_transfer_method=*/"paste",
      /*data=*/"image_data_bytes", future.GetCallback(),
      base::BindRepeating(
          []() -> policy::BrowserPolicyConnector* { return nullptr; }));

  EXPECT_CALL(content_analysis_info_, settings())
      .WillRepeatedly(testing::ReturnRef(settings_));
  EXPECT_CALL(content_analysis_info_, InitializeRequest(testing::_, testing::_))
      .Times(1);

  // Trigger the upload
  EXPECT_TRUE(handler->UploadData());

  // Verify the request was captured and has correct properties
  ASSERT_TRUE(handler->analysis_request_);
  auto* request = handler->analysis_request_.get();

  EXPECT_EQ(request->analysis_connector(), BULK_DATA_ENTRY);
  EXPECT_TRUE(request->image_paste());

  const auto& analysis_request = request->content_analysis_request();
  // Image paste shouldn't set destination/source on the request (only for text)
  EXPECT_FALSE(analysis_request.request_data().has_destination());
  EXPECT_FALSE(analysis_request.request_data().has_source());

  // Now simulate a successful scan response
  ContentAnalysisResponse response;
  response.set_request_token(kTestToken);

  request->FinishRequest(ScanRequestUploadResult::kSuccess, response);

  EXPECT_TRUE(future.IsReady());
  RequestHandlerResult callback_result = future.Take();
  EXPECT_TRUE(callback_result.complies);
  EXPECT_EQ(callback_result.request_token, kTestToken);
  EXPECT_EQ(callback_result.final_result, FinalContentAnalysisResult::SUCCESS);
}

// Tests that if the content analysis service returns a verdict with a BLOCK
// action, the request handler correctly reports a non-compliant FAILURE result
// to its callback.
TEST_F(ClipboardRequestHandlerTest, ScanResponseBlock) {
  GURL url(kTestUrl);
  ContentMetaData::CopiedTextSource clipboard_source;

  base::test::TestFuture<RequestHandlerResult> future;

  auto handler = std::make_unique<TestClipboardRequestHandler>(
      &content_analysis_info_, /*upload_service=*/nullptr,
      /*router=*/nullptr, url, ClipboardRequestHandler::Type::kText,
      DeepScanAccessPoint::PASTE, clipboard_source,
      /*source_content_area_email=*/"",
      /*content_transfer_method=*/"paste",
      /*data=*/kPastedText, future.GetCallback(),
      base::BindRepeating(
          []() -> policy::BrowserPolicyConnector* { return nullptr; }));

  EXPECT_CALL(content_analysis_info_, settings())
      .WillRepeatedly(testing::ReturnRef(settings_));
  EXPECT_CALL(content_analysis_info_, InitializeRequest(testing::_, testing::_))
      .Times(1);

  // Trigger the upload
  EXPECT_TRUE(handler->UploadData());

  ASSERT_TRUE(handler->analysis_request_);
  auto* request = handler->analysis_request_.get();

  // Simulate a block response
  ContentAnalysisResponse response;
  response.set_request_token(kTestToken);
  auto* result = response.add_results();
  result->set_status(ContentAnalysisResponse::Result::SUCCESS);
  result->set_tag("dlp");
  auto* rule = result->add_triggered_rules();
  rule->set_action(TriggeredRule::BLOCK);
  rule->set_rule_name("block-rule");

  request->FinishRequest(ScanRequestUploadResult::kSuccess, response);

  EXPECT_TRUE(future.IsReady());
  RequestHandlerResult callback_result = future.Take();
  EXPECT_FALSE(callback_result.complies);
  EXPECT_EQ(callback_result.request_token, kTestToken);
  EXPECT_EQ(callback_result.final_result, FinalContentAnalysisResult::FAILURE);
}

// Tests that if the content analysis service returns a verdict with a WARN
// action, the request handler correctly reports a non-compliant WARNING result
// to its callback.
TEST_F(ClipboardRequestHandlerTest, ScanResponseWarning) {
  GURL url(kTestUrl);
  ContentMetaData::CopiedTextSource clipboard_source;

  base::test::TestFuture<RequestHandlerResult> future;

  auto handler = std::make_unique<TestClipboardRequestHandler>(
      &content_analysis_info_, /*upload_service=*/nullptr,
      /*router=*/nullptr, url, ClipboardRequestHandler::Type::kText,
      DeepScanAccessPoint::PASTE, clipboard_source,
      /*source_content_area_email=*/"",
      /*content_transfer_method=*/"paste",
      /*data=*/kPastedText, future.GetCallback(),
      base::BindRepeating(
          []() -> policy::BrowserPolicyConnector* { return nullptr; }));

  EXPECT_CALL(content_analysis_info_, settings())
      .WillRepeatedly(testing::ReturnRef(settings_));
  EXPECT_CALL(content_analysis_info_, InitializeRequest(testing::_, testing::_))
      .Times(1);

  // Trigger the upload
  EXPECT_TRUE(handler->UploadData());

  ASSERT_TRUE(handler->analysis_request_);
  auto* request = handler->analysis_request_.get();

  // Simulate a warning response
  ContentAnalysisResponse response;
  response.set_request_token(kTestToken);
  auto* result = response.add_results();
  result->set_status(ContentAnalysisResponse::Result::SUCCESS);
  result->set_tag("dlp");
  auto* rule = result->add_triggered_rules();
  rule->set_action(TriggeredRule::WARN);
  rule->set_rule_name("warn-rule");

  request->FinishRequest(ScanRequestUploadResult::kSuccess, response);

  EXPECT_TRUE(future.IsReady());
  RequestHandlerResult callback_result = future.Take();
  EXPECT_FALSE(callback_result.complies);
  EXPECT_EQ(callback_result.request_token, kTestToken);
  EXPECT_EQ(callback_result.final_result, FinalContentAnalysisResult::WARNING);
}

// Tests that calling ReportWarningBypass does not crash and runs safely even
// with a null ReportingEventRouter.
TEST_F(ClipboardRequestHandlerTest, ReportWarningBypass) {
  GURL url(kTestUrl);
  ContentMetaData::CopiedTextSource clipboard_source;

  auto handler = std::make_unique<TestClipboardRequestHandler>(
      &content_analysis_info_, /*upload_service=*/nullptr,
      /*router=*/nullptr, url, ClipboardRequestHandler::Type::kText,
      DeepScanAccessPoint::PASTE, clipboard_source,
      /*source_content_area_email=*/"",
      /*content_transfer_method=*/"paste",
      /*data=*/kPastedText, base::DoNothing(),
      base::BindRepeating(
          []() -> policy::BrowserPolicyConnector* { return nullptr; }));

  // Call ReportWarningBypass and ensure it doesn't crash.
  handler->ReportWarningBypass(u"justification");
}

}  // namespace enterprise_connectors
