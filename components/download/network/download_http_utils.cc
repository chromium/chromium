// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/network/download_http_utils.h"

#include "net/http/http_byte_range.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"

namespace download {

std::optional<net::HttpByteRange> ParseRangeHeader(
    const net::HttpRequestHeaders& request_headers) {
  std::vector<net::HttpByteRange> byte_ranges;
  std::optional<std::string> range_header =
      request_headers.GetHeader(net::HttpRequestHeaders::kRange);
  if (!range_header) {
    return std::nullopt;
  }

  bool success =
      net::HttpUtil::ParseRangeHeader(range_header.value(), &byte_ranges);

  // Multiple ranges are not supported.
  if (!success || byte_ranges.empty() || byte_ranges.size() > 1)
    return std::nullopt;

  return byte_ranges.front();
}

bool ValidateRequestHeaders(const net::HttpRequestHeaders& request_headers) {
  if (request_headers.HasHeader(net::HttpRequestHeaders::kRange)) {
    return ParseRangeHeader(request_headers).has_value();
  }

  return true;
}

}  // namespace download
