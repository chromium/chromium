// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_RESUMABLE_UPLOADER_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_RESUMABLE_UPLOADER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/common.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/connector_data_pipe_getter.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/connector_upload_request.h"
#include "components/file_access/scoped_file_access.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace enterprise_connectors {

// This class encapsulates the upload of a file with metadata using the
// resumable protocol. This class is neither movable nor copyable.
class ResumableUploadRequest : public ConnectorUploadRequest {
 public:
  using ContentUploadedCallback = base::OnceClosure;
  using VerdictReceivedCallback =
      enterprise_connectors::ConnectorUploadRequest::Callback;
  using OnceRegisterOnGotHashCallback =
      base::OnceCallback<void(enterprise_connectors::OnGotHashCallback)>;
  using ConnectorUploadRequest::ConnectorUploadRequest;

  // Creates a ResumableUploadRequest, which will upload the `metadata` of
  // the file corresponding to the provided `path` to the given `base_url`, and
  // then the file content to the `path` if necessary.
  //
  // `get_data_result` is the result when getting basic information about the
  // file or page.  It lets the ResumableUploadRequest know if the data is
  // considered too large or is encrypted.
  //
  // when `register_on_got_hash_callback` is non-null the final call should
  // include the file hash as a header. ResumableUploadRequest should run it
  // with a callback that receives a string and completes the upload with the
  // hash.
  ResumableUploadRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      enterprise_connectors::ScanRequestUploadResult get_data_result,
      const base::FilePath& path,
      uint64_t file_size,
      bool is_obfuscated,
      const std::string& histogram_suffix,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      VerdictReceivedCallback verdict_received_callback,
      ContentUploadedCallback content_uploaded_callback,
      bool force_sync_upload,
      OnceRegisterOnGotHashCallback register_on_got_hash_callback,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  // Creates a ResumableUploadRequest, which will upload the `metadata` of
  // the page to the given `base_url`, and then the content of `page_region` if
  // necessary.
  ResumableUploadRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      enterprise_connectors::ScanRequestUploadResult get_data_result,
      base::ReadOnlySharedMemoryRegion page_region,
      const std::string& histogram_suffix,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      VerdictReceivedCallback verdict_received_callback,
      ContentUploadedCallback content_uploaded_callback,
      bool force_sync_upload,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  // Creates a ResumableUploadRequest, which will upload the `metadata` of a
  // pasted image to the given `base_url`, and then the `data` if necessary.
  ResumableUploadRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      const std::string& data,
      ConnectorUploadRequest::DataSource data_source,
      const std::string& histogram_suffix,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      VerdictReceivedCallback verdict_received_callback,
      ContentUploadedCallback content_uploaded_callback,
      bool force_sync_upload,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  // Creates a ResumableUploadRequest, which will upload the `metadata` of
  // the network request to the given `base_url`, and its `request_body` if
  // necessary.
  ResumableUploadRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      scoped_refptr<network::ResourceRequestBody> request_body,
      const std::string& histogram_suffix,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      VerdictReceivedCallback verdict_received_callback,
      ContentUploadedCallback content_uploaded_callback,
      bool force_sync_upload,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  ResumableUploadRequest(const ResumableUploadRequest&) = delete;
  ResumableUploadRequest& operator=(const ResumableUploadRequest&) = delete;
  ResumableUploadRequest(ResumableUploadRequest&&) = delete;
  ResumableUploadRequest& operator=(ResumableUploadRequest&&) = delete;

  ~ResumableUploadRequest() override;

  static std::unique_ptr<ConnectorUploadRequest> CreateStringRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      const std::string& data,
      ConnectorUploadRequest::DataSource data_source,
      const std::string& histogram_suffix,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      VerdictReceivedCallback verdict_received_callback,
      ContentUploadedCallback content_uploaded_callback,
      bool force_sync_upload,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  static std::unique_ptr<ConnectorUploadRequest> CreateFileRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      ScanRequestUploadResult get_data_result,
      const base::FilePath& file,
      uint64_t file_size,
      bool is_obfuscated,
      const std::string& histogram_suffix,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      VerdictReceivedCallback verdict_received_callback,
      ContentUploadedCallback content_uploaded_callback,
      bool force_sync_upload,
      OnceRegisterOnGotHashCallback register_on_got_hash_callback,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  static std::unique_ptr<ConnectorUploadRequest> CreatePageRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      ScanRequestUploadResult get_data_result,
      base::ReadOnlySharedMemoryRegion page_region,
      const std::string& histogram_suffix,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      VerdictReceivedCallback verdict_received_callback,
      ContentUploadedCallback content_uploaded_callback,
      bool force_sync_upload,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  static std::unique_ptr<ConnectorUploadRequest> CreateNetworkRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      scoped_refptr<network::ResourceRequestBody> request_body,
      const std::string& histogram_suffix,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      VerdictReceivedCallback verdict_received_callback,
      ContentUploadedCallback content_uploaded_callback,
      bool force_sync_upload,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  // Called whenever a content request finishes (on success or failure).
  void OnSendContentCompleted(base::TimeTicks start_time,
                              std::optional<std::string> response_body);

  // Set the headers for the given metadata `request`.
  void SetMetadataRequestHeaders(network::ResourceRequest* request);

  std::string GetUploadInfo() override;

  // Start the upload. This must be called on the UI thread. When complete, this
  // will call `callback_` on the UI thread.
  void Start() override;

 protected:
  // Called after a metadata request finishes successfully. Virtual for testing.
  virtual void SendContentSoon();

  // Called whenever a net request finishes (on success or failure). Protected
  // for testing
  void Finish(int net_error,
              int response_code,
              std::optional<std::string> response_body);

  bool force_sync_upload() const { return force_sync_upload_; }

  VerdictReceivedCallback verdict_received_callback_;

 private:
  // Called whenever a metadata request finishes (on success or failure).
  void OnMetadataUploadCompleted(base::TimeTicks start_time,
                                 std::optional<std::string> response_body);

  // Initialize `data_pipe_getter_`
  void CreateDatapipe(std::unique_ptr<network::ResourceRequest> request,
                      file_access::ScopedFileAccess file_access);

  // Called after `data_pipe_getter_` has be
  void OnDataPipeCreated(
      std::unique_ptr<network::ResourceRequest> request,
      std::unique_ptr<enterprise_connectors::ConnectorDataPipeGetter>
          data_pipe_getter);

  // Called after `data_pipe_getter_` is known to be initialized to a correct
  // state, if there is one.
  void SendContentNow(std::unique_ptr<network::ResourceRequest> request);

  // Run VerdictReceivedCallback exactly once. Further calls to this function
  // have no side effects.
  void MaybeRunVerdictReceivedCallback(
      int net_error,
      int response_code,
      std::optional<std::string> response_body);

  // Send the metadata information about the file/page to the server.
  void SendMetadataRequest();

  // Returns true if all of the following conditions are met:
  //    1. The HTTP status is OK.
  //    2. The `headers` have `upload_status` and `upload_url`.
  //    3. The `upload_status` is "active".
  // This method also has the side effect of setting upload_url_.
  bool CanUploadContent(const scoped_refptr<net::HttpResponseHeaders>& headers);

  void MaybeSendHashAndFinish(size_t upload_offset,
                              int net_error,
                              int response_code,
                              std::optional<std::string> response_body);

  void SendHashNow(std::unique_ptr<network::ResourceRequest> request,
                   std::string hash);

  void OnSendHashCompleted(std::optional<std::string> response_body);

  // Returns true if `kEnableEncryptedFileUpload`
  // feature is enabled and the `scan_type_` is ASYNC.
  bool ShouldUploadEncryptedFile();

  // Helper used by metrics logging code.
  std::string GetRequestType();

  enum {
    PENDING = 0,
    METADATA_ONLY = 1,
    FULL_CONTENT = 2,
    ASYNC = 3
  } scan_type_ = PENDING;

  ContentUploadedCallback content_uploaded_callback_;

  // The result returned by BinaryUploadService::Request::GetRequestData() when
  // retrieving the data.
  ScanRequestUploadResult get_data_result_;

  bool force_sync_upload_ = false;

  OnceRegisterOnGotHashCallback register_on_got_hash_callback_;
  bool file_hash_computation_is_async_ = false;

  // The upload URL returned from the metadata request.
  std::string upload_url_;

  base::WeakPtrFactory<ResumableUploadRequest> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_RESUMABLE_UPLOADER_H_
