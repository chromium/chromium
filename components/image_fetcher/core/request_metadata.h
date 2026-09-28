// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_CORE_REQUEST_METADATA_H_
#define COMPONENTS_IMAGE_FETCHER_CORE_REQUEST_METADATA_H_

#include <string>

namespace image_fetcher {

// Metadata for a URL request.
struct RequestMetadata {
  // Impossible http response code. Used to signal that no http response code
  // was received.
  enum ResponseCode { RESPONSE_CODE_INVALID = -1 };

  RequestMetadata();

  friend bool operator==(const RequestMetadata&,
                         const RequestMetadata&) = default;

  std::string mime_type;
  // HTTP status, or RESPONSE_CODE_INVALID if no completed response was read.
  int http_response_code;
  // net::OK when no network error was recorded, including cache hits or paths
  // that did not make a network request. Otherwise the network or download
  // error. This can distinguish failures without an HTTP response, including
  // a body that exceeded the configured download limit.
  int net_error;
  std::string content_location_header;
};

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_CORE_REQUEST_METADATA_H_
