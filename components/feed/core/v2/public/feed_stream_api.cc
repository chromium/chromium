// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/public/feed_stream_api.h"

namespace feed {

std::string StreamType::ToString() const {
  switch (type_) {
    case Type::kUnspecified:
      return "Unspecified";
    case Type::kInterest:
      return "ForYou";
    case Type::kWebFeed:
      return "WebFeed";
  }
}

FeedStreamApi::FeedStreamApi() = default;
FeedStreamApi::~FeedStreamApi() = default;

FeedStreamApi::SurfaceInterface::SurfaceInterface(StreamType stream_type)
    : stream_type_(stream_type) {
  static SurfaceId::Generator id_generator;
  surface_id_ = id_generator.GenerateNextId();
}

FeedStreamApi::SurfaceInterface::~SurfaceInterface() = default;

SurfaceId FeedStreamApi::SurfaceInterface::GetSurfaceId() const {
  return surface_id_;
}

}  // namespace feed
