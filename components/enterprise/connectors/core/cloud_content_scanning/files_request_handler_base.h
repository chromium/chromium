// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_FILES_REQUEST_HANDLER_BASE_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_FILES_REQUEST_HANDLER_BASE_H_

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

  std::unique_ptr<Delegate> delegate_;
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_FILES_REQUEST_HANDLER_BASE_H_
