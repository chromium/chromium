// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_RESUMABLE_UPLOADER_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_RESUMABLE_UPLOADER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/common.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/connector_data_pipe_getter.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/connector_upload_request.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/resumable_uploader_base.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace safe_browsing {

// This class encapsulates the upload of a file with metadata using the
// resumable protocol. This class is neither movable nor copyable.
//
// TODO(crbug.com/481674868): Combine resumabled uploader base class with this.
class ResumableUploadRequest
    : public enterprise_connectors::ResumableUploadRequestBase {
 public:
  using enterprise_connectors::ResumableUploadRequestBase::
      ContentUploadedCallback;
  using enterprise_connectors::ResumableUploadRequestBase::
      VerdictReceivedCallback;

  // Creates a ResumableUploadRequest, which will upload the `metadata` of the
  // file corresponding to the provided `path` to the given `base_url`, and then
  // the file content to the `path` if necessary.
  //
  // `get_data_result` is the result when getting basic information about the
  // file or page.  It lets the ResumableUploadRequest know if the data is
  // considered too large or is encrypted.
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
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  // Creates a ResumableUploadRequest, which will upload the `metadata` of the
  // page to the given `base_url`, and then the content of `page_region` if
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
      enterprise_connectors::ConnectorUploadRequest::DataSource data_source,
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

  static std::unique_ptr<enterprise_connectors::ConnectorUploadRequest>
  CreateStringRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      const std::string& data,
      enterprise_connectors::ConnectorUploadRequest::DataSource data_source,
      const std::string& histogram_suffix,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      VerdictReceivedCallback verdict_received_callback,
      ContentUploadedCallback content_uploaded_callback,
      bool force_sync_upload,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  static std::unique_ptr<enterprise_connectors::ConnectorUploadRequest>
  CreateFileRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      enterprise_connectors::ScanRequestUploadResult get_data_result,
      const base::FilePath& file,
      uint64_t file_size,
      bool is_obfuscated,
      const std::string& histogram_suffix,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      VerdictReceivedCallback verdict_received_callback,
      ContentUploadedCallback content_uploaded_callback,
      bool force_sync_upload,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  static std::unique_ptr<enterprise_connectors::ConnectorUploadRequest>
  CreatePageRequest(
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
};

}  // namespace safe_browsing

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_RESUMABLE_UPLOADER_H_
