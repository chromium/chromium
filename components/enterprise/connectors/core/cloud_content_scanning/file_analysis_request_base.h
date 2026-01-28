// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_FILE_ANALYSIS_REQUEST_BASE_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_FILE_ANALYSIS_REQUEST_BASE_H_

#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_request.h"
#include "components/enterprise/connectors/core/service_provider_config.h"
#include "components/file_access/scoped_file_access.h"

namespace enterprise_connectors {

// A BinaryUploadRequest implementation that gets the data to scan from the
// contents of a file. It caches the results so that future calls to
// GetRequestData will return quickly.
class FileAnalysisRequestBase : public BinaryUploadRequest {
 public:
  FileAnalysisRequestBase(
      const AnalysisSettings& analysis_settings,
      base::FilePath path,
      base::FilePath file_name,
      std::string mime_type,
      bool delay_opening_file,
      BinaryUploadRequest::ContentAnalysisCallback callback,
      BrowserPolicyConnectorGetter policy_connector_getter,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      BinaryUploadRequest::RequestStartCallback start_callback =
          base::DoNothing(),
      bool is_obfuscated = false,
      bool force_sync_hash_computation = true);
  FileAnalysisRequestBase(const FileAnalysisRequestBase&) = delete;
  FileAnalysisRequestBase& operator=(const FileAnalysisRequestBase&) = delete;
  ~FileAnalysisRequestBase() override;

  // BinaryUploadRequest implementation. If |delay_opening_file_| is false,
  // OnGotFileData is called by posting after GetFileDataBlocking runs a
  // base::MayBlock() thread, otherwise the callback will be stored and run
  // later when OpenFile is called.
  void GetRequestData(DataCallback callback) override;

  // Opens the file, reads it, and then calls OnGotFileData on the UI thread.
  // This should be called on a thread with base::MayBlock().
  void OpenFile();

 protected:
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  virtual void ProcessZipFile(Data data) = 0;
  virtual void ProcessRarFile(Data data) = 0;
#endif

  // Wrapped by BinaryUploadRequest::register_on_got_hash_callback_. Use
  // call_last = true if a callback may delete this object after it
  // is run.
  void RegisterOnGotHashCallback(
      bool call_last,
      enterprise_connectors::OnGotHashCallback callback);
  void OnGotHash(std::string hash);

  void OnGotFileData(std::pair<ScanRequestUploadResult, Data> result_and_data);

  // Caches the result and data from file processing. This allows future calls
  // to GetRequestData to return synchronously. Subclasses should call this
  // after all processing is complete.
  void CacheResultAndData(ScanRequestUploadResult result, Data data);

  // Runs |data_callback_|.
  void RunCallback();

  void GetData(file_access::ScopedFileAccess file_access);

  // Helper functions to access the request proto.
  bool HasMalwareRequest() const;

  bool has_cached_result_ = false;
  ScanRequestUploadResult cached_result_;
  Data cached_data_;

  // Analysis settings relevant to file analysis requests, copied from the
  // overall analysis settings.
  std::map<std::string, TagSettings> tag_settings_;

  // Path to the file on disk.
  base::FilePath path_;

  // File name excluding the path.
  base::FilePath file_name_;

  DataCallback data_callback_;

  // The file being opened can be delayed so that an external class can have
  // more control on parallelism when multiple files are being opened. If
  // |delay_opening_file_| is false, a task to open the file is posted in the
  // GetRequestData call.
  bool delay_opening_file_;

  // Whether the file contents have been obfuscated during the download
  // process.
  bool is_obfuscated_ = false;

  // Controls whether OpenFile tasks can compute hash after notifying
  // OnGotFileInfo.
  bool force_sync_hash_computation_ = true;

  std::deque<enterprise_connectors::OnGotHashCallback> hash_notify_callbacks_;

  std::unique_ptr<file_access::ScopedFileAccess> scoped_file_access_;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  base::WeakPtrFactory<FileAnalysisRequestBase> weakptr_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_FILE_ANALYSIS_REQUEST_BASE_H_
