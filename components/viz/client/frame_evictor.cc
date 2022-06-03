// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/client/frame_evictor.h"


namespace viz {

FrameEvictor::FrameEvictor(FrameEvictorClient* client) : client_(client) {}

FrameEvictor::~FrameEvictor() {
  OnSurfaceDiscarded();
}

void FrameEvictor::OnNewSurfaceEmbedded() {
  has_surface_ = true;
  FrameEvictionManager::GetInstance()->AddFrame(this, visible_);
}

void FrameEvictor::OnSurfaceDiscarded() {
  FrameEvictionManager::GetInstance()->RemoveFrame(this);
  has_surface_ = false;
}

void FrameEvictor::SetVisible(bool visible) {
  if (visible_ == visible)
    return;
  visible_ = visible;
  if (has_surface_) {
    if (visible)
      FrameEvictionManager::GetInstance()->LockFrame(this);
    else
      FrameEvictionManager::GetInstance()->UnlockFrame(this);
  }
}

void FrameEvictor::EvictCurrentFrame() {
  client_->EvictDelegatedFrame();
}

}  // namespace viz
