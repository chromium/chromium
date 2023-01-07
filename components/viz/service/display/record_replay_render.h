// Copyright 2020 Record Replay Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_RECORD_REPLAY_RENDER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_RECORD_REPLAY_RENDER_H_

#include "cc/trees/proxy_main.h"
#include "components/viz/common/resources/shared_bitmap.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPixmap.h"

namespace recordreplay {

// When recording, renderer processes generate compositor frames in the usual
// way and send them on to the GPU process for drawing to the screen. When
// replaying (and optionally when recording, for debugging) the process
// additionally sends these frames to an in process renderer for updating an
// in process buffer with the data the process is currently drawing. This data
// can then be encoded to base64 images and reported to the record/replay
// driver and sent to clients inspecting the recording.

// Called on the main thread when changes have been committed to the layer tree
// and the thread is about to block until the compositor thread is ready to commit.
void OnCommitPaint();

// Called on the compositor thread when the main thread's notification about
// the commit.
void OnReadyToCommit();

// Called when a shared memory buffer for rasterization has been created.
void NotifyRasterBuffer(const viz::SharedBitmapId& shared_bitmap_id,
                        void* memory, size_t size);

// Called when a CompositorFrame is being submitted to the GPU process.
void SubmitCompositorFrame(const viz::LocalSurfaceId& local_surface_id,
                           const viz::CompositorFrame& frame);

// Called to populate a bitmap with information for the given resource in the current frame.
bool PopulateSkBitmapWithResource(SkBitmap* sk_bitmap, viz::ResourceId resource_id);

// Called on the compositor thread when beginning a repaint while diverged from the recording.
void OnCompositorRepainting();

// Called on the compositor thread when painting to the software output device has finished.
void OnPaintFinished(const SkPixmap& pixmap);

// Called on the compositor thread when repainting has finished, which may or may not
// have actually performed a paint.
void OnRepaintFinished();

// Set the proxy which will be used for triggering repaints from the main thread.
void SetCompositorProxy(cc::ProxyMain* proxy);

} // namespace recordreplay

#endif // COMPONENTS_VIZ_SERVICE_DISPLAY_RECORD_REPLAY_RENDER_H_
