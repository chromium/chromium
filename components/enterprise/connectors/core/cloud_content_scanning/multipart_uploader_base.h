// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_MULTIPART_UPLOADER_BASE_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_MULTIPART_UPLOADER_BASE_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/connector_data_pipe_getter.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/connector_upload_request.h"
#include "components/file_access/scoped_file_access.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"

namespace enterprise_connectors {

class BrowserThreadGuard {
 public:
  virtual void AssertCalledOnUIThread() = 0;
  virtual ~BrowserThreadGuard() = default;
};

// This class encapsulates the upload of a file with metadata using the
// multipart protocol. This class is neither movable nor copyable.
class MultipartUploadRequestBase : public ConnectorUploadRequest {
 public:
  // Creates a MultipartUploadRequestBase, which will upload `data` to the given
  // `base_url` with `metadata` attached.
  MultipartUploadRequestBase(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      const std::string& data,
      const std::string& histogram_suffix,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      Callback callback,
      std::unique_ptr<BrowserThreadGuard> thread_guard);

  // Creates a MultipartUploadRequestBase, which will upload the file
  // corresponding to `path` to the given `base_url` with `metadata` attached.
  MultipartUploadRequestBase(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      const base::FilePath& path,
      uint64_t file_size,
      bool is_obfuscated,
      const std::string& histogram_suffix,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      Callback callback,
      std::unique_ptr<BrowserThreadGuard> thread_guard);

  // Creates a MultipartUploadRequestBase, which will upload the page in
  // `page_region` to the given `base_url` with `metadata` attached.
  MultipartUploadRequestBase(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      base::ReadOnlySharedMemoryRegion page_region,
      const std::string& histogram_suffix,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      Callback callback,
      std::unique_ptr<BrowserThreadGuard> thread_guard);

  MultipartUploadRequestBase(const MultipartUploadRequestBase&) = delete;
  MultipartUploadRequestBase& operator=(const MultipartUploadRequestBase&) =
      delete;
  MultipartUploadRequestBase(MultipartUploadRequestBase&&) = delete;
  MultipartUploadRequestBase& operator=(MultipartUploadRequestBase&&) = delete;

  ~MultipartUploadRequestBase() override;

  // Start the upload. This must be called on the UI thread. When complete, this
  // will call `callback_` on the UI thread.
  void Start() override;

  std::string GetUploadInfo() override;

  void SetRequestHeaders(network::ResourceRequest* request);

  // Update `scan_type_` to be CONTENT to indicate that the content scan is
  // successful. Used in testing only.
  void MarkScanAsCompleteForTesting();

 protected:
  // Helper method to create the multipart request body.
  std::string GenerateRequestBody(const std::string& metadata,
                                  const std::string& data);
  // Called to send a single request. Is overridden in tests.
  virtual void SendRequest();

  // Called whenever a net request finishes (on success or failure).
  void OnURLLoaderComplete(std::optional<std::string> response_body);

  // Called whenever a net request finishes (on success or failure).
  void RetryOrFinish(int net_error,
                     int response_code,
                     std::optional<std::string> response_body);

  // Set the boundary between parts.
  void set_boundary(const std::string& boundary) { boundary_ = boundary; }

 private:
  virtual scoped_refptr<base::TaskRunner> GetTaskRunner() = 0;

  // Called by SendFileRequest and SendPageRequest after `data_pipe_getter_`
  // is known to be initialized to a correct state.
  virtual void CompleteSendRequest(
      std::unique_ptr<network::ResourceRequest> request);

  void SendStringRequest(std::unique_ptr<network::ResourceRequest> request);
  void SendFileRequest(std::unique_ptr<network::ResourceRequest> request);
  void SendPageRequest(std::unique_ptr<network::ResourceRequest> request);

  // Called after `data_pipe_getter_` has been initialized.
  void DataPipeCreatedCallback(
      std::unique_ptr<network::ResourceRequest> request,
      std::unique_ptr<enterprise_connectors::ConnectorDataPipeGetter>
          data_pipe_getter);

  void CreateDatapipe(std::unique_ptr<network::ResourceRequest> request,
                      file_access::ScopedFileAccess file_access);

  std::unique_ptr<BrowserThreadGuard> thread_guard_;
  std::unique_ptr<file_access::ScopedFileAccess> scoped_file_access_;
  std::string boundary_;
  base::Time start_time_;
  base::TimeDelta current_backoff_;
  int retry_count_;
  bool scan_complete_ = false;
  bool is_obfuscated_ = false;

  base::WeakPtrFactory<MultipartUploadRequestBase> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_MULTIPART_UPLOADER_BASE_H_
