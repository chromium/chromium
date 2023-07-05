// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/feed_stream_surface.h"

namespace feed {

FeedStreamSurface::FeedStreamSurface(StreamType stream_type,
                                     SingleWebFeedEntryPoint entry_point)
    : stream_type_(stream_type), entry_point_(entry_point) {
  static SurfaceId::Generator id_generator;
  surface_id_ = id_generator.GenerateNextId();
}

FeedStreamSurface::~FeedStreamSurface() = default;

SurfaceId FeedStreamSurface::GetSurfaceId() const {
  return surface_id_;
}

}  // namespace feed
