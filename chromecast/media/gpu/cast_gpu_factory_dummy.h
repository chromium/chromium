// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_GPU_CAST_GPU_FACTORY_DUMMY_H_
#define CHROMECAST_MEDIA_GPU_CAST_GPU_FACTORY_DUMMY_H_

#include "chromecast/media/gpu/cast_gpu_factory.h"

namespace chromecast {

class CastGpuFactoryDummy : public CastGpuFactory {
 public:
  CastGpuFactoryDummy();
  ~CastGpuFactoryDummy() override;
  CastGpuFactoryDummy(const CastGpuFactoryDummy&) = delete;
  CastGpuFactoryDummy& operator=(const CastGpuFactoryDummy&) = delete;

 private:
  // CastGpuFactory implementation:
  scoped_refptr<viz::ContextProviderCommandBuffer> CreateOpenGLContextProvider()
      override;
  std::unique_ptr<::media::VideoDecoder> CreateVideoDecoder() override;
  std::unique_ptr<::media::VideoEncodeAccelerator> CreateVideoEncoder()
      override;
  gpu::GpuMemoryBufferManager* GpuMemoryBufferManager() override;
};

}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_GPU_CAST_GPU_FACTORY_DUMMY_H_
