// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/gpu/cast_gpu_factory_dummy.h"

#include "base/task/single_thread_task_runner.h"
#include "media/base/video_decoder.h"
#include "media/video/video_encode_accelerator.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"

namespace chromecast {

// static.
std::unique_ptr<CastGpuFactory> CastGpuFactory::Create(
    scoped_refptr<base::SingleThreadTaskRunner> mojo_task_runner,
    RemoteInterfaces* browser_services) {
  return std::make_unique<CastGpuFactoryDummy>();
}

CastGpuFactoryDummy::CastGpuFactoryDummy() = default;

CastGpuFactoryDummy::~CastGpuFactoryDummy() = default;

scoped_refptr<viz::ContextProviderCommandBuffer>
CastGpuFactoryDummy::CreateOpenGLContextProvider() {
  return nullptr;
}

std::unique_ptr<::media::VideoDecoder>
CastGpuFactoryDummy::CreateVideoDecoder() {
  return nullptr;
}

std::unique_ptr<::media::VideoEncodeAccelerator>
CastGpuFactoryDummy::CreateVideoEncoder() {
  return nullptr;
}

gpu::GpuMemoryBufferManager* CastGpuFactoryDummy::GpuMemoryBufferManager() {
  return nullptr;
}

}  // namespace chromecast
