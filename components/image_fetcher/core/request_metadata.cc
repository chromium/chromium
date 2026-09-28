// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/request_metadata.h"

#include "net/base/net_errors.h"

namespace image_fetcher {

RequestMetadata::RequestMetadata()
    : http_response_code(RESPONSE_CODE_INVALID), net_error(net::OK) {}

}  // namespace image_fetcher
