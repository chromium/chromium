// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/request_metadata.h"

namespace image_fetcher {

RequestMetadata::RequestMetadata()
    : http_response_code(RESPONSE_CODE_INVALID) {}

bool operator==(const RequestMetadata& lhs, const RequestMetadata& rhs) {
  return lhs.mime_type == rhs.mime_type &&
         lhs.http_response_code == rhs.http_response_code &&
         lhs.content_location_header == rhs.content_location_header;
}

bool operator!=(const RequestMetadata& lhs, const RequestMetadata& rhs) {
  return !(lhs == rhs);
}

}  // namespace image_fetcher
