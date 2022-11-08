// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PUBLIC_STREAM_TYPE_H_
#define COMPONENTS_FEED_CORE_V2_PUBLIC_STREAM_TYPE_H_

#include <string>

#include "components/feed/core/v2/public/types.h"

namespace feed {

// Selects the stream type.
// Note: currently there are two options, but this leaves room for more
// parameters.
class StreamType {
  // TODO(crbug.com/1369784) rename to StreamID.
 public:
  StreamType() = default;
  virtual ~StreamType() = default;
  explicit StreamType(StreamKind k, std::string s = std::string())
      : kind_(k), web_feed_id_(std::move(s)) {}
  bool operator<(const StreamType& rhs) const {
    if (kind_ == rhs.kind_) {
      if (kind_ != StreamKind::kSingleWebFeed)
        return false;
      return web_feed_id_.compare(rhs.web_feed_id_) < 0;
    }
    return kind_ < rhs.kind_;
  }
  bool operator==(const StreamType& rhs) const {
    return (kind_ == rhs.kind_) && (web_feed_id_ == rhs.web_feed_id_);
  }
  bool IsForYou() const { return kind_ == StreamKind::kForYou; }
  bool IsWebFeed() const { return kind_ == StreamKind::kFollowing; }
  bool IsSingleWebFeed() const { return kind_ == StreamKind::kSingleWebFeed; }
  bool IsValid() const { return kind_ != StreamKind::kUnknown; }
  StreamKind GetKind() const { return kind_; }
  std::string GetWebFeedId() const { return web_feed_id_; }

  // Returns a human-readable value, for debugging/DCHECK prints.
  std::string ToString() const;

  // Mapping functions between RefreshTaskId and StreamType.
  // Returns false if there should be no background refreshes associated with
  // this stream.
  bool GetRefreshTaskId(RefreshTaskId& out_id) const;
  static StreamType ForTaskId(RefreshTaskId task_id);

 private:
  StreamKind kind_ = StreamKind::kUnknown;
  // Identifies the feed ID in the case that the feed is a SingleWebFeed.
  std::string web_feed_id_;
};

inline std::ostream& operator<<(std::ostream& os,
                                const StreamType& stream_type) {
  return os << stream_type.ToString();
}

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_PUBLIC_STREAM_TYPE_H_
