// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/cloud_content_scanning/files_request_handler_base.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_service.h"
#include "components/enterprise/connectors/core/content_analysis_info_base.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/enterprise/connectors/core/reporting_event_router.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

// Mock implementation of the FilesRequestHandlerBase::Delegate.
// Note: All pure virtual methods must be mocked for the class to be
// instantiable, even if they are not called in every test.
class MockFilesRequestHandlerBaseDelegate
    : public FilesRequestHandlerBase::Delegate {
 public:
  MOCK_METHOD(std::unique_ptr<FileAnalysisRequestBase>,
              CreateFileRequest,
              (size_t index,
               const AnalysisSettings& settings,
               base::OnceCallback<void(ScanRequestUploadResult,
                                       ContentAnalysisResponse)> callback,
               base::OnceCallback<void(const BinaryUploadRequest&)>
                   request_start_callback),
              (override));
  MOCK_METHOD(void,
              ReportWarningBypass,
              (std::optional<std::u16string> user_justification,
               const ContentAnalysisInfoBase& info,
               const std::string& trigger,
               const std::string& content_transfer_method),
              (override));
  MOCK_METHOD(bool, UploadDataImpl, (), (override));
  MOCK_METHOD(size_t, GetFileCount, (), (const, override));
  MOCK_METHOD(void,
              UpdateRequestHandlerResult,
              (size_t index,
               RequestHandlerResult result,
               ContentAnalysisResponse response),
              (override));
  MOCK_METHOD(const base::FilePath&,
              GetPath,
              (size_t index),
              (const, override));
  MOCK_METHOD(const FilesRequestHandlerBase::FileInfo&,
              GetFileInfo,
              (size_t index),
              (override));
  MOCK_METHOD(FilesRequestHandlerBase::FileInfo&,
              GetMutableFileInfo,
              (size_t index),
              (override));
  MOCK_METHOD(void, SetFileScanStartTime, (size_t index), (override));
  MOCK_METHOD(const base::TimeTicks,
              GetFileScanStartTime,
              (size_t index),
              (override));
  MOCK_METHOD(ReportingEventRouter*, GetReportingEventRouter, (), (override));
  MOCK_METHOD(void, MaybeCompleteScanRequest, (), (override));
  MOCK_METHOD(std::string, GetSource, (), (override));
  MOCK_METHOD(std::string, GetDestination, (), (override));
  MOCK_METHOD(void,
              SetHandler,
              (FilesRequestHandlerBase * handler),
              (override));
  MOCK_METHOD(void, MaybeCancelAndReport, (), (override));
  MOCK_METHOD(void, MarkFileAsReported, (size_t index), (override));
};

// Mock implementation of the BinaryUploadService.
class MockBinaryUploadService : public BinaryUploadService {
 public:
  MOCK_METHOD(void,
              MaybeUploadForDeepScanning,
              (std::unique_ptr<BinaryUploadRequest> request),
              (override));
  MOCK_METHOD(void,
              MaybeAcknowledge,
              (std::unique_ptr<BinaryUploadAck> ack),
              (override));
  MOCK_METHOD(void,
              MaybeCancelRequests,
              (std::unique_ptr<BinaryUploadCancelRequests> cancel),
              (override));
  base::WeakPtr<BinaryUploadService> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockBinaryUploadService> weak_ptr_factory_{this};
};

// Mock implementation of the ContentAnalysisInfoBase.
class MockContentAnalysisInfoBase : public ContentAnalysisInfoBase {
 public:
  MOCK_METHOD(void,
              InitializeRequest,
              (BinaryUploadRequest * request,
               bool include_enterprise_only_fields),
              (override));
  MOCK_METHOD(const AnalysisSettings&, settings, (), (const, override));
  MOCK_METHOD(signin::IdentityManager*,
              identity_manager,
              (),
              (const, override));
  MOCK_METHOD(int, user_action_requests_count, (), (const, override));
  MOCK_METHOD(std::string, tab_title, (), (const, override));
  MOCK_METHOD(std::string, user_action_id, (), (const, override));
  MOCK_METHOD(std::string, email, (), (const, override));
  MOCK_METHOD(const GURL&, url, (), (const, override));
  MOCK_METHOD(const GURL&, tab_url, (), (const, override));
  MOCK_METHOD(ContentAnalysisRequest::Reason, reason, (), (const, override));
  MOCK_METHOD(
      (google::protobuf::RepeatedPtrField<::safe_browsing::ReferrerChainEntry>),
      referrer_chain,
      (),
      (const, override));
  MOCK_METHOD(google::protobuf::RepeatedPtrField<std::string>,
              frame_url_chain,
              (),
              (const, override));
  MOCK_METHOD(std::string, GetContentAreaAccountEmail, (), (const, override));
};

// A fake BinaryUploadRequest for testing.
class FakeBinaryUploadRequest : public BinaryUploadRequest {
 public:
  FakeBinaryUploadRequest(ContentAnalysisCallback callback,
                          CloudOrLocalAnalysisSettings settings)
      : BinaryUploadRequest(std::move(callback),
                            std::move(settings),
                            base::NullCallback()) {}
  void GetRequestData(DataCallback callback) override {
    std::move(callback).Run(ScanRequestUploadResult::kSuccess, Data());
  }
};

}  // namespace

class FilesRequestHandlerBaseTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  MockContentAnalysisInfoBase content_analysis_info_;
  MockBinaryUploadService upload_service_;
  AnalysisSettings settings_;
  GURL url_{"https://example.com"};
  base::FilePath path_;
};

// Tests that ReportWarningBypass correctly calls the delegate.
TEST_F(FilesRequestHandlerBaseTest, ReportWarningBypass) {
  auto delegate_ptr = std::make_unique<MockFilesRequestHandlerBaseDelegate>();
  auto* delegate = delegate_ptr.get();

  EXPECT_CALL(
      *delegate,
      ReportWarningBypass(testing::Optional(std::u16string(u"justification")),
                          testing::_, testing::_, testing::_))
      .Times(1);
  EXPECT_CALL(*delegate, SetHandler(testing::_)).Times(1);

  FilesRequestHandlerBase handler(&content_analysis_info_, &upload_service_,
                                  url_, "content_transfer_method",
                                  DeepScanAccessPoint::UPLOAD,
                                  std::move(delegate_ptr));
  handler.ReportWarningBypass(u"justification");
}

// Tests that UploadData correctly triggers the delegate's UploadDataImpl.
TEST_F(FilesRequestHandlerBaseTest, UploadDataImpl) {
  auto delegate_ptr = std::make_unique<MockFilesRequestHandlerBaseDelegate>();
  auto* delegate = delegate_ptr.get();

  EXPECT_CALL(*delegate, UploadDataImpl())
      .Times(1)
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*delegate, SetHandler(testing::_)).Times(1);

  FilesRequestHandlerBase handler(&content_analysis_info_, &upload_service_,
                                  url_, "content_transfer_method",
                                  DeepScanAccessPoint::UPLOAD,
                                  std::move(delegate_ptr));
  EXPECT_TRUE(handler.UploadData());
}

// Tests that OnGotFileInfo correctly initiates a deep scan upload on success.
TEST_F(FilesRequestHandlerBaseTest, OnGotFileInfo_Success) {
  auto delegate_ptr = std::make_unique<MockFilesRequestHandlerBaseDelegate>();
  auto* delegate = delegate_ptr.get();
  base::FilePath path;

  EXPECT_CALL(*delegate, GetFileCount()).WillRepeatedly(testing::Return(1));
  EXPECT_CALL(*delegate, SetHandler(testing::_)).Times(1);
  EXPECT_CALL(*delegate, GetPath(testing::_))
      .WillRepeatedly(testing::ReturnRef(path));
  FilesRequestHandlerBase handler(&content_analysis_info_, &upload_service_,
                                  url_, "content_transfer_method",
                                  DeepScanAccessPoint::UPLOAD,
                                  std::move(delegate_ptr));

  FilesRequestHandlerBase::FileInfo file_info;
  EXPECT_CALL(*delegate, GetMutableFileInfo(0))
      .WillRepeatedly(testing::ReturnRef(file_info));
  EXPECT_CALL(content_analysis_info_, settings())
      .WillRepeatedly(testing::ReturnRef(settings_));
  EXPECT_CALL(upload_service_, MaybeUploadForDeepScanning(testing::_)).Times(1);

  auto request = std::make_unique<FakeBinaryUploadRequest>(
      base::DoNothing(), CloudOrLocalAnalysisSettings(CloudAnalysisSettings()));
  BinaryUploadRequest::Data data;
  data.size = 100;
  handler.OnGotFileInfo(std::move(request), 0,
                        ScanRequestUploadResult::kSuccess, std::move(data));
}

// Tests that OnGotFileInfo finishes the request early if the file is empty.
TEST_F(FilesRequestHandlerBaseTest, OnGotFileInfo_EmptyFile) {
  auto delegate_ptr = std::make_unique<MockFilesRequestHandlerBaseDelegate>();
  auto* delegate = delegate_ptr.get();

  EXPECT_CALL(*delegate, GetFileCount()).WillRepeatedly(testing::Return(1));
  EXPECT_CALL(*delegate, SetHandler(testing::_)).Times(1);
  FilesRequestHandlerBase handler(&content_analysis_info_, &upload_service_,
                                  url_, "content_transfer_method",
                                  DeepScanAccessPoint::UPLOAD,
                                  std::move(delegate_ptr));

  FilesRequestHandlerBase::FileInfo file_info;
  EXPECT_CALL(*delegate, GetMutableFileInfo(0))
      .WillRepeatedly(testing::ReturnRef(file_info));
  EXPECT_CALL(content_analysis_info_, settings())
      .WillRepeatedly(testing::ReturnRef(settings_));
  EXPECT_CALL(upload_service_, MaybeUploadForDeepScanning(testing::_)).Times(0);

  base::RunLoop run_loop;
  bool callback_called = false;
  auto request = std::make_unique<FakeBinaryUploadRequest>(
      base::BindLambdaForTesting(
          [&callback_called, &run_loop](ScanRequestUploadResult result,
                                        ContentAnalysisResponse response) {
            EXPECT_EQ(result, ScanRequestUploadResult::kSuccess);
            callback_called = true;
            run_loop.Quit();
          }),
      CloudOrLocalAnalysisSettings(CloudAnalysisSettings()));

  BinaryUploadRequest::Data data;
  data.size = 0;
  handler.OnGotFileInfo(std::move(request), 0,
                        ScanRequestUploadResult::kSuccess, std::move(data));
  run_loop.Run();
  EXPECT_TRUE(callback_called);
}

// Tests that OnGotFileInfo finishes the request early if there's an upload
// failure.
TEST_F(FilesRequestHandlerBaseTest, OnGotFileInfo_Failure) {
  auto delegate_ptr = std::make_unique<MockFilesRequestHandlerBaseDelegate>();
  auto* delegate = delegate_ptr.get();

  EXPECT_CALL(*delegate, GetFileCount()).WillRepeatedly(testing::Return(1));
  EXPECT_CALL(*delegate, SetHandler(testing::_)).Times(1);
  FilesRequestHandlerBase handler(&content_analysis_info_, &upload_service_,
                                  url_, "content_transfer_method",
                                  DeepScanAccessPoint::UPLOAD,
                                  ::std::move(delegate_ptr));

  FilesRequestHandlerBase::FileInfo file_info;
  EXPECT_CALL(*delegate, GetMutableFileInfo(0))
      .WillRepeatedly(testing::ReturnRef(file_info));
  EXPECT_CALL(content_analysis_info_, settings())
      .WillRepeatedly(testing::ReturnRef(settings_));
  EXPECT_CALL(upload_service_, MaybeUploadForDeepScanning(testing::_)).Times(0);

  base::RunLoop run_loop;
  bool callback_called = false;
  auto request = std::make_unique<FakeBinaryUploadRequest>(
      base::BindLambdaForTesting(
          [&callback_called, &run_loop](ScanRequestUploadResult result,
                                        ContentAnalysisResponse response) {
            EXPECT_EQ(result, ScanRequestUploadResult::kFileTooLarge);
            callback_called = true;
            run_loop.Quit();
          }),
      CloudOrLocalAnalysisSettings(CloudAnalysisSettings()));

  BinaryUploadRequest::Data data;
  data.size = 100;
  handler.OnGotFileInfo(std::move(request), 0,
                        ScanRequestUploadResult::kFileTooLarge,
                        std::move(data));
  run_loop.Run();
  EXPECT_TRUE(callback_called);
}

// Tests that FileRequestCallback calls the delegate methods and reports events.
TEST_F(FilesRequestHandlerBaseTest, FileRequestCallback) {
  auto delegate_ptr = std::make_unique<MockFilesRequestHandlerBaseDelegate>();
  auto* delegate = delegate_ptr.get();

  FilesRequestHandlerBase::FileInfo file_info;
  // GetFileCount must return > 0 to prevent index out of bounds in
  // file_reported_ vector.
  EXPECT_CALL(*delegate, GetFileCount()).WillRepeatedly(testing::Return(1));
  EXPECT_CALL(content_analysis_info_, GetContentAreaAccountEmail())
      .WillRepeatedly(testing::Return(""));
  EXPECT_CALL(content_analysis_info_, settings())
      .WillRepeatedly(testing::ReturnRef(settings_));
  EXPECT_CALL(*delegate, UpdateRequestHandlerResult(0, testing::_, testing::_))
      .Times(2);
  EXPECT_CALL(*delegate, GetFileInfo(0))
      .WillRepeatedly(testing::ReturnRef(file_info));
  EXPECT_CALL(*delegate, GetPath(0)).WillRepeatedly(testing::ReturnRef(path_));
  EXPECT_CALL(*delegate, GetReportingEventRouter()).Times(2);
  EXPECT_CALL(*delegate, GetSource()).Times(2);
  EXPECT_CALL(*delegate, GetDestination()).Times(2);
  EXPECT_CALL(*delegate, MaybeCompleteScanRequest()).Times(2);
  EXPECT_CALL(*delegate, SetHandler(testing::_)).Times(1);

  FilesRequestHandlerBase handler(&content_analysis_info_, &upload_service_,
                                  url_, "content_transfer_method",
                                  DeepScanAccessPoint::UPLOAD,
                                  std::move(delegate_ptr));

  EXPECT_EQ(handler.file_result_count_, 0u);
  EXPECT_FALSE(handler.throttled_);

  ContentAnalysisResponse response;
  handler.FileRequestCallback(0, ScanRequestUploadResult::kSuccess, response);
  EXPECT_EQ(handler.file_result_count_, 1u);
  EXPECT_FALSE(handler.throttled_);

  handler.FileRequestCallback(0, ScanRequestUploadResult::kTooManyRequests,
                              response);
  EXPECT_EQ(handler.file_result_count_, 2u);
  EXPECT_TRUE(handler.throttled_);
}

// Tests that ReportCanceledFile correctly reports a canceled file when the
// feature is enabled.
TEST_F(FilesRequestHandlerBaseTest, ReportCanceledFile_FeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_connectors::kEnableCancelUploadOnContentAnalysis);

  auto delegate_ptr = std::make_unique<MockFilesRequestHandlerBaseDelegate>();
  auto* delegate = delegate_ptr.get();

  EXPECT_CALL(*delegate, SetHandler(testing::_)).Times(1);
  FilesRequestHandlerBase::FileInfo file_info;
  file_info.size = 100;
  file_info.mime_type = "text/plain";
  file_info.sha256_or_cb = "hash";

  EXPECT_CALL(*delegate, GetFileInfo(0))
      .WillOnce(testing::ReturnRef(file_info));
  EXPECT_CALL(*delegate, GetPath(0)).WillOnce(testing::ReturnRef(path_));
  EXPECT_CALL(*delegate, GetSource()).WillOnce(testing::Return("source"));
  EXPECT_CALL(*delegate, GetDestination()).WillOnce(testing::Return("dest"));
  EXPECT_CALL(*delegate, GetReportingEventRouter())
      .WillOnce(testing::Return(nullptr));
  EXPECT_CALL(content_analysis_info_, GetContentAreaAccountEmail())
      .WillOnce(testing::Return("email@example.com"));

  FilesRequestHandlerBase handler(&content_analysis_info_, &upload_service_,
                                  url_, "content_transfer_method",
                                  DeepScanAccessPoint::UPLOAD,
                                  std::move(delegate_ptr));

  handler.ReportCanceledFile(0);
}

// Tests that ReportCanceledFile does not report a canceled file when the
// feature is disabled.
TEST_F(FilesRequestHandlerBaseTest, ReportCanceledFile_FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      enterprise_connectors::kEnableCancelUploadOnContentAnalysis);

  auto delegate_ptr = std::make_unique<MockFilesRequestHandlerBaseDelegate>();
  auto* delegate = delegate_ptr.get();

  EXPECT_CALL(*delegate, SetHandler(testing::_)).Times(1);

  // None of the delegate reporting methods should be called.
  EXPECT_CALL(*delegate, GetFileInfo(0)).Times(0);
  EXPECT_CALL(*delegate, GetPath(0)).Times(0);
  EXPECT_CALL(*delegate, GetSource()).Times(0);
  EXPECT_CALL(*delegate, GetDestination()).Times(0);
  EXPECT_CALL(*delegate, GetReportingEventRouter()).Times(0);
  EXPECT_CALL(content_analysis_info_, GetContentAreaAccountEmail()).Times(0);

  FilesRequestHandlerBase handler(&content_analysis_info_, &upload_service_,
                                  url_, "content_transfer_method",
                                  DeepScanAccessPoint::UPLOAD,
                                  std::move(delegate_ptr));

  handler.ReportCanceledFile(0);
}

// Tests that destruction calls the delegate's MaybeCancelAndReport.
TEST_F(FilesRequestHandlerBaseTest, Destructor_ReportsCancellation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_connectors::kEnableCancelUploadOnContentAnalysis);

  auto delegate_ptr = std::make_unique<MockFilesRequestHandlerBaseDelegate>();
  auto* delegate = delegate_ptr.get();

  EXPECT_CALL(*delegate, SetHandler(testing::_)).Times(1);
  EXPECT_CALL(*delegate, GetFileCount()).WillRepeatedly(testing::Return(2));

  FilesRequestHandlerBase::FileInfo file_info;
  EXPECT_CALL(*delegate, GetFileInfo(0))
      .WillRepeatedly(testing::ReturnRef(file_info));
  EXPECT_CALL(*delegate, GetFileInfo(1))
      .WillRepeatedly(testing::ReturnRef(file_info));

  EXPECT_CALL(*delegate, GetPath(0)).WillRepeatedly(testing::ReturnRef(path_));
  EXPECT_CALL(*delegate, GetPath(1)).WillRepeatedly(testing::ReturnRef(path_));

  EXPECT_CALL(*delegate, GetSource()).WillRepeatedly(testing::Return("source"));
  EXPECT_CALL(*delegate, GetDestination())
      .WillRepeatedly(testing::Return("dest"));

  EXPECT_CALL(*delegate, GetReportingEventRouter())
      .WillRepeatedly(
          testing::Return(nullptr));  // Safely exits reporting function

  EXPECT_CALL(content_analysis_info_, settings())
      .WillRepeatedly(testing::ReturnRef(settings_));
  EXPECT_CALL(content_analysis_info_, GetContentAreaAccountEmail())
      .WillRepeatedly(testing::Return(""));

  EXPECT_CALL(*delegate, MaybeCancelAndReport()).Times(1);
  EXPECT_CALL(*delegate, MaybeCompleteScanRequest()).Times(1);

  {
    FilesRequestHandlerBase handler(&content_analysis_info_, &upload_service_,
                                    url_, "content_transfer_method",
                                    DeepScanAccessPoint::UPLOAD,
                                    std::move(delegate_ptr));

    // Simulate completion for the first file.
    ContentAnalysisResponse response;
    handler.FileRequestCallback(0, ScanRequestUploadResult::kSuccess, response);
  }
}

}  // namespace enterprise_connectors
