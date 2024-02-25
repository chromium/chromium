// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_NETWORK_DOWNLOAD_HTTP_UTILS_H_
#define COMPONENTS_DOWNLOAD_NETWORK_DOWNLOAD_HTTP_UTILS_H_

#include <optional>

namespace net {
class HttpByteRange;
class HttpRequestHeaders;
}  // namespace net

namespace download {

// Returns the http byte range for range request. Or nullopt if failed to parse
// the range header.
std::optional<net::HttpByteRange> ParseRangeHeader(
    const net::HttpRequestHeaders& request_headers);

// Validates the http request header. Returns true if request headers can be
// parsed correctly.
bool ValidateRequestHeaders(const net::HttpRequestHeaders& request_headers);

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_NETWORK_DOWNLOAD_HTTP_UTILS_H_
