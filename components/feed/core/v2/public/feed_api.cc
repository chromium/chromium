// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/public/feed_api.h"

namespace feed {

std::string StreamType::ToString() const {
  switch (type_) {
    case Type::kUnspecified:
      return "Unspecified";
    case Type::kForYou:
      return "ForYou";
    case Type::kWebFeed:
      return "WebFeed";
  }
}

// static
StreamType StreamType::ForTaskId(RefreshTaskId task_id) {
  switch (task_id) {
    case RefreshTaskId::kRefreshForYouFeed:
      return kForYouStream;
    case RefreshTaskId::kRefreshWebFeed:
      return kWebFeedStream;
  }
}

bool StreamType::GetRefreshTaskId(RefreshTaskId& out_id) const {
  switch (type_) {
    case Type::kUnspecified:
      return false;
    case Type::kForYou:
      out_id = RefreshTaskId::kRefreshForYouFeed;
      return true;
    case Type::kWebFeed:
      out_id = RefreshTaskId::kRefreshWebFeed;
      return true;
  }
}
FeedApi::FeedApi() = default;
FeedApi::~FeedApi() = default;

FeedApi::UnreadContentObserver::UnreadContentObserver() = default;
FeedApi::UnreadContentObserver::~UnreadContentObserver() = default;

FeedStreamSurface::FeedStreamSurface(StreamType stream_type)
    : stream_type_(stream_type) {
  static SurfaceId::Generator id_generator;
  surface_id_ = id_generator.GenerateNextId();
}

FeedStreamSurface::~FeedStreamSurface() = default;

SurfaceId FeedStreamSurface::GetSurfaceId() const {
  return surface_id_;
}

}  // namespace feed
