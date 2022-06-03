// Copyright 2021 The Chromium Authors. All rights reserved.
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
  enum class Type {
    // An unspecified stream type. Used only to represent an uninitialized
    // stream type value.
    kUnspecified,
    // The For-You feed stream.
    kForYou,
    // The Web Feed stream.
    kWebFeed,
  };
  constexpr StreamType() = default;
  constexpr explicit StreamType(Type t) : type_(t) {}
  bool operator<(const StreamType& rhs) const { return type_ < rhs.type_; }
  bool operator==(const StreamType& rhs) const { return type_ == rhs.type_; }
  bool IsForYou() const { return type_ == Type::kForYou; }
  bool IsWebFeed() const { return type_ == Type::kWebFeed; }
  bool IsValid() const { return type_ != Type::kUnspecified; }
  Type GetType() const { return type_; }

  // Returns a human-readable value, for debugging/DCHECK prints.
  std::string ToString() const;

  // Mapping functions between RefreshTaskId and StreamType.
  // Returns false if there should be no background refreshes associated with
  // this stream.
  bool GetRefreshTaskId(RefreshTaskId& out_id) const;
  static StreamType ForTaskId(RefreshTaskId task_id);

 private:
  Type type_ = Type::kUnspecified;
};

constexpr StreamType kForYouStream(StreamType::Type::kForYou);
constexpr StreamType kWebFeedStream(StreamType::Type::kWebFeed);

inline std::ostream& operator<<(std::ostream& os,
                                const StreamType& stream_type) {
  return os << stream_type.ToString();
}

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_PUBLIC_STREAM_TYPE_H_
