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
 public:
  constexpr StreamType() = default;
  constexpr explicit StreamType(StreamKind k) : kind_(k) {}
  bool operator<(const StreamType& rhs) const { return kind_ < rhs.kind_; }
  bool operator==(const StreamType& rhs) const { return kind_ == rhs.kind_; }
  bool IsForYou() const { return kind_ == StreamKind::kForYou; }
  bool IsWebFeed() const { return kind_ == StreamKind::kFollowing; }
  bool IsValid() const { return kind_ != StreamKind::kUnknown; }
  StreamKind GetType() const { return kind_; }

  // Returns a human-readable value, for debugging/DCHECK prints.
  std::string ToString() const;

  // Mapping functions between RefreshTaskId and StreamType.
  // Returns false if there should be no background refreshes associated with
  // this stream.
  bool GetRefreshTaskId(RefreshTaskId& out_id) const;
  static StreamType ForTaskId(RefreshTaskId task_id);

 private:
  StreamKind kind_ = StreamKind::kUnknown;
};

constexpr StreamType kForYouStream(StreamKind::kForYou);
constexpr StreamType kWebFeedStream(StreamKind::kFollowing);

inline std::ostream& operator<<(std::ostream& os,
                                const StreamType& stream_type) {
  return os << stream_type.ToString();
}

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_PUBLIC_STREAM_TYPE_H_
