// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_FILES_REQUEST_HANDLER_BASE_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_FILES_REQUEST_HANDLER_BASE_H_

#include "base/gtest_prod_util.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/common.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/file_analysis_request_base.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/request_handler_base.h"

namespace enterprise_connectors {

class BinaryUploadService;
class ReportingEventRouter;

// A base class for handling scanning and reporting of deep scanning requests
// for files.
class FilesRequestHandlerBase : public RequestHandlerBase {
 public:
  // File information used as an input to event report functions.
  struct FileInfo {
    FileInfo();
    FileInfo(FileInfo&& other);
    ~FileInfo();

    // Hex-encoded SHA256 hash for the given file, or a callback to register a
    // function to be called with the hash as an argument.
    HashCallbackVariant sha256_or_cb;

    // File size in bytes. 0 represents an unknown size.
    uint64_t size = 0;

    // File mime type.
    std::string mime_type;
  };

  // Delegate interface to be implemented by child classes to handle the
  // specifics of different types of file requests. Methods with an `index`
  // parameter are intended to support multiple files on other platforms
  // (e.g. desktop). Since iOS only supports single-file operations, the `index`
  // will always be 0 for iOS.
  class Delegate {
   public:
    Delegate() = default;
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;
    virtual ~Delegate() = default;

    // Creates a platform-specific upload request for the file at `index`.
    virtual std::unique_ptr<FileAnalysisRequestBase> CreateFileRequest(
        size_t index,
        const AnalysisSettings& settings,
        base::OnceCallback<void(ScanRequestUploadResult,
                                ContentAnalysisResponse)> callback,
        base::OnceCallback<void(const BinaryUploadRequest&)>
            request_start_callback) = 0;

    // Called when a user bypasses a scanning warning.
    virtual void ReportWarningBypass(
        std::optional<std::u16string> user_justification,
        const ContentAnalysisInfoBase& info,
        const std::string& trigger,
        const std::string& content_transfer_method) = 0;

    // Returns true if scanning should proceed for the files managed by this
    // delegate.
    virtual bool UploadDataImpl() = 0;

    // Updates the `RequestHandlerResult` in `result_` for a scanning request
    // corresponding to the given `index`, and update the file_warnings_
    // accordingly based on the `result.final_result`.
    virtual void UpdateRequestHandlerResult(
        size_t index,
        RequestHandlerResult result,
        ContentAnalysisResponse response) = 0;

    // Returns the file path for the given index.
    virtual const base::FilePath& GetPath(size_t index) const = 0;

    // Returns the file_info for the given index.
    virtual const FileInfo& GetFileInfo(size_t index) = 0;

    // Returns a mutable reference to the file_info for the given index.
    virtual FileInfo& GetMutableFileInfo(size_t index) = 0;

    // Returns the number of files to be scanned.
    virtual size_t GetFileCount() const = 0;

    // Set the scan start time of the file for the given index.
    virtual void SetFileScanStartTime(size_t index) = 0;

    // Returns the start time of the file scan for the given index.
    virtual const base::TimeTicks GetFileScanStartTime(size_t index) = 0;

    // Returns the reporting event router.
    virtual ReportingEventRouter* GetReportingEventRouter() = 0;

    // Notifies that a scan request might be complete.
    virtual void MaybeCompleteScanRequest() = 0;

    // The source and destination strings are only for ChromeOS.
    virtual std::string GetSource() = 0;
    virtual std::string GetDestination() = 0;

    // Sets the handler for this delegate.
    virtual void SetHandler(FilesRequestHandlerBase* handler) = 0;

    // Cancels any pending file requests and reports the cancellation for any
    // files that have not been reported yet.
    virtual void MaybeCancelAndReport() = 0;

    // Marks the file at the given index as reported.
    virtual void MarkFileAsReported(size_t index) = 0;
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
      const std::string& content_transfer_method,
      DeepScanAccessPoint access_point,
      std::unique_ptr<FilesRequestHandlerBase::Delegate> delegate);

  FilesRequestHandlerBase(const FilesRequestHandlerBase&) = delete;
  FilesRequestHandlerBase& operator=(const FilesRequestHandlerBase&) = delete;

  ~FilesRequestHandlerBase() override;

  // This should only call the delegate_->ReportWarningBypass.
  void ReportWarningBypass(
      std::optional<std::u16string> user_justification) override;

  base::WeakPtr<FilesRequestHandlerBase> GetWeakPtr();

  // Prepares an upload request for the file at `index`.
  FileAnalysisRequestBase* PrepareFileRequest(size_t index);

  // Reports a user cancellation for the file at `index`.
  void ReportCanceledFile(size_t index);

  size_t file_result_count() const;
  const std::string& content_transfer_method() const;

 protected:
  // Initiates scanning for all files managed by the delegate.
  bool UploadDataImpl() override;

  // Upload the request for deep scanning using the binary upload service.
  // These methods exist so they can be overridden in tests as needed.
  // The `result` argument exists as an optimization to finish the request early
  // when the result is known in advance to avoid using the upload service.
  virtual void UploadFileForDeepScanning(
      ScanRequestUploadResult result,
      const base::FilePath& path,
      std::unique_ptr<BinaryUploadRequest> request);

  void FileRequestCallback(
      size_t index,
      ScanRequestUploadResult upload_result,
      enterprise_connectors::ContentAnalysisResponse response);

 private:
  FRIEND_TEST_ALL_PREFIXES(FilesRequestHandlerBaseTest, OnGotFileInfo_Success);
  FRIEND_TEST_ALL_PREFIXES(FilesRequestHandlerBaseTest,
                           OnGotFileInfo_EmptyFile);
  FRIEND_TEST_ALL_PREFIXES(FilesRequestHandlerBaseTest, OnGotFileInfo_Failure);
  FRIEND_TEST_ALL_PREFIXES(FilesRequestHandlerBaseTest,
                           OnGotFileInfo_FileTooLarge_Blocked);
  FRIEND_TEST_ALL_PREFIXES(FilesRequestHandlerBaseTest,
                           OnGotFileInfo_FileTooLarge_NotBlocked);
  FRIEND_TEST_ALL_PREFIXES(FilesRequestHandlerBaseTest, FileRequestCallback);
  FRIEND_TEST_ALL_PREFIXES(FilesRequestHandlerBaseTest,
                           Destructor_ReportsCancellation);

  void OnGotFileInfo(std::unique_ptr<BinaryUploadRequest> request,
                     size_t index,
                     ScanRequestUploadResult result,
                     BinaryUploadRequest::Data data);

  void OnGotHash(size_t index, std::string hash);

  void MaybeTrackCancellation();

  // Called when a request is finished early without uploading it.
  // This is, e.g., called for encrypted files and responsible for posting the
  // required data to safe-browsing ui.
  void FinishRequestEarly(std::unique_ptr<BinaryUploadRequest> request,
                          ScanRequestUploadResult result);

  void FileRequestStartCallback(size_t index,
                                const BinaryUploadRequest& request);

  // This is set to true as soon as a TOO_MANY_REQUESTS response is obtained. No
  // more data should be upload for `this` at that point.
  bool throttled_ = false;

  // The number of file scans that have completed. If more than one file is
  // requested for scanning in `data_`, each is scanned in parallel with
  // separate requests.
  size_t file_result_count_ = 0;

  std::string content_transfer_method_;
  std::unique_ptr<Delegate> delegate_;

  base::WeakPtrFactory<FilesRequestHandlerBase> weak_ptr_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_FILES_REQUEST_HANDLER_BASE_H_
