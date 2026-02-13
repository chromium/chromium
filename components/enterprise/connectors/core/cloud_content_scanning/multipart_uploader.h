// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_MULTIPART_UPLOADER_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_MULTIPART_UPLOADER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/connector_data_pipe_getter.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/connector_upload_request.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/multipart_uploader_base.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"

namespace safe_browsing {

// This class encapsulates the upload of a file with metadata using the
// multipart protocol. This class is neither movable nor copyable.
//
// TODO(crbug.com/481674868): Combine multipart uploader base class and the
// corresponding unit tests with this.
class MultipartUploadRequest
    : public enterprise_connectors::MultipartUploadRequestBase {
 public:
  // Creates a MultipartUploadRequest, which will upload `data` to the given
  // `base_url` with `metadata` attached.
  MultipartUploadRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      const std::string& data,
      const std::string& histogram_suffix,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      Callback callback,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  // Creates a MultipartUploadRequest, which will upload the file corresponding
  // to `path` to the given `base_url` with `metadata` attached.
  MultipartUploadRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      const base::FilePath& path,
      uint64_t file_size,
      bool is_obfuscated,
      const std::string& histogram_suffix,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      Callback callback,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  // Creates a MultipartUploadRequest, which will upload the page in
  // `page_region` to the given `base_url` with `metadata` attached.
  MultipartUploadRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      base::ReadOnlySharedMemoryRegion page_region,
      const std::string& histogram_suffix,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      Callback callback,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  MultipartUploadRequest(const MultipartUploadRequest&) = delete;
  MultipartUploadRequest& operator=(const MultipartUploadRequest&) = delete;
  MultipartUploadRequest(MultipartUploadRequest&&) = delete;
  MultipartUploadRequest& operator=(MultipartUploadRequest&&) = delete;

  ~MultipartUploadRequest() override;

  static std::unique_ptr<enterprise_connectors::ConnectorUploadRequest>
  CreateStringRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      const std::string& data,
      const std::string& histogram_suffix,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      MultipartUploadRequest::Callback callback,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  static std::unique_ptr<enterprise_connectors::ConnectorUploadRequest>
  CreateFileRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      const base::FilePath& file,
      uint64_t file_size,
      bool is_obfuscated,
      const std::string& histogram_suffix,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      MultipartUploadRequest::Callback callback,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  static std::unique_ptr<enterprise_connectors::ConnectorUploadRequest>
  CreatePageRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      base::ReadOnlySharedMemoryRegion page_region,
      const std::string& histogram_suffix,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      MultipartUploadRequest::Callback callback,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_MULTIPART_UPLOADER_H_
