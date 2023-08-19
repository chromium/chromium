// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_STREAM_SURFACE_SET_H_
#define COMPONENTS_FEED_CORE_V2_STREAM_SURFACE_SET_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/feed/core/proto/v2/wire/reliability_logging_enums.pb.h"
#include "components/feed/core/v2/public/stream_type.h"
#include "components/feed/core/v2/public/types.h"

namespace feed {
class SurfaceRenderer;

// The set of surfaces attached to a StreamType.
class StreamSurfaceSet {
 public:
  // Entry in the surface set. Holds the surface and information about it.
  struct Entry {
    SurfaceId surface_id = {};
    // The surface renderer.
    raw_ptr<SurfaceRenderer, DanglingUntriaged> renderer;
    // Whether or not the feed content was ever reported as viewed.
    bool feed_viewed = false;
  };

  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;
    virtual void SurfaceAdded(
        SurfaceId surface_id,
        SurfaceRenderer* renderer,
        feedwire::DiscoverLaunchResult loading_not_allowed_reason) = 0;
    virtual void SurfaceRemoved(SurfaceId surface_id) = 0;
  };

  explicit StreamSurfaceSet(const StreamType& stream_type);
  ~StreamSurfaceSet();

  void SurfaceAdded(SurfaceId surface_id,
                    SurfaceRenderer* renderer,
                    feedwire::DiscoverLaunchResult loading_not_allowed_reason);
  void SurfaceRemoved(SurfaceId surface_id);
  bool SurfacePresent(SurfaceId surface_id);
  void FeedViewed(SurfaceId surface_id);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Surface access.
  const std::vector<Entry>& surfaces() const { return surfaces_; }
  Entry* FindSurface(SurfaceId surface_id);
  std::vector<Entry>::const_iterator begin() const { return surfaces_.begin(); }
  std::vector<Entry>::const_iterator end() const { return surfaces_.end(); }
  std::vector<Entry>::iterator begin() { return surfaces_.begin(); }
  std::vector<Entry>::iterator end() { return surfaces_.end(); }
  bool empty() const { return surfaces_.empty(); }

  // Returns whether or not at least one attached surface has shown content.
  bool HasSurfaceShowingContent() const;

 private:
  StreamType stream_type_;
  std::vector<Entry> surfaces_;
  base::ObserverList<Observer> observers_;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_STREAM_SURFACE_SET_H_
