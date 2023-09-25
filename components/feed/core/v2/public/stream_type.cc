// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/public/stream_type.h"

namespace feed {

std::string StreamType::ToString() const {
  switch (kind_) {
    case StreamKind::kUnknown:
      return "Unknown";
    case StreamKind::kForYou:
      return "ForYou";
    case StreamKind::kFollowing:
      return "WebFeed";
    case StreamKind::kSupervisedUser:
      return "SupervisedUserFeed";
    case StreamKind::kSingleWebFeed:
      return "SingleWebFeed_" + web_feed_id_;
  }
}

// static
StreamType StreamType::ForTaskId(RefreshTaskId task_id) {
  switch (task_id) {
    case RefreshTaskId::kRefreshForYouFeed:
      return StreamType(StreamKind::kForYou);
    case RefreshTaskId::kRefreshWebFeed:
      return StreamType(StreamKind::kFollowing);
  }
}

bool StreamType::GetRefreshTaskId(RefreshTaskId& out_id) const {
  switch (kind_) {
    case StreamKind::kUnknown:
      return false;
    case StreamKind::kForYou:
      out_id = RefreshTaskId::kRefreshForYouFeed;
      return true;
    case StreamKind::kFollowing:
    case StreamKind::kSingleWebFeed:
    case StreamKind::kSupervisedUser:
      return false;
  }
}

}  // namespace feed
