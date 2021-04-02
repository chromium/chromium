// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_LITE_VIDEO_LITE_VIDEO_UTIL_H_
#define CHROME_RENDERER_LITE_VIDEO_LITE_VIDEO_UTIL_H_

#include <stddef.h>

#include "base/optional.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace lite_video {

// Returns whether LiteVideo is enabled.
bool IsLiteVideoEnabled();

// Returns whether LiteVideo should be disabled for cache-control: no-transform
// responses.
bool ShouldDisableLiteVideoForCacheControlNoTransform();

// Returns whether LiteVideo should throttle responses without content-length.
bool ShouldThrottleLiteVideoMissingContentLength();

// Returns the maximum active throttles size.
size_t GetMaxActiveThrottles();

// Returns the content length of the response received. base::nullopt is
// returned when content length cannot be retrieved.
base::Optional<uint64_t> GetContentLength(
    const network::mojom::URLResponseHead& response_head);

}  // namespace lite_video

#endif  // CHROME_RENDERER_LITE_VIDEO_LITE_VIDEO_UTIL_H_
