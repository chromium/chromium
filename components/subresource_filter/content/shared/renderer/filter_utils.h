// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_RENDERER_FILTER_UTILS_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_RENDERER_FILTER_UTILS_H_

#include "components/url_pattern_index/proto/rules.pb.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"

namespace subresource_filter {

// Converts from blink `RequestContextType` to an `ElementType` that can be used
// to determine `LoadPolicy` via a `DocumentSubresourceFilter`.
url_pattern_index::proto::ElementType ToElementType(
    blink::mojom::RequestContextType request_context);

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_RENDERER_FILTER_UTILS_H_
