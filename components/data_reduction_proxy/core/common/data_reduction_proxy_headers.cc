// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <utility>

#include "base/metrics/field_trial_params.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

const char kChromeProxyHeader[] = "chrome-proxy";
const char kDataReductionPassThroughHeader[] =
    "Chrome-Proxy-Accept-Transform: identity\r\nCache-Control: "
    "no-cache\r\n\r\n";
const char kChromeProxyECTHeader[] = "chrome-proxy-ect";
const char kChromeProxyAcceptTransformHeader[] =
    "chrome-proxy-accept-transform";
const char kChromeProxyContentTransformHeader[] =
    "chrome-proxy-content-transform";

const char kActionValueDelimiter = '=';

// Previews directives.
const char kLitePageDirective[] = "lite-page";
const char kCompressedVideoDirective[] = "compressed-video";
const char kIdentityDirective[] = "identity";
const char kChromeProxyPagePoliciesDirective[] = "page-policies";

const char kChromeProxyActionBlockOnce[] = "block-once";
const char kChromeProxyActionBlock[] = "block";
const char kChromeProxyActionBypass[] = "bypass";

const int kShortBypassMaxSeconds = 59;
const int kMediumBypassMaxSeconds = 300;

base::TimeDelta GetRandomBypassTime(base::TimeDelta min_time,
                                    base::TimeDelta max_time) {
  const int64_t delta_ms =
      base::RandInt(min_time.InMilliseconds(), max_time.InMilliseconds());
  return base::TimeDelta::FromMilliseconds(delta_ms);
}

// Returns a random bypass duration between 1 and 5 minutes.
base::TimeDelta GetDefaultBypassDuration() {
  return GetRandomBypassTime(base::TimeDelta::FromMinutes(1),
                             base::TimeDelta::FromMinutes(5));
}

bool StartsWithActionPrefix(base::StringPiece header_value,
                            base::StringPiece action_prefix) {
  DCHECK(!action_prefix.empty());
  // A valid action does not include a trailing '='.
  DCHECK(action_prefix.back() != kActionValueDelimiter);

  return header_value.size() > action_prefix.size() + 1 &&
         header_value[action_prefix.size()] == kActionValueDelimiter &&
         base::StartsWith(header_value, action_prefix,
                          base::CompareCase::INSENSITIVE_ASCII);
}

// Returns true if the provided transform type is specified in the provided
// Chrome-Proxy-Content-Transform header value.
bool IsPreviewTypeInHeaderValue(base::StringPiece header_value,
                                base::StringPiece transform_type) {
  DCHECK_EQ(transform_type, base::ToLowerASCII(transform_type));

  // The Chrome-Proxy-Content-Transform header consists of a single
  // transformation type string followed by zero or more semicolon-delimited
  // options, e.g. "empty-image", "empty-image;foo-option".
  base::StringPiece token = base::TrimWhitespaceASCII(
      header_value.substr(0, header_value.find(';')), base::TRIM_ALL);
  return base::LowerCaseEqualsASCII(token, transform_type);
}

// Returns true if the provided transform type is specified in the
// Chrome-Proxy-Content-Transform-Header.
bool IsPreviewType(const net::HttpResponseHeaders& headers,
                   base::StringPiece transform_type) {
  std::string value;
  return headers.EnumerateHeader(nullptr, kChromeProxyContentTransformHeader,
                                 &value) &&
         IsPreviewTypeInHeaderValue(value, transform_type);
}

}  // namespace

namespace data_reduction_proxy {

const char* chrome_proxy_header() {
  return kChromeProxyHeader;
}

const char* chrome_proxy_pass_through_header() {
  return kDataReductionPassThroughHeader;
}

const char* chrome_proxy_ect_header() {
  return kChromeProxyECTHeader;
}

const char* chrome_proxy_accept_transform_header() {
  return kChromeProxyAcceptTransformHeader;
}

const char* chrome_proxy_content_transform_header() {
  return kChromeProxyContentTransformHeader;
}

const char* lite_page_directive() {
  return kLitePageDirective;
}

const char* compressed_video_directive() {
  return kCompressedVideoDirective;
}

const char* page_policies_directive() {
  return kChromeProxyPagePoliciesDirective;
}

bool HasURLRedirectCycle(const std::vector<GURL>& url_chain) {
  if (url_chain.size() <= 1)
    return false;

  // If the last entry occurs earlier in the |url_chain|, then very likely there
  // is a redirect cycle.
  return std::find(url_chain.rbegin() + 1, url_chain.rend(),
                   url_chain.back()) != url_chain.rend();
}

TransformDirective ParseRequestTransform(
    const net::HttpRequestHeaders& headers) {
  std::string accept_transform_value;
  if (!headers.GetHeader(chrome_proxy_accept_transform_header(),
                         &accept_transform_value)) {
    return TRANSFORM_NONE;
  }

  if (base::LowerCaseEqualsASCII(accept_transform_value,
                                 lite_page_directive())) {
    return TRANSFORM_LITE_PAGE;
  }
  if (base::LowerCaseEqualsASCII(accept_transform_value,
                                 compressed_video_directive())) {
    return TRANSFORM_COMPRESSED_VIDEO;
  }
  if (base::LowerCaseEqualsASCII(accept_transform_value, kIdentityDirective)) {
    return TRANSFORM_IDENTITY;
  }

  return TRANSFORM_NONE;
}

TransformDirective ParseResponseTransform(
    const net::HttpResponseHeaders& headers) {
  std::string content_transform_value;
  if (!headers.GetNormalizedHeader(chrome_proxy_content_transform_header(),
                                   &content_transform_value)) {
    return TRANSFORM_NONE;
  }
  if (base::LowerCaseEqualsASCII(content_transform_value,
                                 lite_page_directive())) {
    return TRANSFORM_LITE_PAGE;
  }
  if (base::LowerCaseEqualsASCII(content_transform_value, kIdentityDirective)) {
    return TRANSFORM_IDENTITY;
  }
  if (base::LowerCaseEqualsASCII(content_transform_value,
                                 compressed_video_directive())) {
    return TRANSFORM_COMPRESSED_VIDEO;
  }
  return TRANSFORM_UNKNOWN;
}

bool IsLitePagePreview(const net::HttpResponseHeaders& headers) {
  return IsPreviewType(headers, kLitePageDirective);
}

bool GetDataReductionProxyActionValue(const net::HttpResponseHeaders* headers,
                                      base::StringPiece action_prefix,
                                      std::string* action_value) {
  DCHECK(headers);
  size_t iter = 0;
  std::string value;

  while (headers->EnumerateHeader(&iter, kChromeProxyHeader, &value)) {
    if (StartsWithActionPrefix(value, action_prefix)) {
      if (action_value)
        *action_value = value.substr(action_prefix.size() + 1);
      return true;
    }
  }
  return false;
}

bool ParseHeadersAndSetBypassDuration(const net::HttpResponseHeaders& headers,
                                      base::StringPiece action_prefix,
                                      base::TimeDelta* bypass_duration) {
  size_t iter = 0;
  std::string value;

  while (headers.EnumerateHeader(&iter, kChromeProxyHeader, &value)) {
    if (StartsWithActionPrefix(value, action_prefix)) {
      int64_t seconds;
      if (!base::StringToInt64(
              base::StringPiece(value).substr(action_prefix.size() + 1),
              &seconds) ||
          seconds < 0) {
        continue;  // In case there is a well formed instruction.
      }
      if (seconds != 0) {
        *bypass_duration = base::TimeDelta::FromSeconds(seconds);
      } else {
        // Server deferred to us to choose a duration. Default to a random
        // duration between one and five minutes.
        *bypass_duration = GetDefaultBypassDuration();
      }
      return true;
    }
  }
  return false;
}

bool ParseHeadersForBypassInfo(const net::HttpResponseHeaders& headers,
                               DataReductionProxyInfo* proxy_info) {
  DCHECK(proxy_info);

  // Support header of the form Chrome-Proxy: bypass|block=<duration>, where
  // <duration> is the number of seconds to wait before retrying
  // the proxy. If the duration is 0, then the default proxy retry delay
  // (specified in |ProxyList::UpdateRetryInfoOnFallback|) will be used.
  // 'bypass' instructs Chrome to bypass the currently connected data reduction
  // proxy, whereas 'block' instructs Chrome to bypass all available data
  // reduction proxies.

  // 'block' takes precedence over 'bypass' and 'block-once', so look for it
  // first.
  // TODO(bengr): Reduce checks for 'block' and 'bypass' to a single loop.
  if (ParseHeadersAndSetBypassDuration(
          headers, kChromeProxyActionBlock, &proxy_info->bypass_duration)) {
    proxy_info->bypass_all = true;
    proxy_info->mark_proxies_as_bad = true;
    return true;
  }

  // Next, look for 'bypass'.
  if (ParseHeadersAndSetBypassDuration(
          headers, kChromeProxyActionBypass, &proxy_info->bypass_duration)) {
    proxy_info->bypass_all = false;
    proxy_info->mark_proxies_as_bad = true;
    return true;
  }

  // Lastly, look for 'block-once'. 'block-once' instructs Chrome to retry the
  // current request (if it's idempotent), bypassing all available data
  // reduction proxies. Unlike 'block', 'block-once' does not cause data
  // reduction proxies to be bypassed for an extended period of time;
  // 'block-once' only affects the retry of the current request.
  if (headers.HasHeaderValue(kChromeProxyHeader, kChromeProxyActionBlockOnce)) {
    proxy_info->bypass_all = true;
    proxy_info->mark_proxies_as_bad = false;
    proxy_info->bypass_duration = base::TimeDelta();
    return true;
  }

  return false;
}

bool HasDataReductionProxyViaHeader(const net::HttpResponseHeaders& headers,
                                    bool* has_intermediary) {
  static const size_t kVersionSize = 4;
  static const char kDataReductionProxyViaValue[] = "Chrome-Compression-Proxy";
  size_t iter = 0;
  std::string value;

  // Case-sensitive comparison of |value|. Assumes the received protocol and the
  // space following it are always |kVersionSize| characters. E.g.,
  // 'Via: 1.1 Chrome-Compression-Proxy'
  while (headers.EnumerateHeader(&iter, "via", &value)) {
    if (base::StringPiece(value).substr(
            kVersionSize, base::size(kDataReductionProxyViaValue) - 1) ==
        kDataReductionProxyViaValue) {
      if (has_intermediary)
        // We assume intermediary exists if there is another Via header after
        // the data reduction proxy's Via header.
        *has_intermediary = !(headers.EnumerateHeader(&iter, "via", &value));
      return true;
    }
  }

  return false;
}

DataReductionProxyBypassType GetDataReductionProxyBypassType(
    const std::vector<GURL>& url_chain,
    const net::HttpResponseHeaders& headers,
    DataReductionProxyInfo* data_reduction_proxy_info) {
  DCHECK(data_reduction_proxy_info);

  // Responses from the warmup URL probe should not be checked for bypass types.
  // Doing so may unnecessarily cause all data saver proxies to be marked as
  // bad (e.g., when via header is missing on the response to the probe from the
  // HTTP proxy).
  DCHECK(url_chain.empty() || (params::GetWarmupURL() != url_chain.back()));

  bool has_via_header = HasDataReductionProxyViaHeader(headers, nullptr);

  // The following logic to detect redirect cycles works only when network
  // servicification is disabled.
  if (!base::FeatureList::IsEnabled(
          features::kDataReductionProxyEnabledWithNetworkService) &&
      has_via_header && HasURLRedirectCycle(url_chain)) {
    data_reduction_proxy_info->bypass_all = true;
    data_reduction_proxy_info->mark_proxies_as_bad = false;
    data_reduction_proxy_info->bypass_duration = base::TimeDelta();
    return BYPASS_EVENT_TYPE_URL_REDIRECT_CYCLE;
  }

  if (ParseHeadersForBypassInfo(headers, data_reduction_proxy_info)) {
    // A chrome-proxy response header is only present in a 502. For proper
    // reporting, this check must come before the 5xx checks below.
    if (!data_reduction_proxy_info->mark_proxies_as_bad)
      return BYPASS_EVENT_TYPE_CURRENT;

    const base::TimeDelta& duration =
        data_reduction_proxy_info->bypass_duration;
    if (duration <= base::TimeDelta::FromSeconds(kShortBypassMaxSeconds))
      return BYPASS_EVENT_TYPE_SHORT;
    if (duration <= base::TimeDelta::FromSeconds(kMediumBypassMaxSeconds))
      return BYPASS_EVENT_TYPE_MEDIUM;
    return BYPASS_EVENT_TYPE_LONG;
  }

  // If a bypass is triggered by any of the following cases, then the data
  // reduction proxy should be bypassed for a random duration between 1 and 5
  // minutes.
  data_reduction_proxy_info->mark_proxies_as_bad = true;
  data_reduction_proxy_info->bypass_duration = GetDefaultBypassDuration();

  // Fall back if a 500, 502 or 503 is returned.
  if (headers.response_code() == net::HTTP_INTERNAL_SERVER_ERROR)
    return BYPASS_EVENT_TYPE_STATUS_500_HTTP_INTERNAL_SERVER_ERROR;
  if (headers.response_code() == net::HTTP_BAD_GATEWAY) {
    if (base::FeatureList::IsEnabled(
            features::kDataReductionProxyBlockOnBadGatewayResponse)) {
      // When 502 response is received with no valid directive in Chrome-Proxy
      // header, it is likely the renderer may have been blocked in receiving
      // the headers due to CORB, etc. In this case, block-once or a block all
      // proxies for a small number of seconds can be an alternative.
      data_reduction_proxy_info->bypass_all = true;
      data_reduction_proxy_info->bypass_duration =
          base::TimeDelta::FromSeconds(base::GetFieldTrialParamByFeatureAsInt(
              features::kDataReductionProxyBlockOnBadGatewayResponse,
              "block_duration_seconds", 1));
      return BYPASS_EVENT_TYPE_SHORT;
    }
    return BYPASS_EVENT_TYPE_STATUS_502_HTTP_BAD_GATEWAY;
  }
  if (headers.response_code() == net::HTTP_SERVICE_UNAVAILABLE)
    return BYPASS_EVENT_TYPE_STATUS_503_HTTP_SERVICE_UNAVAILABLE;
  // TODO(kundaji): Bypass if Proxy-Authenticate header value cannot be
  // interpreted by data reduction proxy.
  if (headers.response_code() == net::HTTP_PROXY_AUTHENTICATION_REQUIRED &&
      !headers.HasHeader("Proxy-Authenticate")) {
    // Bypass all proxies for a few RTTs until the config fetch could update the
    // session key. The value 500 ms is the RTT observed for config fetches.
    data_reduction_proxy_info->bypass_all = true;
    data_reduction_proxy_info->bypass_duration =
        base::TimeDelta::FromMilliseconds(3 * 500);
    return BYPASS_EVENT_TYPE_MALFORMED_407;
  }

  bool disable_bypass_on_missing_via_header =
      params::IsWarmupURLFetchCallbackEnabled();

  if (!has_via_header && !disable_bypass_on_missing_via_header &&
      (headers.response_code() != net::HTTP_NOT_MODIFIED)) {
    // A Via header might not be present in a 304. Since the goal of a 304
    // response is to minimize information transfer, a sender in general
    // should not generate representation metadata other than Cache-Control,
    // Content-Location, Date, ETag, Expires, and Vary.

    // The proxy Via header might also not be present in a 4xx response.
    // Separate this case from other responses that are missing the header.
    if (headers.response_code() >= net::HTTP_BAD_REQUEST &&
        headers.response_code() < net::HTTP_INTERNAL_SERVER_ERROR) {
      // At this point, any 4xx response that is missing the via header
      // indicates an issue that is scoped to only the current request, so only
      // bypass the data reduction proxy for the current request.
      data_reduction_proxy_info->bypass_all = true;
      data_reduction_proxy_info->mark_proxies_as_bad = false;
      data_reduction_proxy_info->bypass_duration = base::TimeDelta();
      return BYPASS_EVENT_TYPE_MISSING_VIA_HEADER_4XX;
    }

    data_reduction_proxy_info->mark_proxies_as_bad = true;
    data_reduction_proxy_info->bypass_duration = GetRandomBypassTime(
        base::TimeDelta::FromSeconds(60), base::TimeDelta::FromSeconds(300));

    return BYPASS_EVENT_TYPE_MISSING_VIA_HEADER_OTHER;
  }
  // There is no bypass event.
  return BYPASS_EVENT_TYPE_MAX;
}

int64_t GetDataReductionProxyOFCL(const net::HttpResponseHeaders* headers) {
  std::string ofcl_str;
  int64_t ofcl;
  if (GetDataReductionProxyActionValue(headers, "ofcl", &ofcl_str) &&
      base::StringToInt64(ofcl_str, &ofcl) && ofcl >= 0) {
    return ofcl;
  }
  return -1;
}

double EstimateCompressionRatioFromHeaders(
    const network::mojom::URLResponseHead* response_head) {
  if (!response_head->network_accessed || !response_head->headers ||
      response_head->headers->GetContentLength() <= 0 ||
      response_head->proxy_server.is_direct()) {
    return 1.0;  // No compression
  }

  int64_t original_content_length =
      GetDataReductionProxyOFCL(response_head->headers.get());
  if (original_content_length > 0) {
    return static_cast<double>(original_content_length) /
           static_cast<double>(response_head->headers->GetContentLength());
  }
  return 1.0;  // No compression
}

}  // namespace data_reduction_proxy
