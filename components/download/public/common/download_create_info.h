// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_CREATE_INFO_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_CREATE_INFO_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_save_info.h"
#include "components/download/public/common/download_source.h"
#include "components/download/public/common/download_url_parameters.h"
#include "net/http/http_connection_info.h"
#include "net/url_request/referrer_policy.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {
class HttpResponseHeaders;
}

namespace download {
// Server support for range request inferred from the response headers.
// |kSupport| value means the server supports range requests. |kNoSupport|
// means no range request is accepted by server. and |kUnknown| is used if
// range request support cannot be inferred from response headers.
enum class RangeRequestSupportType {
  kSupport = 0,
  kUnknown,
  kNoSupport,
};

// Used for informing the download manager of a new download, since we don't
// want to pass |DownloadItem|s between threads.
struct COMPONENTS_DOWNLOAD_EXPORT DownloadCreateInfo {
  DownloadCreateInfo(const base::Time& start_time,
                     std::unique_ptr<DownloadSaveInfo> save_info);
  DownloadCreateInfo();

  DownloadCreateInfo(const DownloadCreateInfo&) = delete;
  DownloadCreateInfo& operator=(const DownloadCreateInfo&) = delete;

  ~DownloadCreateInfo();

  bool is_new_download;

  // The URL from which we are downloading. This is the final URL after any
  // redirection by the server for |url_chain|.
  const GURL& url() const;

  // The unique identifier for the download.
  std::string guid;

  // The chain of redirects that leading up to and including the final URL.
  std::vector<GURL> url_chain;

  // The URL and referrer policy that referred us.
  GURL referrer_url;
  net::ReferrerPolicy referrer_policy;

  // The serialized embedder download data for the site instance that initiated
  // the download.
  std::string serialized_embedder_download_data;

  // The URL of the tab that started us.
  GURL tab_url;

  // The referrer URL of the tab that started us.
  GURL tab_referrer_url;

  // The origin of the requester that originally initiated the download.
  std::optional<url::Origin> request_initiator;

  // The time when the download started.
  base::Time start_time;

  // The size of the response body. If content-length response header is not
  // presented or can't be parse, set to 0.
  int64_t total_bytes;

  // The starting position of the initial request.
  // This value matches the offset in DownloadSaveInfo.
  // TODO(xingliu): Refactor to remove |offset| and |length|.
  int64_t offset;

  // True if the download was initiated by user action.
  bool has_user_gesture;

  // Whether the download should be transient. A transient download is
  // short-lived and is not shown in the UI.
  bool transient;

  // Whether this download requires safety checks.
  bool require_safety_checks;

  std::optional<ui::PageTransition> transition_type;

  // The HTTP response headers. This contains a nullptr when the response has
  // not yet been received. Only for consuming headers.
  scoped_refptr<const net::HttpResponseHeaders> response_headers;

  // The remote IP address where the download was fetched from.  Copied from
  // UrlRequest::GetSocketAddress().
  std::string remote_address;

  // If the download is initially created in an interrupted state (because the
  // response was in error), then |result| would be something other than
  // INTERRUPT_REASON_NONE.
  DownloadInterruptReason result;

  // The download file save info.
  std::unique_ptr<DownloadSaveInfo> save_info;

  // The render process id that initiates this download.
  int render_process_id;

  // The render frame id that initiates this download.
  int render_frame_id;

  // ---------------------------------------------------------------------------
  // The remaining fields are Entity-body properties. These are only set if
  // |result| is DOWNLOAD_INTERRUPT_REASON_NONE.
  // ---------------------------------------------------------------------------

  // The content-disposition string from the response header.
  std::string content_disposition;

  // The mime type string from the response header (may be overridden).
  std::string mime_type;

  // The value of the content type header sent with the downloaded item.  It
  // may be different from |mime_type|, which may be set based on heuristics
  // which may look at the file extension and first few bytes of the file.
  std::string original_mime_type;

  // For continuing a download, the modification time of the file.
  // Storing as a string for exact match to server format on
  // "If-Unmodified-Since" comparison.
  std::string last_modified;

  // For continuing a download, the ETag of the file.
  std::string etag;

  // Whether the server supports range requests.
  RangeRequestSupportType accept_range;

  // The HTTP connection type.
  net::HttpConnectionInfo connection_info;

  // The HTTP request method.
  std::string method;

  // Whether the download should fetch the response body for non successful HTTP
  // response.
  bool fetch_error_body = false;

  // The request headers that has been sent in the download request.
  DownloadUrlParameters::RequestHeadersType request_headers;

  // Source ID generated for UKM.
  ukm::SourceId ukm_source_id;

  // For downloads originating from custom tabs, this records the origin
  // of the custom tab.
  std::string request_origin;

  // Source of the download, used in metrics.
  DownloadSource download_source = DownloadSource::UNKNOWN;

  // Whether download is initiated by the content on the page.
  bool is_content_initiated;

  // The credentials mode for whether to expose the response headers to
  // javascript, see Access-Control-Allow-Credentials header.
  ::network::mojom::CredentialsMode credentials_mode;

  // Isolation info for the download request, mainly for same site cookies.
  std::optional<net::IsolationInfo> isolation_info;

#if BUILDFLAG(IS_ANDROID)
  // Whether the original URL must be downloaded, e.g. from context menu
  // or download service, or has "attachment" in content-disposition.
  bool is_must_download = true;
#endif  // BUILDFLAG(IS_ANDROID)
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_CREATE_INFO_H_
