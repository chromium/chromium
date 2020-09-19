// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_LITE_VIDEO_LITE_VIDEO_UTIL_H_
#define CHROME_RENDERER_LITE_VIDEO_LITE_VIDEO_UTIL_H_

#include <stddef.h>

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

}  // namespace lite_video

#endif  // CHROME_RENDERER_LITE_VIDEO_LITE_VIDEO_UTIL_H_
