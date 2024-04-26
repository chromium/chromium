// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_CLIENT_FRAME_EVICTOR_H_
#define COMPONENTS_VIZ_CLIENT_FRAME_EVICTOR_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/viz/client/frame_eviction_manager.h"
#include "components/viz/common/surfaces/surface_id.h"

namespace viz {

class VIZ_CLIENT_EXPORT FrameEvictorClient {
 public:
  struct VIZ_CLIENT_EXPORT EvictIds {
    EvictIds();
    ~EvictIds();

    EvictIds(const EvictIds&) = delete;
    EvictIds& operator=(const EvictIds&) = delete;

    EvictIds(EvictIds&& other);
    EvictIds& operator=(EvictIds&& other);

    // `embedded_ids` contains a list of SurfaceIds embedded by the UI
    // compositor.
    std::vector<SurfaceId> embedded_ids;
    // `ui_compositor_id`, if valid, is the SurfaceId of the UI compositor root
    // surface to evict.
    SurfaceId ui_compositor_id;
  };

  virtual ~FrameEvictorClient() = default;
  virtual void EvictDelegatedFrame(
      const std::vector<SurfaceId>& surface_ids) = 0;
  virtual EvictIds CollectSurfaceIdsForEviction() const = 0;
  virtual SurfaceId GetCurrentSurfaceId() const = 0;
  virtual SurfaceId GetPreNavigationSurfaceId() const = 0;
};

// Keeps track of the visibility state of a child and notifies when the parent
// needs to drop its surface.
class VIZ_CLIENT_EXPORT FrameEvictor : public FrameEvictionManagerClient {
 public:
  explicit FrameEvictor(FrameEvictorClient* client);

  FrameEvictor(const FrameEvictor&) = delete;
  FrameEvictor& operator=(const FrameEvictor&) = delete;

  ~FrameEvictor() override;

  // Called when the parent allocates a new LocalSurfaceId for this child and
  // embeds it.
  void OnNewSurfaceEmbedded();

  // Called when the parent stops embedding the child's surface and evicts it.
  void OnSurfaceDiscarded();

  // Returns whether the parent is currently embedding a surface of this child.
  bool has_surface() const { return has_surface_; }

  // Notifies that the visibility state of the child has changed.
  void SetVisible(bool visible);

  bool visible() const { return visible_; }

  // Returns an ordered collection of `SurfaceIds` that should be evicted.
  std::vector<SurfaceId> CollectSurfaceIdsForEviction() const;

 private:
  // FrameEvictionManagerClient implementation.
  void EvictCurrentFrame() override;

  raw_ptr<FrameEvictorClient> client_;
  bool has_surface_ = false;
  bool visible_ = false;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_CLIENT_FRAME_EVICTOR_H_
