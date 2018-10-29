// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_job_factory.h"

#include <memory>

#include "components/download/internal/common/download_job_impl.h"
#include "components/download/internal/common/parallel_download_job.h"
#include "components/download/internal/common/parallel_download_utils.h"
#include "components/download/internal/common/save_package_download_job.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_stats.h"
#include "components/download/public/common/download_url_loader_factory_getter.h"
#include "net/url_request/url_request_context_getter.h"

namespace download {

namespace {

// Returns if the download can be parallelized.
bool IsParallelizableDownload(const DownloadCreateInfo& create_info,
                              DownloadItem* download_item) {
  // To enable parallel download, following conditions need to be satisfied.
  // 1. Feature |kParallelDownloading| enabled.
  // 2. Strong validators response headers. i.e. ETag and Last-Modified.
  // 3. Accept-Ranges or Content-Range header.
  // 4. Content-Length header.
  // 5. Content-Length is no less than the minimum slice size configuration, or
  // persisted slices alreay exist.
  // 6. HTTP/1.1 protocol, not QUIC nor HTTP/1.0.
  // 7. HTTP or HTTPS scheme with GET method in the initial request.

  // Etag and last modified are stored into DownloadCreateInfo in
  // DownloadRequestCore only if the response header complies to the strong
  // validator rule.
  bool has_strong_validator =
      !create_info.etag.empty() || !create_info.last_modified.empty();
  bool has_content_length = create_info.total_bytes > 0;
  bool satisfy_min_file_size =
      !download_item->GetReceivedSlices().empty() ||
      create_info.total_bytes >= GetMinSliceSizeConfig();
  bool satisfy_connection_type = create_info.connection_info ==
                                 net::HttpResponseInfo::CONNECTION_INFO_HTTP1_1;
  bool http_get_method =
      create_info.method == "GET" && create_info.url().SchemeIsHTTPOrHTTPS();
  bool partial_response_success =
      download_item->GetReceivedSlices().empty() || create_info.offset != 0;
  bool is_parallelizable = has_strong_validator && create_info.accept_range &&
                           has_content_length && satisfy_min_file_size &&
                           satisfy_connection_type && http_get_method &&
                           partial_response_success;

  if (!IsParallelDownloadEnabled())
    return is_parallelizable;

  RecordParallelDownloadCreationEvent(
      is_parallelizable
          ? ParallelDownloadCreationEvent::STARTED_PARALLEL_DOWNLOAD
          : ParallelDownloadCreationEvent::FELL_BACK_TO_NORMAL_DOWNLOAD);

  if (!has_strong_validator) {
    RecordParallelDownloadCreationEvent(
        ParallelDownloadCreationEvent::FALLBACK_REASON_STRONG_VALIDATORS);
  }
  if (!create_info.accept_range) {
    RecordParallelDownloadCreationEvent(
        ParallelDownloadCreationEvent::FALLBACK_REASON_ACCEPT_RANGE_HEADER);
  }
  if (!has_content_length) {
    RecordParallelDownloadCreationEvent(
        ParallelDownloadCreationEvent::FALLBACK_REASON_CONTENT_LENGTH_HEADER);
  }
  if (!satisfy_min_file_size) {
    RecordParallelDownloadCreationEvent(
        ParallelDownloadCreationEvent::FALLBACK_REASON_FILE_SIZE);
  }
  if (!satisfy_connection_type) {
    RecordParallelDownloadCreationEvent(
        ParallelDownloadCreationEvent::FALLBACK_REASON_CONNECTION_TYPE);
  }
  if (!http_get_method) {
    RecordParallelDownloadCreationEvent(
        ParallelDownloadCreationEvent::FALLBACK_REASON_HTTP_METHOD);
  }

  return is_parallelizable;
}

}  // namespace

// static
std::unique_ptr<DownloadJob> DownloadJobFactory::CreateJob(
    DownloadItem* download_item,
    std::unique_ptr<DownloadRequestHandleInterface> req_handle,
    const DownloadCreateInfo& create_info,
    bool is_save_package_download,
    scoped_refptr<download::DownloadURLLoaderFactoryGetter>
        url_loader_factory_getter,
    net::URLRequestContextGetter* url_request_context_getter) {
  if (is_save_package_download) {
    return std::make_unique<SavePackageDownloadJob>(download_item,
                                                    std::move(req_handle));
  }

  bool is_parallelizable = IsParallelizableDownload(create_info, download_item);
  // Build parallel download job.
  if (IsParallelDownloadEnabled() && is_parallelizable) {
    return std::make_unique<ParallelDownloadJob>(
        download_item, std::move(req_handle), create_info,
        std::move(url_loader_factory_getter), url_request_context_getter);
  }

  // An ordinary download job.
  return std::make_unique<DownloadJobImpl>(download_item, std::move(req_handle),
                                           is_parallelizable);
}

}  // namespace download
