// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/stream_surface_set.h"

#include "base/observer_list.h"
#include "components/feed/core/v2/feed_stream_surface.h"

namespace feed {

StreamSurfaceSet::StreamSurfaceSet(const StreamType& stream_type)
    : stream_type_(stream_type) {}

StreamSurfaceSet::~StreamSurfaceSet() = default;

void StreamSurfaceSet::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void StreamSurfaceSet::RemoveObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void StreamSurfaceSet::SurfaceAdded(
    SurfaceId surface_id,
    SurfaceRenderer* renderer,
    feedwire::DiscoverLaunchResult loading_not_allowed_reason) {
  Entry entry;
  entry.surface_id = surface_id;
  entry.renderer = renderer;
  surfaces_.push_back(entry);

  for (auto& observer : observers_) {
    observer.SurfaceAdded(surface_id, renderer, loading_not_allowed_reason);
  }
}

void StreamSurfaceSet::SurfaceRemoved(SurfaceId surface_id) {
  for (size_t i = 0; i < surfaces_.size(); ++i) {
    if (surfaces_[i].surface_id == surface_id) {
      surfaces_.erase(surfaces_.begin() + i);
      break;
    }
  }

  for (auto& observer : observers_) {
    observer.SurfaceRemoved(surface_id);
  }
}

bool StreamSurfaceSet::SurfacePresent(SurfaceId surface_id) {
  for (auto& surface : surfaces_) {
    if (surface.surface_id == surface_id) {
      return true;
    }
  }
  return false;
}

void StreamSurfaceSet::FeedViewed(SurfaceId surface_id) {
  StreamSurfaceSet::Entry* entry = FindSurface(surface_id);
  if (!entry)
    return;
  entry->feed_viewed = true;
}

bool StreamSurfaceSet::HasSurfaceShowingContent() const {
  for (const auto& entry : surfaces_) {
    if (entry.feed_viewed)
      return true;
  }
  return false;
}

StreamSurfaceSet::Entry* StreamSurfaceSet::FindSurface(SurfaceId surface_id) {
  for (Entry& entry : *this) {
    if (entry.surface_id == surface_id) {
      return &entry;
    }
  }
  return nullptr;
}

}  // namespace feed
