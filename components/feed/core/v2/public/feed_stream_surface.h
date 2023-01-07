// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PUBLIC_FEED_STREAM_SURFACE_H_
#define COMPONENTS_FEED_CORE_V2_PUBLIC_FEED_STREAM_SURFACE_H_

#include "base/observer_list_types.h"
#include "components/feed/core/v2/public/reliability_logging_bridge.h"
#include "components/feed/core/v2/public/stream_type.h"
#include "components/feed/core/v2/public/types.h"

namespace feedui {
class StreamUpdate;
}

namespace feed {

// Consumes stream data for a single `StreamType` and displays it to the user.
class FeedStreamSurface : public base::CheckedObserver {
 public:
  explicit FeedStreamSurface(StreamType type);
  ~FeedStreamSurface() override;

  // Returns a unique ID for the surface. The ID will not be reused until
  // after the Chrome process is closed.
  SurfaceId GetSurfaceId() const;

  // Returns the `StreamType` this `FeedStreamSurface` requests.
  StreamType GetStreamType() const { return stream_type_; }

  // Called after registering the observer to provide the full stream state.
  // Also called whenever the stream changes.
  virtual void StreamUpdate(const feedui::StreamUpdate&) = 0;

  // Access to the xsurface data store.
  virtual void ReplaceDataStoreEntry(base::StringPiece key,
                                     base::StringPiece data) = 0;
  virtual void RemoveDataStoreEntry(base::StringPiece key) = 0;

  // Returns the ReliabilityLogger associated with this surface.
  virtual ReliabilityLoggingBridge& GetReliabilityLoggingBridge() = 0;

 private:
  StreamType stream_type_;
  SurfaceId surface_id_;
};

}  // namespace feed
#endif  // COMPONENTS_FEED_CORE_V2_PUBLIC_FEED_STREAM_SURFACE_H_
