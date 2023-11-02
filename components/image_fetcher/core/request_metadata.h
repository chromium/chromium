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

  std::string mime_type;
  int http_response_code;
  std::string content_location_header;
};

bool operator==(const RequestMetadata& lhs, const RequestMetadata& rhs);
bool operator!=(const RequestMetadata& lhs, const RequestMetadata& rhs);

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_CORE_REQUEST_METADATA_H_
