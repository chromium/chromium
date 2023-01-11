// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_GMB_VIDEO_FRAME_POOL_CONTEXT_PROVIDER_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_GMB_VIDEO_FRAME_POOL_CONTEXT_PROVIDER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "media/video/renderable_gpu_memory_buffer_video_frame_pool.h"

namespace viz {

// Context provider for contexts needed to create instances of
// `media::RenderableGpuMemoryBufferVideoFramePool`. Used to create an instance
// of `FrameSinkManagerImpl` capable of creating `FrameSinkVideoCapturerImpl`
// with `GpuMemoryBuffer` support.
class GmbVideoFramePoolContextProvider {
 public:
  virtual ~GmbVideoFramePoolContextProvider() = default;

  // Creates new context that can then subsequently be used to create
  // a media::RenderableGpuMemoryBufferVideoFramePool. The |on_context_lost|
  // will be invoked to notify the callers that the context returned from the
  // call is no longer functional. It will be called on the current sequence.
  virtual std::unique_ptr<
      media::RenderableGpuMemoryBufferVideoFramePool::Context>
  CreateContext(base::OnceClosure on_context_lost) = 0;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_GMB_VIDEO_FRAME_POOL_CONTEXT_PROVIDER_H_
