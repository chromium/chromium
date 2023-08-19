// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_FEED_STREAM_SURFACE_H_
#define COMPONENTS_FEED_CORE_V2_FEED_STREAM_SURFACE_H_

#include "components/feed/core/v2/feed_stream_surface.h"

#include "components/feed/core/v2/public/stream_type.h"

namespace feed {

// Consumes stream data for a single `StreamType` and displays it to the user.
class FeedStreamSurface final {
 public:
  explicit FeedStreamSurface(
      StreamType type,
      SingleWebFeedEntryPoint entry_point = SingleWebFeedEntryPoint::kOther);
  ~FeedStreamSurface();

  // Returns a unique ID for the surface. The ID will not be reused until
  // after the Chrome process is closed.
  SurfaceId GetSurfaceId() const;

  // Returns the `StreamType` this `FeedStreamSurface` requests.
  const StreamType& GetStreamType() const { return stream_type_; }

  // Returns the `SingleWebFeedEntryPoint` this `FeedStreamSurface` was created
  // with.
  SingleWebFeedEntryPoint GetSingleWebFeedEntryPoint() const {
    return entry_point_;
  }

 private:
  StreamType stream_type_;
  SurfaceId surface_id_;
  SingleWebFeedEntryPoint entry_point_ = SingleWebFeedEntryPoint::kOther;
};

}  // namespace feed
#endif  // COMPONENTS_FEED_CORE_V2_FEED_STREAM_SURFACE_H_
