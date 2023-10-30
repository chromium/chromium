// Copyright 2020 Record Replay Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Define no-op versions of the record_replay_render.h interface, for fixing linker errors
// on executables that aren't recorded.

#include "components/viz/service/display/record_replay_render.h"

namespace recordreplay {

void OnCommitPaint() {}

void OnReadyToCommit() {}

void NotifyRasterBuffer(const viz::SharedBitmapId& shared_bitmap_id,
                        void* memory, size_t size) {}

void SubmitCompositorFrame(const viz::LocalSurfaceId& local_surface_id,
                           const viz::CompositorFrame& frame) {}

bool PopulateSkBitmapWithResource(SkBitmap* sk_bitmap, viz::ResourceId resource_id) {
  return false;
}

void OnPaintFinished(const SkPixmap& pixmap) {}

void SetCompositorProxy(cc::ProxyMain* proxy) {}

void CompositorProxyDestroyed(cc::ProxyMain* proxy) {}

} // namespace recordreplay
