// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_utils.h"

#include "base/format_macros.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "components/download/public/common/download_create_info.h"
#include "components/download/public/common/download_interrupt_reasons_utils.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_save_info.h"
#include "components/download/public/common/download_stats.h"
#include "components/download/public/common/download_url_parameters.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"

namespace download {

namespace {

void AppendExtraHeaders(net::HttpRequestHeaders* headers,
                        DownloadUrlParameters* params) {
  for (const auto& header : params->request_headers())
    headers->SetHeaderIfMissing(header.first, header.second);
}

}  // namespace

const uint32_t DownloadItem::kInvalidId = 0;

DownloadInterruptReason HandleRequestCompletionStatus(
    net::Error error_code,
    bool has_strong_validators,
    net::CertStatus cert_status,
    DownloadInterruptReason abort_reason) {
  // ERR_CONTENT_LENGTH_MISMATCH can be caused by 1 of the following reasons:
  // 1. Server or proxy closes the connection too early.
  // 2. The content-length header is wrong.
  // If the download has strong validators, we can interrupt the download
  // and let it resume automatically. Otherwise, resuming the download will
  // cause it to restart and the download may never complete if the error was
  // caused by reason 2. As a result, downloads without strong validators are
  // treated as completed here.
  // TODO(qinmin): check the metrics from downloads with strong validators,
  // and decide whether we should interrupt downloads without strong validators
  // rather than complete them.
  if (error_code == net::ERR_CONTENT_LENGTH_MISMATCH &&
      !has_strong_validators) {
    error_code = net::OK;
    RecordDownloadCount(COMPLETED_WITH_CONTENT_LENGTH_MISMATCH_COUNT);
  }

  if (error_code == net::ERR_ABORTED) {
    // ERR_ABORTED == something outside of the network
    // stack cancelled the request.  There aren't that many things that
    // could do this to a download request (whose lifetime is separated from
    // the tab from which it came).  We map this to USER_CANCELLED as the
    // case we know about (system suspend because of laptop close) corresponds
    // to a user action.
    // TODO(asanka): A lid close or other power event should result in an
    // interruption that doesn't discard the partial state, unlike
    // USER_CANCELLED. (https://crbug.com/166179)
    if (net::IsCertStatusError(cert_status))
      return DOWNLOAD_INTERRUPT_REASON_SERVER_CERT_PROBLEM;
    else
      return DOWNLOAD_INTERRUPT_REASON_USER_CANCELED;
  } else if (abort_reason != DOWNLOAD_INTERRUPT_REASON_NONE) {
    // If a more specific interrupt reason was specified before the request
    // was explicitly cancelled, then use it.
    return abort_reason;
  }

  return ConvertNetErrorToInterruptReason(error_code,
                                          DOWNLOAD_INTERRUPT_FROM_NETWORK);
}

DownloadInterruptReason HandleSuccessfulServerResponse(
    const net::HttpResponseHeaders& http_headers,
    DownloadSaveInfo* save_info,
    bool fetch_error_body) {
  DownloadInterruptReason result = DOWNLOAD_INTERRUPT_REASON_NONE;
  switch (http_headers.response_code()) {
    case -1:  // Non-HTTP request.
    case net::HTTP_OK:
    case net::HTTP_NON_AUTHORITATIVE_INFORMATION:
    case net::HTTP_PARTIAL_CONTENT:
      // Expected successful codes.
      break;

    case net::HTTP_CREATED:
    case net::HTTP_ACCEPTED:
      // Per RFC 7231 the entity being transferred is metadata about the
      // resource at the target URL and not the resource at that URL (or the
      // resource that would be at the URL once processing is completed in the
      // case of HTTP_ACCEPTED). However, we currently don't have special
      // handling for these response and they are downloaded the same as a
      // regular response.
      break;

    case net::HTTP_NO_CONTENT:
    case net::HTTP_RESET_CONTENT:
    // These two status codes don't have an entity (or rather RFC 7231
    // requires that there be no entity). They are treated the same as the
    // resource not being found since there is no entity to download.

    case net::HTTP_NOT_FOUND:
      result = DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT;
      break;

    case net::HTTP_REQUESTED_RANGE_NOT_SATISFIABLE:
      // Retry by downloading from the start automatically:
      // If we haven't received data when we get this error, we won't.
      result = DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE;
      break;
    case net::HTTP_UNAUTHORIZED:
    case net::HTTP_PROXY_AUTHENTICATION_REQUIRED:
      // Server didn't authorize this request.
      result = DOWNLOAD_INTERRUPT_REASON_SERVER_UNAUTHORIZED;
      break;
    case net::HTTP_FORBIDDEN:
      // Server forbids access to this resource.
      result = DOWNLOAD_INTERRUPT_REASON_SERVER_FORBIDDEN;
      break;
    default:  // All other errors.
      // Redirection and informational codes should have been handled earlier
      // in the stack.
      // TODO(xingliu): Handle HTTP_PRECONDITION_FAILED and resurrect
      // DOWNLOAD_INTERRUPT_REASON_SERVER_PRECONDITION for range
      // requests. This will change extensions::api::InterruptReason.
      DCHECK_NE(3, http_headers.response_code() / 100);
      DCHECK_NE(1, http_headers.response_code() / 100);
      result = DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED;
  }

  if (result != DOWNLOAD_INTERRUPT_REASON_NONE && !fetch_error_body)
    return result;

  // The caller is expecting a partial response.
  if (save_info && (save_info->offset > 0 || save_info->length > 0)) {
    if (http_headers.response_code() != net::HTTP_PARTIAL_CONTENT) {
      // Server should send partial content when "If-Match" or
      // "If-Unmodified-Since" check passes, and the range request header has
      // last byte position. e.g. "Range:bytes=50-99".
      if (save_info->length != DownloadSaveInfo::kLengthFullContent &&
          !fetch_error_body)
        return DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT;

      // Requested a partial range, but received the entire response, when
      // the range request header is "Range:bytes={offset}-".
      // The response can be HTTP 200 or other error code when
      // |fetch_error_body| is true.
      save_info->offset = 0;
      save_info->hash_of_partial_file.clear();
      save_info->hash_state.reset();
      return DOWNLOAD_INTERRUPT_REASON_NONE;
    }

    int64_t first_byte = -1;
    int64_t last_byte = -1;
    int64_t length = -1;
    if (!http_headers.GetContentRangeFor206(&first_byte, &last_byte, &length))
      return DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT;
    DCHECK_GE(first_byte, 0);

    if (first_byte != save_info->offset ||
        (save_info->length > 0 &&
         last_byte != save_info->offset + save_info->length - 1)) {
      // The server returned a different range than the one we requested. Assume
      // the response is bad.
      //
      // In the future we should consider allowing offsets that are less than
      // the offset we've requested, since in theory we can truncate the partial
      // file at the offset and continue.
      return DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT;
    }

    return DOWNLOAD_INTERRUPT_REASON_NONE;
  }

  if (http_headers.response_code() == net::HTTP_PARTIAL_CONTENT)
    return DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT;

  return DOWNLOAD_INTERRUPT_REASON_NONE;
}

void HandleResponseHeaders(const net::HttpResponseHeaders* headers,
                           DownloadCreateInfo* create_info) {
  if (!headers)
    return;

  if (headers->HasStrongValidators()) {
    // If we don't have strong validators as per RFC 7232 section 2, then
    // we neither store nor use them for range requests.
    if (!headers->EnumerateHeader(nullptr, "Last-Modified",
                                  &create_info->last_modified))
      create_info->last_modified.clear();
    if (!headers->EnumerateHeader(nullptr, "ETag", &create_info->etag))
      create_info->etag.clear();
  }

  // Grab the first content-disposition header.  There may be more than one,
  // though as of this writing, the network stack ensures if there are, they
  // are all duplicates.
  headers->EnumerateHeader(nullptr, "Content-Disposition",
                           &create_info->content_disposition);

  // Parse the original mime type from the header, notice that actual mime type
  // might be different due to mime type sniffing.
  if (!headers->GetMimeType(&create_info->original_mime_type))
    create_info->original_mime_type.clear();

  // Content-Range is validated in HandleSuccessfulServerResponse.
  // In RFC 7233, a single part 206 partial response must generate
  // Content-Range. Accept-Range may be sent in 200 response to indicate the
  // server can handle range request, but optional in 206 response.
  create_info->accept_range =
      headers->HasHeaderValue("Accept-Ranges", "bytes") ||
      (headers->HasHeader("Content-Range") &&
       headers->response_code() == net::HTTP_PARTIAL_CONTENT);
}

std::unique_ptr<network::ResourceRequest> CreateResourceRequest(
    DownloadUrlParameters* params) {
  DCHECK(params->offset() >= 0);

  std::unique_ptr<network::ResourceRequest> request(
      new network::ResourceRequest);
  request->method = params->method();
  request->url = params->url();
  request->request_initiator = params->initiator();
  request->do_not_prompt_for_login = params->do_not_prompt_for_login();
  request->site_for_cookies = params->url();
  request->referrer = params->referrer();
  request->referrer_policy = params->referrer_policy();
  request->allow_download = true;
  request->is_main_frame = true;

  if (params->render_process_host_id() >= 0)
    request->render_frame_id = params->render_frame_host_routing_id();

  bool has_upload_data = false;
  if (params->post_body()) {
    request->request_body = params->post_body();
    has_upload_data = true;
  }

  if (params->post_id() >= 0) {
    // The POST in this case does not have an actual body, and only works
    // when retrieving data from cache. This is done because we don't want
    // to do a re-POST without user consent, and currently don't have a good
    // plan on how to display the UI for that.
    DCHECK(params->prefer_cache());
    DCHECK_EQ("POST", params->method());
    request->request_body = new network::ResourceRequestBody();
    request->request_body->set_identifier(params->post_id());
    has_upload_data = true;
  }

  request->load_flags = GetLoadFlags(params, has_upload_data);

  // Add additional request headers.
  std::unique_ptr<net::HttpRequestHeaders> headers =
      GetAdditionalRequestHeaders(params);
  request->headers.Swap(headers.get());

  return request;
}

int GetLoadFlags(DownloadUrlParameters* params, bool has_upload_data) {
  int load_flags = 0;
  if (params->prefer_cache()) {
    // If there is upload data attached, only retrieve from cache because there
    // is no current mechanism to prompt the user for their consent for a
    // re-post. For GETs, try to retrieve data from the cache and skip
    // validating the entry if present.
    if (has_upload_data)
      load_flags |= net::LOAD_ONLY_FROM_CACHE | net::LOAD_SKIP_CACHE_VALIDATION;
    else
      load_flags |= net::LOAD_SKIP_CACHE_VALIDATION;
  } else {
    load_flags |= net::LOAD_DISABLE_CACHE;
  }
  return load_flags;
}

std::unique_ptr<net::HttpRequestHeaders> GetAdditionalRequestHeaders(
    DownloadUrlParameters* params) {
  auto headers = std::make_unique<net::HttpRequestHeaders>();
  if (params->offset() == 0 &&
      params->length() == DownloadSaveInfo::kLengthFullContent) {
    AppendExtraHeaders(headers.get(), params);
    return headers;
  }

  bool has_last_modified = !params->last_modified().empty();
  bool has_etag = !params->etag().empty();

  // Strong validator(i.e. etag or last modified) is required in range requests
  // for download resumption and parallel download.
  DCHECK(has_etag || has_last_modified);
  if (!has_etag && !has_last_modified) {
    DVLOG(1) << "Creating partial request without strong validators.";
    AppendExtraHeaders(headers.get(), params);
    return headers;
  }

  // Add "Range" header.
  std::string range_header =
      (params->length() == DownloadSaveInfo::kLengthFullContent)
          ? base::StringPrintf("bytes=%" PRId64 "-", params->offset())
          : base::StringPrintf("bytes=%" PRId64 "-%" PRId64, params->offset(),
                               params->offset() + params->length() - 1);
  headers->SetHeader(net::HttpRequestHeaders::kRange, range_header);

  // Add "If-Range" headers.
  if (params->use_if_range()) {
    // In accordance with RFC 7233 Section 3.2, use If-Range to specify that
    // the server return the entire entity if the validator doesn't match.
    // Last-Modified can be used in the absence of ETag as a validator if the
    // response headers satisfied the HttpUtil::HasStrongValidators()
    // predicate.
    //
    // This function assumes that HasStrongValidators() was true and that the
    // ETag and Last-Modified header values supplied are valid.
    headers->SetHeader(net::HttpRequestHeaders::kIfRange,
                       has_etag ? params->etag() : params->last_modified());
    AppendExtraHeaders(headers.get(), params);
    return headers;
  }

  // Add "If-Match"/"If-Unmodified-Since" headers.
  if (has_etag)
    headers->SetHeader(net::HttpRequestHeaders::kIfMatch, params->etag());

  // According to RFC 7232 section 3.4, "If-Unmodified-Since" is mainly for
  // old servers that didn't implement "If-Match" and must be ignored when
  // "If-Match" presents.
  if (has_last_modified) {
    headers->SetHeader(net::HttpRequestHeaders::kIfUnmodifiedSince,
                       params->last_modified());
  }

  AppendExtraHeaders(headers.get(), params);
  return headers;
}

DownloadEntry CreateDownloadEntryFromItem(
    const DownloadItem& item,
    const std::string& request_origin,
    DownloadSource download_source,
    bool fetch_error_body,
    const DownloadUrlParameters::RequestHeadersType& request_headers) {
  return DownloadEntry(item.GetGuid(), request_origin, download_source,
                       fetch_error_body, request_headers,
                       GetUniqueDownloadId());
}

DownloadDBEntry CreateDownloadDBEntryFromItem(
    const DownloadItem& item,
    const UkmInfo& ukm_info,
    bool fetch_error_body,
    const DownloadUrlParameters::RequestHeadersType& request_headers) {
  DownloadDBEntry entry;
  DownloadInfo download_info;
  download_info.guid = item.GetGuid();
  download_info.id = item.GetId();
  InProgressInfo in_progress_info;
  in_progress_info.url_chain = item.GetUrlChain();
  in_progress_info.referrer_url = item.GetReferrerUrl();
  in_progress_info.site_url = item.GetSiteUrl();
  in_progress_info.tab_url = item.GetTabUrl();
  in_progress_info.tab_referrer_url = item.GetTabReferrerUrl();
  in_progress_info.fetch_error_body = fetch_error_body;
  in_progress_info.request_headers = request_headers;
  in_progress_info.etag = item.GetETag();
  in_progress_info.last_modified = item.GetLastModifiedTime();
  in_progress_info.mime_type = item.GetMimeType();
  in_progress_info.original_mime_type = item.GetOriginalMimeType();
  in_progress_info.total_bytes = item.GetTotalBytes();
  in_progress_info.current_path = item.GetFullPath();
  in_progress_info.target_path = item.GetTargetFilePath();
  in_progress_info.received_bytes = item.GetReceivedBytes();
  in_progress_info.start_time = item.GetStartTime();
  in_progress_info.end_time = item.GetEndTime();
  in_progress_info.received_slices = item.GetReceivedSlices();
  in_progress_info.hash = item.GetHash();
  in_progress_info.transient = item.IsTransient();
  in_progress_info.state = item.GetState();
  in_progress_info.danger_type = item.GetDangerType();
  in_progress_info.interrupt_reason = item.GetLastReason();
  in_progress_info.paused = item.IsPaused();
  in_progress_info.bytes_wasted = item.GetBytesWasted();

  download_info.in_progress_info = in_progress_info;

  download_info.ukm_info = ukm_info;
  entry.download_info = download_info;
  return entry;
}

base::Optional<DownloadEntry> CreateDownloadEntryFromDownloadDBEntry(
    base::Optional<DownloadDBEntry> entry) {
  if (!entry || !entry->download_info)
    return base::Optional<DownloadEntry>();

  base::Optional<InProgressInfo> in_progress_info =
      entry->download_info->in_progress_info;
  base::Optional<UkmInfo> ukm_info = entry->download_info->ukm_info;
  if (!ukm_info || !in_progress_info)
    return base::Optional<DownloadEntry>();

  return base::Optional<DownloadEntry>(DownloadEntry(
      entry->download_info->guid, std::string(), ukm_info->download_source,
      in_progress_info->fetch_error_body, in_progress_info->request_headers,
      ukm_info->ukm_download_id));
}

uint64_t GetUniqueDownloadId() {
  // Get a new UKM download_id that is not 0.
  uint64_t download_id = 0;
  do {
    download_id = base::RandUint64();
  } while (download_id == 0);
  return download_id;
}

ResumeMode GetDownloadResumeMode(DownloadInterruptReason reason,
                                 bool restart_required,
                                 bool user_action_required) {
  switch (reason) {
    case DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR:
    case DOWNLOAD_INTERRUPT_REASON_NETWORK_TIMEOUT:
    case DOWNLOAD_INTERRUPT_REASON_SERVER_CONTENT_LENGTH_MISMATCH:
      break;

    case DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE:
    // The server disagreed with the file offset that we sent.

    case DOWNLOAD_INTERRUPT_REASON_FILE_HASH_MISMATCH:
    // The file on disk was found to not match the expected hash. Discard and
    // start from beginning.

    case DOWNLOAD_INTERRUPT_REASON_FILE_TOO_SHORT:
      // The [possibly persisted] file offset disagreed with the file on disk.

      // The intermediate stub is not usable and the server is responding. Hence
      // retrying the request from the beginning is likely to work.
      restart_required = true;
      break;

    case DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED:
    case DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED:
    case DOWNLOAD_INTERRUPT_REASON_NETWORK_SERVER_DOWN:
    case DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED:
    case DOWNLOAD_INTERRUPT_REASON_SERVER_UNREACHABLE:
    case DOWNLOAD_INTERRUPT_REASON_USER_SHUTDOWN:
    case DOWNLOAD_INTERRUPT_REASON_CRASH:
      // It is not clear whether attempting a resumption is acceptable at this
      // time or whether it would work at all. Hence allow the user to retry the
      // download manually.
      user_action_required = true;
      break;

    case DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE:
      // There was no space. Require user interaction so that the user may, for
      // example, choose a different location to store the file. Or they may
      // free up some space on the targret device and retry. But try to reuse
      // the partial stub.
      user_action_required = true;
      break;

    case DOWNLOAD_INTERRUPT_REASON_FILE_FAILED:
    case DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED:
    case DOWNLOAD_INTERRUPT_REASON_FILE_NAME_TOO_LONG:
    case DOWNLOAD_INTERRUPT_REASON_FILE_TOO_LARGE:
      // Assume the partial stub is unusable. Also it may not be possible to
      // restart immediately.
      user_action_required = true;
      restart_required = true;
      break;

    case DOWNLOAD_INTERRUPT_REASON_NONE:
    case DOWNLOAD_INTERRUPT_REASON_NETWORK_INVALID_REQUEST:
    case DOWNLOAD_INTERRUPT_REASON_FILE_VIRUS_INFECTED:
    case DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT:
    case DOWNLOAD_INTERRUPT_REASON_USER_CANCELED:
    case DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED:
    case DOWNLOAD_INTERRUPT_REASON_FILE_SECURITY_CHECK_FAILED:
    case DOWNLOAD_INTERRUPT_REASON_SERVER_UNAUTHORIZED:
    case DOWNLOAD_INTERRUPT_REASON_SERVER_CERT_PROBLEM:
    case DOWNLOAD_INTERRUPT_REASON_SERVER_FORBIDDEN:
    case DOWNLOAD_INTERRUPT_REASON_SERVER_CROSS_ORIGIN_REDIRECT:
    case DOWNLOAD_INTERRUPT_REASON_FILE_SAME_AS_SOURCE:
      return ResumeMode::INVALID;
  }
  if (user_action_required && restart_required)
    return ResumeMode::USER_RESTART;

  if (restart_required)
    return ResumeMode::IMMEDIATE_RESTART;

  if (user_action_required)
    return ResumeMode::USER_CONTINUE;

  return ResumeMode::IMMEDIATE_CONTINUE;
}

}  // namespace download
