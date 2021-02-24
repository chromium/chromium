// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_CLIENT_FRAME_EVICTOR_H_
#define COMPONENTS_VIZ_CLIENT_FRAME_EVICTOR_H_

#include "base/macros.h"
#include "components/viz/client/frame_eviction_manager.h"

namespace viz {

class FrameEvictorClient {
 public:
  virtual ~FrameEvictorClient() {}
  virtual void EvictDelegatedFrame() = 0;
};

// Keeps track of the visibility state of a child and notifies when the parent
// needs to drop its surface.
class VIZ_CLIENT_EXPORT FrameEvictor : public FrameEvictionManagerClient {
 public:
  explicit FrameEvictor(FrameEvictorClient* client);
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

 private:
  // FrameEvictionManagerClient implementation.
  void EvictCurrentFrame() override;

  FrameEvictorClient* client_;
  bool has_surface_ = false;
  bool visible_ = false;

  DISALLOW_COPY_AND_ASSIGN(FrameEvictor);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_CLIENT_FRAME_EVICTOR_H_
