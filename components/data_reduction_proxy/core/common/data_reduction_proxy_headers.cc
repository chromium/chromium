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

const char kActionValueDelimiter = '=';

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

}  // namespace

namespace data_reduction_proxy {

const char* chrome_proxy_header() {
  return kChromeProxyHeader;
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
