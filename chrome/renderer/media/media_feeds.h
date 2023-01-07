// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_MEDIA_FEEDS_H_
#define CHROME_RENDERER_MEDIA_MEDIA_FEEDS_H_

#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace content {
class RenderFrame;
}  // namespace content

class MediaFeeds {
 public:
  // Gets the Media Feed URL (if present).
  static absl::optional<GURL> GetMediaFeedURL(content::RenderFrame* frame);
};

#endif  // CHROME_RENDERER_MEDIA_MEDIA_FEEDS_H_
