// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.h"

#include <algorithm>
#include <limits>
#include <set>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_bypass_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_util.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/lofi_decider.h"
#include "net/base/load_flags.h"
#include "net/base/mime_util.h"
#include "net/base/network_change_notifier.h"
#include "net/base/proxy_server.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/nqe/effective_connection_type.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_status.h"
#include "services/network/public/cpp/features.h"
#include "url/gurl.h"

namespace data_reduction_proxy {

namespace {

// Values of the UMA DataReductionProxy.Protocol.AcceptTransform histogram
// defined in metrics/histograms/histograms.xml. This enum must remain
// synchronized with DataReductionProxyProtocolAcceptTransformEvent in
// tools/metrics/histograms/enums.xml.
enum AcceptTransformEvent {
  LITE_PAGE_REQUESTED = 0,
  LITE_PAGE_TRANSFORM_RECEIVED = 1,
  EMPTY_IMAGE_POLICY_DIRECTIVE_RECEIVED = 2,
  EMPTY_IMAGE_REQUESTED = 3,
  EMPTY_IMAGE_TRANSFORM_RECEIVED = 4,
  COMPRESSED_VIDEO_REQUESTED = 5,
  IDENTITY_TRANSFORM_REQUESTED = 6,
  IDENTITY_TRANSFORM_RECEIVED = 7,
  COMPRESSED_VIDEO_RECEIVED = 8,
  UNKNOWN_TRANSFORM_RECEIVED = 9,
  ACCEPT_TRANSFORM_EVENT_BOUNDARY
};

// Records the occurrence of |sample| in |name| histogram. UMA macros are not
// used because the |name| is not static.
void RecordNewContentLengthHistogram(const std::string& name, int64_t sample) {
  base::UmaHistogramCustomCounts(
      name, sample,
      1,          // Minimum sample size in bytes.
      128 << 20,  // Maximum sample size in bytes. 128MB is chosen because some
                  // video requests can be very large.
      50          // Bucket count.
      );
}

void RecordNewContentLengthHistograms(
    const char* prefix,
    bool is_https,
    bool is_video,
    DataReductionProxyRequestType request_type,
    int64_t content_length) {
  const char* connection_type = is_https ? ".Https" : ".Http";
  const char* suffix = ".Other";
  // TODO(crbug.com/726411): Differentiate between a bypass and a disabled
  // proxy config.
  switch (request_type) {
    case VIA_DATA_REDUCTION_PROXY:
      suffix = ".ViaDRP";
      break;
    case HTTPS:
    case DIRECT_HTTP:
      suffix = ".Direct";
      break;
    case SHORT_BYPASS:
    case LONG_BYPASS:
      suffix = ".BypassedDRP";
      break;
    case UPDATE:
    case UNKNOWN_TYPE:
    default:
      // Value already properly initialized to ".Other"
      break;
  }
  // Record a histogram for all traffic, including video.
  RecordNewContentLengthHistogram(
      base::StringPrintf("%s%s%s", prefix, connection_type, suffix),
      content_length);
  if (is_video) {
    RecordNewContentLengthHistogram(
        base::StringPrintf("%s%s%s.Video", prefix, connection_type, suffix),
        content_length);
  }
}

// |received_content_length| is the number of prefilter bytes received.
// |original_content_length| is the length of resource if accessed directly
// without data saver proxy. |freshness_lifetime| specifies how long the
// resource will be fresh for.
void RecordContentLengthHistograms(bool is_https,
                                   bool is_video,
                                   int64_t received_content_length,
                                   int64_t original_content_length,
                                   const base::TimeDelta& freshness_lifetime,
                                   DataReductionProxyRequestType request_type) {
  // Add the current resource to these histograms only when the content length
  // is valid.
  if (original_content_length >= 0) {
    // This is only used locally in integration testing.
    LOCAL_HISTOGRAM_COUNTS_1000000("Net.HttpOriginalContentLengthWithValidOCL",
                                   original_content_length);
    UMA_HISTOGRAM_COUNTS_1M("Net.HttpContentLengthDifferenceWithValidOCL",
                            original_content_length - received_content_length);
  } else {
    // Presume the original content length is the same as the received content
    // length.
    original_content_length = received_content_length;
  }
  UMA_HISTOGRAM_COUNTS_1M("Net.HttpContentLength", received_content_length);

  // Record the new histograms broken down by HTTP/HTTPS and video/non-video
  RecordNewContentLengthHistograms("Net.HttpContentLengthV2", is_https,
                                   is_video, request_type,
                                   received_content_length);
  RecordNewContentLengthHistograms("Net.HttpOriginalContentLengthV2", is_https,
                                   is_video, request_type,
                                   original_content_length);

  UMA_HISTOGRAM_CUSTOM_COUNTS("Net.HttpContentFreshnessLifetime",
                              freshness_lifetime.InSeconds(),
                              base::TimeDelta::FromHours(1).InSeconds(),
                              base::TimeDelta::FromDays(30).InSeconds(), 100);
}

void RecordAcceptTransformEvent(AcceptTransformEvent event) {
  UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.Protocol.AcceptTransform",
                            event, ACCEPT_TRANSFORM_EVENT_BOUNDARY);
}

void RecordAcceptTransformSentUMA(
    const net::HttpRequestHeaders& request_headers) {
  switch (ParseRequestTransform(request_headers)) {
    case TRANSFORM_LITE_PAGE:
      RecordAcceptTransformEvent(LITE_PAGE_REQUESTED);
      break;
    case TRANSFORM_EMPTY_IMAGE:
      RecordAcceptTransformEvent(EMPTY_IMAGE_REQUESTED);
      break;
    case TRANSFORM_COMPRESSED_VIDEO:
      RecordAcceptTransformEvent(COMPRESSED_VIDEO_REQUESTED);
      break;
    case TRANSFORM_IDENTITY:
      RecordAcceptTransformEvent(IDENTITY_TRANSFORM_REQUESTED);
      break;
    case TRANSFORM_NONE:
      break;
    case TRANSFORM_PAGE_POLICIES_EMPTY_IMAGE:
    case TRANSFORM_UNKNOWN:
      NOTREACHED();
      break;
  }
}

void RecordAcceptTransformReceivedUMA(const net::URLRequest& request) {
  net::HttpResponseHeaders* response_headers = request.response_headers();
  if (!response_headers) {
    return;
  }

  switch (ParseResponseTransform(*response_headers)) {
    case TRANSFORM_UNKNOWN:
      RecordAcceptTransformEvent(UNKNOWN_TRANSFORM_RECEIVED);
      break;
    case TRANSFORM_LITE_PAGE:
      RecordAcceptTransformEvent(LITE_PAGE_TRANSFORM_RECEIVED);
      break;
    case TRANSFORM_PAGE_POLICIES_EMPTY_IMAGE:
      RecordAcceptTransformEvent(EMPTY_IMAGE_POLICY_DIRECTIVE_RECEIVED);
      break;
    case TRANSFORM_EMPTY_IMAGE:
      RecordAcceptTransformEvent(EMPTY_IMAGE_TRANSFORM_RECEIVED);
      break;
    case TRANSFORM_IDENTITY:
      RecordAcceptTransformEvent(IDENTITY_TRANSFORM_RECEIVED);
      break;
    case TRANSFORM_COMPRESSED_VIDEO:
      RecordAcceptTransformEvent(COMPRESSED_VIDEO_RECEIVED);
      break;
    case TRANSFORM_NONE:
      break;
  }
}

// Verifies that the chrome proxy related request headers are set correctly.
// |via_chrome_proxy| is true if the request is being fetched via Chrome Data
// Saver proxy.
void VerifyHttpRequestHeaders(bool via_chrome_proxy,
                              const net::HttpRequestHeaders& headers) {
  // If holdback is enabled, then |via_chrome_proxy| should be false.
  DCHECK(!params::IsIncludedInHoldbackFieldTrial() || !via_chrome_proxy);

  if (via_chrome_proxy) {
    DCHECK(headers.HasHeader(chrome_proxy_ect_header()));
    std::string chrome_proxy_header_value;
    DCHECK(
        headers.GetHeader(chrome_proxy_header(), &chrome_proxy_header_value));
    // Check that only 1 "exp" directive is sent.
    DCHECK_GT(3u, base::SplitStringUsingSubstr(chrome_proxy_header_value,
                                               "exp=", base::TRIM_WHITESPACE,
                                               base::SPLIT_WANT_ALL)
                      .size());
    // Silence unused variable warning in release builds.
    (void)chrome_proxy_header_value;
  } else {
    DCHECK(!headers.HasHeader(chrome_proxy_header()));
    DCHECK(!headers.HasHeader(chrome_proxy_accept_transform_header()));
    DCHECK(!headers.HasHeader(chrome_proxy_ect_header()));
  }
}

// If the response is the entire resource, then the renderer won't show a
// placeholder. This should match the behavior in blink::ImageResource.
bool IsEntireResource(const net::HttpResponseHeaders* response_headers) {
  if (!response_headers || response_headers->response_code() != 206)
    return true;

  int64_t first, last, length;
  return response_headers->GetContentRangeFor206(&first, &last, &length) &&
         first == 0 && last + 1 == length;
}

}  // namespace

DataReductionProxyNetworkDelegate::DataReductionProxyNetworkDelegate(
    std::unique_ptr<net::NetworkDelegate> network_delegate,
    DataReductionProxyConfig* config,
    DataReductionProxyRequestOptions* request_options,
    const DataReductionProxyConfigurator* configurator)
    : LayeredNetworkDelegate(std::move(network_delegate)),
      data_reduction_proxy_config_(config),
      data_reduction_proxy_bypass_stats_(nullptr),
      data_reduction_proxy_request_options_(request_options),
      data_reduction_proxy_io_data_(nullptr),
      configurator_(configurator) {
  DCHECK(data_reduction_proxy_config_);
  DCHECK(data_reduction_proxy_request_options_);
  DCHECK(configurator_);
}

DataReductionProxyNetworkDelegate::~DataReductionProxyNetworkDelegate() {
}

void DataReductionProxyNetworkDelegate::InitIODataAndUMA(
    DataReductionProxyIOData* io_data,
    DataReductionProxyBypassStats* bypass_stats) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(bypass_stats);
  data_reduction_proxy_io_data_ = io_data;
  data_reduction_proxy_bypass_stats_ = bypass_stats;
}

void DataReductionProxyNetworkDelegate::OnBeforeStartTransactionInternal(
    net::URLRequest* request,
    net::HttpRequestHeaders* headers) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!data_reduction_proxy_io_data_)
    return;
  if (!data_reduction_proxy_io_data_->IsEnabled())
    return;

  if (request->url().SchemeIsCryptographic() ||
      !request->url().SchemeIsHTTPOrHTTPS()) {
    return;
  }

  if (data_reduction_proxy_io_data_->resource_type_provider()) {
    // Sets content type of |request| in the resource type provider, so it can
    // be later used for determining the proxy that should be used for fetching
    // |request|.
    data_reduction_proxy_io_data_->resource_type_provider()->SetContentType(
        *request);
  }

  if (data_reduction_proxy_io_data_->lofi_decider()) {
    data_reduction_proxy_io_data_->lofi_decider()
        ->MaybeSetAcceptTransformHeader(*request, headers);
  }

  MaybeAddChromeProxyECTHeader(headers, *request);
}

void DataReductionProxyNetworkDelegate::OnBeforeSendHeadersInternal(
    net::URLRequest* request,
    const net::ProxyInfo& proxy_info,
    const net::ProxyRetryInfoMap& proxy_retry_info,
    net::HttpRequestHeaders* headers) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(data_reduction_proxy_config_);
  DCHECK(request);

  // If there was a redirect or request bypass, use the same page ID for both
  // requests. As long as the session ID has not changed. Re-issued requests
  // and client redirects will be assigned a new page ID as they are different
  // URLRequests.
  DataReductionProxyData* data = DataReductionProxyData::GetData(*request);
  base::Optional<uint64_t> page_id;
  if (data && data->session_key() ==
                  data_reduction_proxy_request_options_->GetSecureSession()) {
    page_id = data->page_id();
  }
  // Always persist data's |request_info| since it tracks connection pingback
  // data for redirects on main frame requests. It should include re-issued
  // requests and client redirects.
  std::vector<DataReductionProxyData::RequestInfo> request_info;
  if (data)
    request_info = data->TakeRequestInfo();

  // Reset |request|'s DataReductionProxyData.
  DataReductionProxyData::ClearData(request);
  data = nullptr;

  bool using_data_reduction_proxy = true;
  // The following checks rule out direct, invalid, and other connection types.
  if (!proxy_info.is_http() && !proxy_info.is_https() &&
      !proxy_info.is_quic()) {
    using_data_reduction_proxy = false;
  } else if (proxy_info.proxy_server().host_port_pair().IsEmpty()) {
    using_data_reduction_proxy = false;
  } else if (!data_reduction_proxy_config_->FindConfiguredDataReductionProxy(
                 proxy_info.proxy_server())) {
    using_data_reduction_proxy = false;
  }

  bool is_holdback_eligible = false;

  if (params::IsIncludedInHoldbackFieldTrial() &&
      WasEligibleWithoutHoldback(*request, proxy_info, proxy_retry_info)) {
    is_holdback_eligible = true;
  }
  // If holdback is enabled, |using_data_reduction_proxy| must be false.
  DCHECK(!params::IsIncludedInHoldbackFieldTrial() ||
         !using_data_reduction_proxy);

  // For the holdback field trial, still log UMA and send the pingback as if
  // the proxy were used.
  if (is_holdback_eligible || using_data_reduction_proxy) {
    // Retrieves DataReductionProxyData from a request, creating a new instance
    // if needed.
    data = DataReductionProxyData::GetDataAndCreateIfNecessary(request);
    data->set_used_data_reduction_proxy(true);
    // Only set GURL, NQE and session key string for main frame requests since
    // they are not needed for sub-resources.
    if (request->load_flags() & net::LOAD_MAIN_FRAME_DEPRECATED) {
      data->set_session_key(
          data_reduction_proxy_request_options_->GetSecureSession());
      data->set_request_url(request->url());
      if (data_reduction_proxy_io_data_) {
        data->set_effective_connection_type(
            data_reduction_proxy_io_data_->GetEffectiveConnectionType());
      }
      data->set_connection_type(
          net::NetworkChangeNotifier::GetConnectionType());
      // Generate a page ID for main frame requests that don't already have one.
      // TODO(ryansturm): remove LOAD_MAIN_FRAME_DEPRECATED from d_r_p.
      // crbug.com/709621
      if (!page_id) {
        page_id = data_reduction_proxy_request_options_->GeneratePageId();
      }
      data->set_page_id(page_id.value());
      data->set_request_info(std::move(request_info));
    }
  }

  LoFiDecider* lofi_decider = nullptr;
  if (data_reduction_proxy_io_data_)
    lofi_decider = data_reduction_proxy_io_data_->lofi_decider();

  // The headers below were speculatively added for caching for HTTP requests
  // when DRP is enabled. Before modifying them, make sure that this is a
  // DRP-eligible request so that the below headers are not removed when they
  // are included by other Chrome or Preview features, currently limited to
  // HTTPS.
  if (request->url().SchemeIsCryptographic())
    return;

  if (!using_data_reduction_proxy) {
    if (lofi_decider) {
      // If not using the data reduction proxy, strip the
      // Chrome-Proxy-Accept-Transform header.
      lofi_decider->RemoveAcceptTransformHeader(headers);
    }
    RemoveChromeProxyECTHeader(headers);
    headers->RemoveHeader(chrome_proxy_header());
    VerifyHttpRequestHeaders(false, *headers);
    return;
  }

  DCHECK(data);
  MaybeAddBrotliToAcceptEncodingHeader(proxy_info, headers, *request);

  data_reduction_proxy_request_options_->AddRequestHeader(headers, page_id);

  VerifyHttpRequestHeaders(true, *headers);
  RecordAcceptTransformSentUMA(*headers);
}

void DataReductionProxyNetworkDelegate::OnBeforeRedirectInternal(
    net::URLRequest* request,
    const GURL& new_location) {
  // Since this is after a redirect response, reset |request|'s
  // DataReductionProxyData, but keep page ID and session.
  // TODO(ryansturm): Change ClearData logic to have persistent and
  // non-persistent (WRT redirects) data.
  // crbug.com/709564
  DataReductionProxyData* data = DataReductionProxyData::GetData(*request);
  base::Optional<uint64_t> page_id;
  if (data && data->session_key() ==
                  data_reduction_proxy_request_options_->GetSecureSession()) {
    page_id = data->page_id();
  }

  // Persist data's |request_info| since it tracks connection pingback data for
  // redirects on main frame requests.
  std::vector<DataReductionProxyData::RequestInfo> request_info;
  if (data)
    request_info = data->TakeRequestInfo();

  DataReductionProxyData::ClearData(request);

  if (page_id) {
    data = DataReductionProxyData::GetDataAndCreateIfNecessary(request);
    data->set_page_id(page_id.value());
    data->set_session_key(
        data_reduction_proxy_request_options_->GetSecureSession());
    data->set_request_info(std::move(request_info));
  }
}

void DataReductionProxyNetworkDelegate::OnCompletedInternal(
    net::URLRequest* request,
    bool started,
    int net_error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(request);
  DCHECK_NE(net::ERR_IO_PENDING, net_error);
  if (data_reduction_proxy_bypass_stats_)
    data_reduction_proxy_bypass_stats_->OnUrlRequestCompleted(request, started,
                                                              net_error);

  net::HttpRequestHeaders request_headers;
  bool server_lofi = request->response_headers() &&
                     IsEmptyImagePreview(*(request->response_headers()));
  bool will_show_client_lofi_placeholder =
      data_reduction_proxy_io_data_ &&
      data_reduction_proxy_io_data_->lofi_decider() &&
      data_reduction_proxy_io_data_->lofi_decider()->IsClientLoFiImageRequest(
          *request) &&
      // If the response contains the entire resource, then the renderer won't
      // show a placeholder for this image, so don't bother triggering an
      // infobar.
      !IsEntireResource(request->response_headers());

  if ((server_lofi || will_show_client_lofi_placeholder) &&
      data_reduction_proxy_io_data_ &&
      data_reduction_proxy_io_data_->lofi_ui_service()) {
    data_reduction_proxy_io_data_->lofi_ui_service()->OnLoFiReponseReceived(
        *request);
  } else if (data_reduction_proxy_io_data_ && request->response_headers() &&
             IsLitePagePreview(*(request->response_headers()))) {
    RecordLitePageTransformationType(LITE_PAGE);
  } else if (request->GetFullRequestHeaders(&request_headers)) {
    // TODO(bengr): transform processing logic should happen elsewhere.
    std::string header_value;
    request_headers.GetHeader(chrome_proxy_accept_transform_header(),
                              &header_value);
    if (header_value == lite_page_directive())
      RecordLitePageTransformationType(NO_TRANSFORMATION_LITE_PAGE_REQUESTED);
  }

  if (!request->response_info().network_accessed ||
      !request->url().SchemeIsHTTPOrHTTPS() ||
      request->GetTotalReceivedBytes() == 0) {
    return;
  }

  DataReductionProxyRequestType request_type = GetDataReductionProxyRequestType(
      *request, configurator_->GetProxyConfig(), *data_reduction_proxy_config_);

  CalculateAndRecordDataUsage(*request, request_type);
  RecordContentLength(*request, request_type,
                      util::CalculateOCLFromOFCL(*request));
  RecordAcceptTransformReceivedUMA(*request);
}

void DataReductionProxyNetworkDelegate::OnHeadersReceivedInternal(
    net::URLRequest* request,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    GURL* allowed_unsafe_redirect_url) {
  if (!original_response_headers ||
      original_response_headers->IsRedirect(nullptr))
    return;

  if (request->was_cached() && request->url().SchemeIsHTTPOrHTTPS() &&
      !request->url().SchemeIsCryptographic() &&
      original_response_headers->HasHeader(chrome_proxy_header())) {
    DataReductionProxyData* data =
        DataReductionProxyData::GetDataAndCreateIfNecessary(request);
    data->set_was_cached_data_reduction_proxy_response(true);
  }

  switch (ParseResponseTransform(*original_response_headers)) {
    case TRANSFORM_LITE_PAGE:
      DataReductionProxyData::GetDataAndCreateIfNecessary(request)
          ->set_lite_page_received(true);
      break;
    case TRANSFORM_PAGE_POLICIES_EMPTY_IMAGE:
      DataReductionProxyData::GetDataAndCreateIfNecessary(request)
          ->set_lofi_policy_received(true);
      break;
    case TRANSFORM_EMPTY_IMAGE:
      DataReductionProxyData::GetDataAndCreateIfNecessary(request)
          ->set_lofi_received(true);
      break;
    case TRANSFORM_IDENTITY:
    case TRANSFORM_COMPRESSED_VIDEO:
    case TRANSFORM_NONE:
    case TRANSFORM_UNKNOWN:
      break;
  }
  if (data_reduction_proxy_io_data_ &&
      data_reduction_proxy_io_data_->lofi_decider() &&
      data_reduction_proxy_io_data_->lofi_decider()->IsClientLoFiImageRequest(
          *request)) {
    DataReductionProxyData* data =
        DataReductionProxyData::GetDataAndCreateIfNecessary(request);
    data->set_client_lofi_requested(true);
  }
}

void DataReductionProxyNetworkDelegate::CalculateAndRecordDataUsage(
    const net::URLRequest& request,
    DataReductionProxyRequestType request_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  int64_t data_used = request.GetTotalReceivedBytes();

  // Estimate how many bytes would have been used if the DataReductionProxy was
  // not used, and record the data usage.
  int64_t original_size = util::EstimateOriginalReceivedBytes(
      request, data_reduction_proxy_io_data_
                   ? data_reduction_proxy_io_data_->lofi_decider()
                   : nullptr);

  std::string mime_type;
  if (request.response_headers())
    request.response_headers()->GetMimeType(&mime_type);

  AccumulateDataUsage(
      data_used, original_size, request_type, mime_type,
      data_use_measurement::DataUseMeasurement::IsUserRequest(
          request.traffic_annotation().unique_id_hash_code),
      data_use_measurement::DataUseMeasurement::GetContentTypeForRequest(
          request),
      request.traffic_annotation().unique_id_hash_code);

  if (params::IsDataSaverSiteBreakdownUsingPLMEnabled() &&
      data_reduction_proxy_io_data_ &&
      data_reduction_proxy_io_data_->resource_type_provider() &&
      data_reduction_proxy_io_data_->resource_type_provider()
          ->IsNonContentInitiatedRequest(request)) {
    // Record non-content initiated traffic to the Other bucket for data saver
    // site-breakdown.
    DCHECK(!base::FeatureList::IsEnabled(network::features::kNetworkService));
    data_reduction_proxy_io_data_->UpdateDataUseForHost(
        data_used, original_size, util::GetSiteBreakdownOtherHostName());
  }
}

void DataReductionProxyNetworkDelegate::AccumulateDataUsage(
    int64_t data_used,
    int64_t original_size,
    DataReductionProxyRequestType request_type,
    const std::string& mime_type,
    bool is_user_traffic,
    data_use_measurement::DataUseUserData::DataUseContentType content_type,
    int32_t service_hash_code) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_GE(data_used, 0);
  DCHECK_GE(original_size, 0);
  if (data_reduction_proxy_io_data_) {
    data_reduction_proxy_io_data_->UpdateContentLengths(
        data_used, original_size, data_reduction_proxy_io_data_->IsEnabled(),
        request_type, mime_type, is_user_traffic, content_type,
        service_hash_code);
  }
}

void DataReductionProxyNetworkDelegate::RecordContentLength(
    const net::URLRequest& request,
    DataReductionProxyRequestType request_type,
    int64_t original_content_length) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!request.response_headers() || request.was_cached() ||
      request.received_response_content_length() == 0) {
    return;
  }

  // Record content length histograms for the request.
  base::TimeDelta freshness_lifetime =
      request.response_headers()
          ->GetFreshnessLifetimes(request.response_info().response_time)
          .freshness;

  bool is_https = request.url().SchemeIs("https");
  bool is_video = false;
  std::string mime_type;
  if (request.response_headers()->GetMimeType(&mime_type)) {
    is_video = net::MatchesMimeType("video/*", mime_type);
  }

  RecordContentLengthHistograms(
      is_https, is_video, request.received_response_content_length(),
      original_content_length, freshness_lifetime, request_type);

  if (data_reduction_proxy_io_data_ && data_reduction_proxy_bypass_stats_) {
    // Record BypassedBytes histograms for the request.
    data_reduction_proxy_bypass_stats_->RecordBypassedBytesHistograms(
        request, data_reduction_proxy_io_data_->IsEnabled(),
        configurator_->GetProxyConfig());
  }
}

void DataReductionProxyNetworkDelegate::RecordLitePageTransformationType(
    LitePageTransformationType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.LoFi.TransformationType", type,
                            LITE_PAGE_TRANSFORMATION_TYPES_INDEX_BOUNDARY);
}

bool DataReductionProxyNetworkDelegate::WasEligibleWithoutHoldback(
    const net::URLRequest& request,
    const net::ProxyInfo& proxy_info,
    const net::ProxyRetryInfoMap& proxy_retry_info) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(proxy_info.is_empty() || proxy_info.is_direct() ||
         !data_reduction_proxy_config_->FindConfiguredDataReductionProxy(
             proxy_info.proxy_server()));
  if (!util::EligibleForDataReductionProxy(proxy_info, request.url(),
                                           request.method())) {
    return false;
  }
  net::ProxyConfig proxy_config =
      data_reduction_proxy_config_->ProxyConfigIgnoringHoldback();
  net::ProxyInfo data_reduction_proxy_info;
  return util::ApplyProxyConfigToProxyInfo(proxy_config, proxy_retry_info,
                                           request.url(),
                                           &data_reduction_proxy_info);
}

void DataReductionProxyNetworkDelegate::MaybeAddBrotliToAcceptEncodingHeader(
    const net::ProxyInfo& proxy_info,
    net::HttpRequestHeaders* request_headers,
    const net::URLRequest& request) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (base::FeatureList::IsEnabled(
          features::kDataReductionProxyBrotliHoldback)) {
    return;
  }

  // This method should be called only when the resolved proxy was a data
  // saver proxy.
  DCHECK(data_reduction_proxy_config_->FindConfiguredDataReductionProxy(
      proxy_info.proxy_server()));
  DCHECK(request.url().is_valid());
  DCHECK(!request.url().SchemeIsCryptographic());
  DCHECK(request.url().SchemeIsHTTPOrHTTPS());

  static const char kBrotli[] = "br";

  if (!request.context()->enable_brotli()) {
    // Verify that Brotli is enabled globally.
    return;
  }

  if (!params::IsBrotliAcceptEncodingEnabled()) {
    // Verify that Brotli is enabled for data reduction proxy.
    return;
  }

  if (!proxy_info.proxy_server().is_https() &&
      !proxy_info.proxy_server().is_quic()) {
    // Brotli encoding can be used only when the proxy server is a secure proxy
    // server.
    return;
  }

  if (!request_headers->HasHeader(net::HttpRequestHeaders::kAcceptEncoding))
    return;

  std::string header_value;
  request_headers->GetHeader(net::HttpRequestHeaders::kAcceptEncoding,
                             &header_value);

  // Only add Brotli to the header if it is not already present.
  std::set<std::string> header_entry_set;
  if (net::HttpUtil::ParseAcceptEncoding(header_value, &header_entry_set) &&
      header_entry_set.find(kBrotli) == header_entry_set.end()) {
    if (!header_value.empty())
      header_value += ", ";
    header_value += kBrotli;
    request_headers->SetHeader(net::HttpRequestHeaders::kAcceptEncoding,
                               header_value);
  }
}

void DataReductionProxyNetworkDelegate::MaybeAddChromeProxyECTHeader(
    net::HttpRequestHeaders* request_headers,
    const net::URLRequest& request) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  // This method should be called only when the resolved proxy was a data
  // saver proxy.
  DCHECK(request.url().is_valid());
  DCHECK(!request.url().SchemeIsCryptographic());
  DCHECK(request.url().SchemeIsHTTPOrHTTPS());

  if (request_headers->HasHeader(chrome_proxy_ect_header()))
    request_headers->RemoveHeader(chrome_proxy_ect_header());

  if (data_reduction_proxy_io_data_) {
    net::EffectiveConnectionType type =
        data_reduction_proxy_io_data_->GetEffectiveConnectionType();
    if (type > net::EFFECTIVE_CONNECTION_TYPE_OFFLINE) {
      DCHECK_NE(net::EFFECTIVE_CONNECTION_TYPE_LAST, type);
      request_headers->SetHeader(chrome_proxy_ect_header(),
                                 net::GetNameForEffectiveConnectionType(type));
      return;
    }
  }
  request_headers->SetHeader(chrome_proxy_ect_header(),
                             net::GetNameForEffectiveConnectionType(
                                 net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN));

  static_assert(net::EFFECTIVE_CONNECTION_TYPE_OFFLINE + 1 ==
                    net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
                "ECT enum value is not handled.");
  static_assert(net::EFFECTIVE_CONNECTION_TYPE_4G + 1 ==
                    net::EFFECTIVE_CONNECTION_TYPE_LAST,
                "ECT enum value is not handled.");
}

void DataReductionProxyNetworkDelegate::RemoveChromeProxyECTHeader(
    net::HttpRequestHeaders* request_headers) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  request_headers->RemoveHeader(chrome_proxy_ect_header());
}

}  // namespace data_reduction_proxy
