// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_job_factory.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/download/internal/common/download_job_impl.h"
#include "components/download/internal/common/parallel_download_job.h"
#include "components/download/internal/common/parallel_download_utils.h"
#include "components/download/internal/common/save_package_download_job.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/download_item.h"
#include "net/http/http_connection_info.h"

namespace download {

namespace {
// Connection type of the download.
enum class ConnectionType {
  kHTTP = 0,
  kHTTP2,
  kQUIC,
  kUnknown,
};

ConnectionType GetConnectionType(net::HttpConnectionInfo connection_info) {
  switch (net::HttpConnectionInfoToCoarse(connection_info)) {
    case net::HttpConnectionInfoCoarse::kHTTP1:
      return ConnectionType::kHTTP;
    case net::HttpConnectionInfoCoarse::kHTTP2:
      return ConnectionType::kHTTP2;
    case net::HttpConnectionInfoCoarse::kQUIC:
      return ConnectionType::kQUIC;
    case net::HttpConnectionInfoCoarse::kOTHER:
      return ConnectionType::kUnknown;
  }
  NOTREACHED_IN_MIGRATION();
  return ConnectionType::kUnknown;
}

// Returns if the download can be parallelized.
bool IsParallelizableDownload(const DownloadCreateInfo& create_info,
                              DownloadItem* download_item) {
  if (download_item->GetDownloadFile() &&
      download_item->GetDownloadFile()->IsMemoryFile()) {
    return false;
  }

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
  ConnectionType type = GetConnectionType(create_info.connection_info);
  bool satisfy_connection_type =
      (create_info.connection_info == net::HttpConnectionInfo::kHTTP1_1) ||
      (type == ConnectionType::kHTTP2 &&
       base::FeatureList::IsEnabled(features::kUseParallelRequestsForHTTP2)) ||
      (type == ConnectionType::kQUIC &&
       base::FeatureList::IsEnabled(features::kUseParallelRequestsForQUIC));
  bool http_get_method =
      create_info.method == "GET" && create_info.url().SchemeIsHTTPOrHTTPS();
  // If the file is empty, we always assume parallel download is supported.
  // Otherwise, check if the download already has multiple slices and whether
  // the http response offset is non-zero.
  bool can_support_parallel_requests =
      download_item->GetReceivedBytes() <= 0 ||
      (download_item->GetReceivedSlices().size() > 0 &&
       create_info.offset != 0);

  bool range_support_allowed =
      create_info.accept_range == RangeRequestSupportType::kSupport ||
      create_info.accept_range == RangeRequestSupportType::kUnknown;

  bool is_parallelizable = has_strong_validator && range_support_allowed &&
                           has_content_length && satisfy_min_file_size &&
                           satisfy_connection_type && http_get_method &&
                           can_support_parallel_requests;

  // Don't use parallel download if the caller wants to fetch with range
  // request explicitly.
  bool arbitrary_range_request =
      create_info.save_info && create_info.save_info->IsArbitraryRangeRequest();
  is_parallelizable = is_parallelizable && !arbitrary_range_request;

  return is_parallelizable;
}

}  // namespace

// static
std::unique_ptr<DownloadJob> DownloadJobFactory::CreateJob(
    DownloadItem* download_item,
    DownloadJob::CancelRequestCallback cancel_request_callback,
    const DownloadCreateInfo& create_info,
    bool is_save_package_download,
    URLLoaderFactoryProvider::URLLoaderFactoryProviderPtr
        url_loader_factory_provider,
    WakeLockProviderBinder wake_lock_provider_binder) {
  if (is_save_package_download) {
    return std::make_unique<SavePackageDownloadJob>(
        download_item, std::move(cancel_request_callback));
  }

  bool is_parallelizable = IsParallelizableDownload(create_info, download_item);
  // Build parallel download job.
  if (IsParallelDownloadEnabled() && is_parallelizable) {
    return std::make_unique<ParallelDownloadJob>(
        download_item, std::move(cancel_request_callback), create_info,
        std::move(url_loader_factory_provider),
        std::move(wake_lock_provider_binder));
  }

  // An ordinary download job.
  return std::make_unique<DownloadJobImpl>(
      download_item, std::move(cancel_request_callback), is_parallelizable);
}

}  // namespace download
