// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_GPU_CAST_GPU_FACTORY_H_
#define CHROMECAST_MEDIA_GPU_CAST_GPU_FACTORY_H_

#include <memory>

#include "base/memory/scoped_refptr.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace gpu {
class GpuMemoryBufferManager;
}  // namespace gpu

namespace media {
class VideoDecoder;
class VideoEncodeAccelerator;
}  // namespace media

namespace viz {
class ContextProviderCommandBuffer;
}  // namespace viz

namespace chromecast {

class RemoteInterfaces;

// Abstraction for accessing GPU related capabilities from non-Renderer
// process (e.g., Utility process). All APIs must be invoked from the
// provided |mojo_task_runner|.
class CastGpuFactory {
 public:
  static std::unique_ptr<CastGpuFactory> Create(
      scoped_refptr<base::SingleThreadTaskRunner> mojo_task_runner,
      RemoteInterfaces* browser_services);

  virtual ~CastGpuFactory() = default;

  // Create and return a CommandBuffer context provider for OpenGL related
  // operations.
  virtual scoped_refptr<viz::ContextProviderCommandBuffer>
  CreateOpenGLContextProvider() = 0;

  // Create and return VideoDecoder.
  virtual std::unique_ptr<::media::VideoDecoder> CreateVideoDecoder() = 0;

  // Create and return VideoEncodeAccelerator.
  virtual std::unique_ptr<::media::VideoEncodeAccelerator>
  CreateVideoEncoder() = 0;

  virtual gpu::GpuMemoryBufferManager* GpuMemoryBufferManager() = 0;
};

}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_GPU_CAST_GPU_FACTORY_H_
