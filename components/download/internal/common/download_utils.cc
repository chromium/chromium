// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_utils.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/i18n/file_util_icu.h"
#include "base/metrics/field_trial_params.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/download/public/common/download_create_info.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/download_interrupt_reasons_utils.h"
#include "components/download/public/common/download_save_info.h"
#include "components/download/public/common/download_stats.h"
#include "components/download/public/common/download_task_runner.h"
#include "components/download/public/common/download_url_parameters.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_content_disposition.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/download/internal/common/android/download_collection_bridge.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace download {

namespace {
// Default value for |kDownloadContentValidationLengthFinchKey|, when no
// parameter is specified.
const int64_t kDefaultContentValidationLength = 1024;

// If the file_offset value from SaveInfo is equal to this, no content
// validation will be performed and download stream will be written to
// file starting at the offset from the response.
const int64_t kInvalidFileWriteOffset = -1;

// Default expiration time of download in days. Canceled and interrupted
// downloads will be deleted after expiration.
const int kDefaultDownloadExpiredTimeInDays = 90;

// Default time for an overwritten download to be removed from the history.
const int kDefaultOverwrittenDownloadExpiredTimeInDays = 90;

// Default buffer size in bytes to write to the download file.
const int kDefaultDownloadFileBufferSize = 524288;

#if BUILDFLAG(IS_ANDROID)
// Default maximum length of a downloaded file name on Android.
const int kDefaultMaxFileNameLengthOnAndroid = 127;

DownloadItem::DownloadRenameResult RenameDownloadedFileForContentUri(
    const base::FilePath& from_path,
    const base::FilePath& display_name) {
  if (static_cast<int>(display_name.value().length()) >
      kDefaultMaxFileNameLengthOnAndroid) {
    return DownloadItem::DownloadRenameResult::FAILURE_NAME_TOO_LONG;
  }

  if (DownloadCollectionBridge::FileNameExists(display_name))
    return DownloadItem::DownloadRenameResult::FAILURE_NAME_CONFLICT;

  return DownloadCollectionBridge::RenameDownloadUri(from_path, display_name)
             ? DownloadItem::DownloadRenameResult::SUCCESS
             : DownloadItem::DownloadRenameResult::FAILURE_NAME_INVALID;
}
#endif  // BUILDFLAG(IS_ANDROID)

void AppendExtraHeaders(net::HttpRequestHeaders* headers,
                        DownloadUrlParameters* params) {
  // Some headers like "Range" or "If-Ranage", etc are managed by download
  // system, which might be ignored when adding to the actual request.
  // TODO(xingliu): Print out the conflict headers here.
  for (const auto& header : params->request_headers())
    headers->SetHeaderIfMissing(header.first, header.second);
}

// Return whether the download is explicitly to fetch part of the file.
bool IsArbitraryRangeRequest(DownloadSaveInfo* save_info) {
  return save_info && save_info->IsArbitraryRangeRequest();
}

bool IsArbitraryRangeRequest(DownloadUrlParameters* parameters) {
  DCHECK(parameters);
  auto offsets = parameters->range_request_offset();
  return offsets.first != kInvalidRange || offsets.second != kInvalidRange;
}

void AppendRangeHeader(net::HttpRequestHeaders* headers,
                       DownloadUrlParameters* params) {
  std::string range_header =
      base::StringPrintf("bytes=%" PRId64 "-", params->offset());

  if (IsArbitraryRangeRequest(params)) {
    DCHECK(!params->use_if_range());
    auto range_offsets = params->range_request_offset();
    std::string range_from, range_to;
    DCHECK_GE(params->offset(), 0);
    if (range_offsets.first != kInvalidRange) {
      // Have a starting byte in the range request.
      range_from = base::NumberToString(range_offsets.first + params->offset());
      range_to = range_offsets.second != kInvalidRange
                     ? base::NumberToString(range_offsets.second)
                     : "";
    } else {
      // Have no starting byte, trying to fetch the last x bytes.
      DCHECK_NE(range_offsets.second, kInvalidRange);
      DCHECK_GE(range_offsets.second, params->offset())
          << "All the bytes have been fetched.";
      range_to = base::NumberToString(range_offsets.second - params->offset());
    }
    range_header = "bytes=" + range_from + "-" + range_to;
  }

  headers->SetHeader(net::HttpRequestHeaders::kRange, range_header);
}

#if BUILDFLAG(IS_ANDROID)
struct CreateIntermediateUriResult {
 public:
  CreateIntermediateUriResult(const base::FilePath& content_uri,
                              const base::FilePath& file_name)
      : content_uri(content_uri), file_name(file_name) {}

  base::FilePath content_uri;
  base::FilePath file_name;
};

CreateIntermediateUriResult CreateIntermediateUri(
    const GURL& original_url,
    const GURL& referrer_url,
    const base::FilePath& current_path,
    const base::FilePath& suggested_name,
    const std::string& mime_type) {
  base::FilePath content_path =
      current_path.IsContentUri() && base::PathExists(current_path)
          ? current_path
          : DownloadCollectionBridge::CreateIntermediateUriForPublish(
                original_url, referrer_url, suggested_name, mime_type);
  base::FilePath file_name;
  if (!content_path.empty()) {
    file_name = DownloadCollectionBridge::GetDisplayName(content_path);
  }
  if (file_name.empty())
    file_name = suggested_name;
  return CreateIntermediateUriResult(content_path, file_name);
}

void OnInterMediateUriCreated(LocalPathCallback callback,
                              const CreateIntermediateUriResult& result) {
  std::move(callback).Run(result.content_uri, result.file_name);
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

const uint32_t DownloadItem::kInvalidId = 0;

DownloadInterruptReason HandleRequestCompletionStatus(
    net::Error error_code,
    bool has_strong_validators,
    net::CertStatus cert_status,
    bool is_partial_request,
    DownloadInterruptReason abort_reason) {
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

  // For some servers, a range request could cause the server to send
  // wrongly encoded content and cause decoding failures. Restart the download
  // in that case.
  if (is_partial_request && error_code == net::ERR_CONTENT_DECODING_FAILED)
    return DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE;

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

  // Handle normal errors which are not related to range requests.
  if (result != DOWNLOAD_INTERRUPT_REASON_NONE && !fetch_error_body)
    return result;

  int64_t first_byte = -1;
  int64_t last_byte = -1;
  int64_t length = -1;

  // Explicitly range request.
  if (IsArbitraryRangeRequest(save_info)) {
    // Only 206 response is allowed.
    if (http_headers.response_code() != net::HTTP_PARTIAL_CONTENT) {
      return DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT;
    }

    // Must has valid range response header.
    if (!http_headers.GetContentRangeFor206(&first_byte, &last_byte, &length)) {
      return DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT;
    }

    return DOWNLOAD_INTERRUPT_REASON_NONE;
  }

  // The caller is expecting a partial response.
  if (save_info && save_info->offset > 0) {
    if (http_headers.response_code() != net::HTTP_PARTIAL_CONTENT) {
      // Requested a partial range, but received the entire response, when
      // the range request header is "Range:bytes={offset}-".
      // The response can be HTTP 200 or other error code when
      // |fetch_error_body| is true.
      save_info->offset = 0;
      save_info->file_offset = kInvalidFileWriteOffset;
      save_info->hash_of_partial_file.clear();
      save_info->hash_state.reset();
      return DOWNLOAD_INTERRUPT_REASON_NONE;
    }

    if (!http_headers.GetContentRangeFor206(&first_byte, &last_byte, &length))
      return DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT;
    DCHECK_GE(first_byte, 0);

    if (first_byte != save_info->offset) {
      // The server returned a different range than the one we requested. Assume
      // the server doesn't support range request.
      return DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE;
    }

    return DOWNLOAD_INTERRUPT_REASON_NONE;
  }

  // For non range request.
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
    std::optional<std::string_view> last_modified =
        headers->EnumerateHeader(nullptr, "Last-Modified");
    create_info->last_modified = last_modified.value_or(std::string_view());

    std::optional<std::string_view> etag =
        headers->EnumerateHeader(nullptr, "ETag");
    create_info->etag = etag.value_or(std::string_view());
  }

  // Grab the first content-disposition header.  There may be more than one,
  // though as of this writing, the network stack ensures if there are, they
  // are all duplicates.
  std::optional<std::string_view> content_disposition =
      headers->EnumerateHeader(nullptr, "Content-Disposition");
  create_info->content_disposition =
      content_disposition.value_or(std::string_view());

  // Parse the original mime type from the header, notice that actual mime type
  // might be different due to mime type sniffing.
  if (!headers->GetMimeType(&create_info->original_mime_type))
    create_info->original_mime_type.clear();

  // Content-Range is validated in HandleSuccessfulServerResponse.
  // In RFC 7233, a single part 206 partial response must generate
  // Content-Range. Accept-Range may be sent in 200 response to indicate the
  // server can handle range request, but optional in 206 response.
  if (headers->HasHeaderValue("Accept-Ranges", "bytes") ||
      (headers->HasHeader("Content-Range") &&
       headers->response_code() == net::HTTP_PARTIAL_CONTENT)) {
    create_info->accept_range = RangeRequestSupportType::kSupport;
  } else if (headers->HasHeaderValue("Accept-Ranges", "none")) {
    create_info->accept_range = RangeRequestSupportType::kNoSupport;
  } else {
    create_info->accept_range = RangeRequestSupportType::kUnknown;
  }
}

std::unique_ptr<network::ResourceRequest> CreateResourceRequest(
    DownloadUrlParameters* params) {
  DCHECK_GE(params->offset(), 0);

  std::unique_ptr<network::ResourceRequest> request(
      new network::ResourceRequest);
  request->method = params->method();
  request->url = params->url();
  request->request_initiator = params->initiator();
  request->trusted_params = network::ResourceRequest::TrustedParams();
  request->has_user_gesture = params->has_user_gesture();

  if (params->isolation_info().has_value()) {
    request->trusted_params->isolation_info = params->isolation_info().value();
    request->site_for_cookies = params->isolation_info()->site_for_cookies();
  } else {
    // Treat downloads like top-level frame navigations to be consistent with
    // cookie behavior. Also, since web-initiated downloads bypass the disk
    // cache, sites can't use download timing information to tell if a
    // cross-site URL has been visited before.
    url::Origin origin = url::Origin::Create(params->url());
    request->trusted_params->isolation_info = net::IsolationInfo::Create(
        params->update_first_party_url_on_redirect()
            ? net::IsolationInfo::RequestType::kMainFrame
            : net::IsolationInfo::RequestType::kOther,
        origin, origin, net::SiteForCookies::FromOrigin(origin));
    request->site_for_cookies = net::SiteForCookies::FromUrl(params->url());
  }

  request->do_not_prompt_for_login = params->do_not_prompt_for_login();
  request->referrer = params->referrer();
  request->referrer_policy = params->referrer_policy();
  request->is_outermost_main_frame = true;
  request->update_first_party_url_on_redirect =
      params->update_first_party_url_on_redirect();

  // Downloads should be treated as navigations from Fetch spec perspective.
  // See also:
  // - https://crbug.com/952834
  // - https://github.com/whatwg/fetch/issues/896#issuecomment-484423278
  request->mode = network::mojom::RequestMode::kNavigate;

  bool has_upload_data = false;
  if (params->post_body()) {
    request->request_body = params->post_body();
    request->enable_upload_progress = !params->upload_callback().is_null();
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

  if (params->offset() == 0 && !IsArbitraryRangeRequest(params)) {
    AppendExtraHeaders(headers.get(), params);
    return headers;
  }

  bool has_last_modified = !params->last_modified().empty();
  bool has_etag = !params->etag().empty();

  // Strong validator(i.e. etag or last modified) is required in range requests
  // for download resumption and parallel download, unless
  // |kAllowDownloadResumptionWithoutStrongValidators| is enabled.
  // For arbitrary range request, always allow to send range headers.
  bool allow_resumption =
      has_etag || has_last_modified ||
      base::FeatureList::IsEnabled(
          features::kAllowDownloadResumptionWithoutStrongValidators);
  if (!allow_resumption && !IsArbitraryRangeRequest(params)) {
    DVLOG(1) << "Creating partial request without strong validators.";
    AppendExtraHeaders(headers.get(), params);
    return headers;
  }

  // Add "Range" header.
  AppendRangeHeader(headers.get(), params);

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

DownloadDBEntry CreateDownloadDBEntryFromItem(const DownloadItemImpl& item) {
  DownloadDBEntry entry;
  DownloadInfo download_info;
  download_info.guid = item.GetGuid();
  download_info.id = item.GetId();
  InProgressInfo in_progress_info;
  in_progress_info.url_chain = item.GetUrlChain();
  in_progress_info.referrer_url = item.GetReferrerUrl();
  in_progress_info.serialized_embedder_download_data =
      item.GetSerializedEmbedderDownloadData();
  in_progress_info.tab_url = item.GetTabUrl();
  in_progress_info.tab_referrer_url = item.GetTabReferrerUrl();
  in_progress_info.fetch_error_body = item.fetch_error_body();
  in_progress_info.request_headers = item.request_headers();
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
  in_progress_info.metered = item.AllowMetered();
  in_progress_info.bytes_wasted = item.GetBytesWasted();
  in_progress_info.auto_resume_count = item.GetAutoResumeCount();
  in_progress_info.credentials_mode = item.GetCredentialsMode();
  auto range_request_offset = item.GetRangeRequestOffset();
  in_progress_info.range_request_from = range_request_offset.first;
  in_progress_info.range_request_to = range_request_offset.second;

  download_info.in_progress_info = std::move(in_progress_info);

  download_info.ukm_info =
      UkmInfo(item.GetDownloadSource(), item.ukm_download_id());
  entry.download_info = std::move(download_info);
  return entry;
}

std::unique_ptr<DownloadEntry> CreateDownloadEntryFromDownloadDBEntry(
    std::optional<DownloadDBEntry> entry) {
  if (!entry || !entry->download_info)
    return nullptr;

  std::optional<InProgressInfo> in_progress_info =
      entry->download_info->in_progress_info;
  std::optional<UkmInfo> ukm_info = entry->download_info->ukm_info;
  if (!ukm_info || !in_progress_info)
    return nullptr;

  return std::make_unique<DownloadEntry>(
      entry->download_info->guid, std::string(), ukm_info->download_source,
      in_progress_info->fetch_error_body, in_progress_info->request_headers,
      ukm_info->ukm_download_id);
}

uint64_t GetUniqueDownloadId() {
  // Get a new UKM download_id that is not 0.
  uint64_t download_id = 0;
  do {
    download_id = base::RandUint64();
  } while (download_id == 0);
  return download_id;
}

ResumeMode GetDownloadResumeMode(const GURL& url,
                                 DownloadInterruptReason reason,
                                 bool restart_required,
                                 bool user_action_required) {
  // Only support resumption for HTTP(S).
  if (!url.SchemeIsHTTPOrHTTPS())
    return ResumeMode::INVALID;

  switch (reason) {
    case DOWNLOAD_INTERRUPT_REASON_NETWORK_TIMEOUT:
#if BUILDFLAG(IS_ANDROID)
      // If resume mode is USER_CONTINUE, android can still resume
      // the download automatically if we didn't reach the auto resumption
      // limit and the interruption was due to network related reasons.
      user_action_required = true;
      break;
#endif
    case DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR:
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
    case DOWNLOAD_INTERRUPT_REASON_USER_SHUTDOWN:
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

bool IsDownloadDone(const GURL& url,
                    DownloadItem::DownloadState state,
                    DownloadInterruptReason reason) {
  switch (state) {
    case DownloadItem::IN_PROGRESS:
      return false;
    case DownloadItem::COMPLETE:
      [[fallthrough]];
    case DownloadItem::CANCELLED:
      return true;
    case DownloadItem::INTERRUPTED:
      return GetDownloadResumeMode(url, reason, false /* restart_required */,
                                   false /* user_action_required */) ==
             ResumeMode::INVALID;
    default:
      return false;
  }
}

bool DeleteDownloadedFile(const base::FilePath& path) {
  DCHECK(GetDownloadTaskRunner()->RunsTasksInCurrentSequence());
  // Make sure we only delete files.
  if (base::DirectoryExists(path))
    return true;
  return base::DeleteFile(path);
}

DownloadItem::DownloadRenameResult RenameDownloadedFile(
    const base::FilePath& from_path,
    const base::FilePath& display_name) {
#if BUILDFLAG(IS_ANDROID)
  if (from_path.IsContentUri())
    return RenameDownloadedFileForContentUri(from_path, display_name);
#endif  // BUILDFLAG(IS_ANDROID)
  auto to_path = base::FilePath(from_path.DirName()).Append(display_name);
  if (!base::PathExists(from_path) ||
      !base::DirectoryExists(from_path.DirName()))
    return DownloadItem::DownloadRenameResult::FAILURE_UNAVAILABLE;

  if (!base::DirectoryExists(to_path.DirName()))
    return DownloadItem::DownloadRenameResult::FAILURE_NAME_INVALID;

  if (base::PathExists(to_path))
    return DownloadItem::DownloadRenameResult::FAILURE_NAME_CONFLICT;

  int max_path_component_length =
      base::GetMaximumPathComponentLength(to_path.DirName());
  if (max_path_component_length != -1) {
    if (static_cast<int>(to_path.value().length()) >=
        max_path_component_length) {
      return DownloadItem::DownloadRenameResult::FAILURE_NAME_TOO_LONG;
    }
  }
  return base::Move(from_path, to_path)
             ? DownloadItem::DownloadRenameResult::SUCCESS
             : DownloadItem::DownloadRenameResult::FAILURE_NAME_INVALID;
}

int64_t GetDownloadValidationLengthConfig() {
  std::string finch_value = base::GetFieldTrialParamValueByFeature(
      features::kAllowDownloadResumptionWithoutStrongValidators,
      kDownloadContentValidationLengthFinchKey);
  int64_t result;
  return base::StringToInt64(finch_value, &result)
             ? result
             : kDefaultContentValidationLength;
}

base::TimeDelta GetExpiredDownloadDeleteTime() {
  int expired_days = base::GetFieldTrialParamByFeatureAsInt(
      features::kDeleteExpiredDownloads, kExpiredDownloadDeleteTimeFinchKey,
      kDefaultDownloadExpiredTimeInDays);
  return base::Days(expired_days);
}

base::TimeDelta GetOverwrittenDownloadDeleteTime() {
  int expired_days = base::GetFieldTrialParamByFeatureAsInt(
      features::kDeleteOverwrittenDownloads,
      kOverwrittenDownloadDeleteTimeFinchKey,
      kDefaultOverwrittenDownloadExpiredTimeInDays);
  return base::Days(expired_days);
}

size_t GetDownloadFileBufferSize() {
  return base::checked_cast<size_t>(base::GetFieldTrialParamByFeatureAsInt(
      features::kAllowFileBufferSizeControl, kDownloadFileBufferSizeFinchKey,
      kDefaultDownloadFileBufferSize));
}

void DetermineLocalPath(DownloadItem* download,
                        const base::FilePath& virtual_path,
                        LocalPathCallback callback) {
#if BUILDFLAG(IS_ANDROID)
  if ((!download->IsTransient() &&
       DownloadCollectionBridge::ShouldPublishDownload(virtual_path)) ||
      virtual_path.IsContentUri()) {
    GetDownloadTaskRunner()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&CreateIntermediateUri,
                       // Safe because we control download file lifetime.
                       download->GetOriginalUrl(), download->GetReferrerUrl(),
                       virtual_path,
                       virtual_path.IsContentUri()
                           ? download->GetFileNameToReportUser()
                           : virtual_path.BaseName(),
                       download->GetMimeType()),
        base::BindOnce(&OnInterMediateUriCreated, std::move(callback)));
    return;
  }
#endif  // BUILDFLAG(IS_ANDROID)
  std::move(callback).Run(virtual_path, base::FilePath());
}

bool IsInterruptedDownloadAutoResumable(download::DownloadItem* download_item,
                                        int auto_resumption_size_limit) {
  DCHECK_EQ(download::DownloadItem::INTERRUPTED, download_item->GetState());
  if (download_item->IsDangerous()) {
    return false;
  }

  if (!download_item->GetURL().SchemeIsHTTPOrHTTPS()) {
    return false;
  }

  if (download_item->GetBytesWasted() > auto_resumption_size_limit) {
    return false;
  }

  if (download_item->GetTargetFilePath().empty()) {
    return false;
  }

  // TODO(shaktisahu): Use DownloadItemImpl::kMaxAutoResumeAttempts.
  if (download_item->GetAutoResumeCount() >= 5) {
    return false;
  }

  int interrupt_reason = download_item->GetLastReason();
  DCHECK_NE(interrupt_reason, download::DOWNLOAD_INTERRUPT_REASON_NONE);
  return interrupt_reason ==
             download::DOWNLOAD_INTERRUPT_REASON_NETWORK_TIMEOUT ||
         interrupt_reason ==
             download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED ||
         interrupt_reason ==
             download::DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED ||
         interrupt_reason == download::DOWNLOAD_INTERRUPT_REASON_CRASH;
}

bool IsContentDispositionAttachmentInHead(
    const network::mojom::URLResponseHead& response_head) {
  if (!response_head.headers) {
    return false;
  }
  std::string disposition;
  response_head.headers->GetNormalizedHeader("content-disposition",
                                             &disposition);
  return !disposition.empty() &&
         net::HttpContentDisposition(disposition, std::string())
             .is_attachment();
}

}  // namespace download
