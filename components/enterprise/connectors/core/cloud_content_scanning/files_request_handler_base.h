// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_FILES_REQUEST_HANDLER_BASE_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_FILES_REQUEST_HANDLER_BASE_H_

#include "base/gtest_prod_util.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/file_analysis_request_base.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/request_handler_base.h"

namespace enterprise_connectors {

class BinaryUploadService;

// A base class for handling scanning and reporting of deep scanning requests
// for files.
class FilesRequestHandlerBase : public RequestHandlerBase {
 public:
  // Delegate interface to be implemented by child classes to handle the
  // specifics of different types of file requests.
  class Delegate {
   public:
    Delegate() = default;
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;
    virtual ~Delegate() = default;

    // Prepares an upload request for the file at `index`. If the file
    // cannot be uploaded, a failure verdict is added to the scanning
    // result.
    virtual enterprise_connectors::FileAnalysisRequestBase* PrepareFileRequest(
        size_t index) = 0;

    // Called when a user bypasses a scanning warning. The delegate is
    // responsible for sending any necessary reports related to this bypass.
    virtual void ReportWarningBypass(
        std::optional<std::u16string> user_justification) = 0;

    // Implements the actual data upload. Returns true if the upload is
    // happening asynchronously in the background, or false if there is no data
    // to upload.
    virtual bool UploadDataImpl() = 0;

    // Update the file_info for a given `index`. Unlike desktop and android, on
    // ios, user can't download a folder containing multiple items. So we'll
    // have platform-dependent implementations for updating the file_info.
    virtual void UpdateFileInfo(size_t index,
                                BinaryUploadRequest::Data data) = 0;
  };

  // `content_analysis_info` and `upload_service` are used to manage the deep
  // scanning request.
  // `url` is the target URL of the file transfer.
  // `access_point` indicates where the deep scanning request was triggered.
  // `delegate` is used to handle specific file request logic.
  explicit FilesRequestHandlerBase(
      ContentAnalysisInfoBase* content_analysis_info,
      BinaryUploadService* upload_service,
      GURL url,
      DeepScanAccessPoint access_point,
      std::unique_ptr<FilesRequestHandlerBase::Delegate> delegate);

  FilesRequestHandlerBase(const FilesRequestHandlerBase&) = delete;
  FilesRequestHandlerBase& operator=(const FilesRequestHandlerBase&) = delete;

  ~FilesRequestHandlerBase() override;

  // This should only call the delegate_->ReportWarningBypass.
  void ReportWarningBypass(
      std::optional<std::u16string> user_justification) override;

 protected:
  // This should only call the delegate_->UploadDataImpl().
  bool UploadDataImpl() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(FilesRequestHandlerBaseTest, OnGotFileInfo_Success);
  FRIEND_TEST_ALL_PREFIXES(FilesRequestHandlerBaseTest,
                           OnGotFileInfo_EmptyFile);
  FRIEND_TEST_ALL_PREFIXES(FilesRequestHandlerBaseTest, OnGotFileInfo_Failure);

  // Called when the file info for `path` has been fetched. Also begins the
  // upload process.
  void OnGotFileInfo(std::unique_ptr<BinaryUploadRequest> request,
                     size_t index,
                     ScanRequestUploadResult result,
                     BinaryUploadRequest::Data data);

  // Called when a request is finished early without uploading it.
  // This is, e.g., called for encrypted files and responsible for posting the
  // required data to safe-browsing ui.
  void FinishRequestEarly(std::unique_ptr<BinaryUploadRequest> request,
                          ScanRequestUploadResult result);

  // Upload the request for deep scanning using the binary upload service.
  // These methods exist so they can be overridden in tests as needed.
  // The `result` argument exists as an optimization to finish the request early
  // when the result is known in advance to avoid using the upload service.
  void UploadFileForDeepScanning(ScanRequestUploadResult result,
                                 std::unique_ptr<BinaryUploadRequest> request);

  // This is set to true as soon as a TOO_MANY_REQUESTS response is obtained. No
  // more data should be upload for `this` at that point.
  bool throttled_ = false;

  std::unique_ptr<Delegate> delegate_;
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_FILES_REQUEST_HANDLER_BASE_H_
