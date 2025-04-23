// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/mirroring_gpu_factories_factory.h"

#include "base/functional/bind.h"
#include "media/mojo/mojom/video_encode_accelerator.mojom.h"
#include "services/viz/public/cpp/gpu/command_buffer_metrics.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "services/viz/public/cpp/gpu/gpu.h"

namespace mirroring {

namespace {

using media::cast::CastEnvironment;

}

MirroringGpuFactoriesFactory::MirroringGpuFactoriesFactory(
    scoped_refptr<CastEnvironment> cast_environment,
    viz::Gpu& gpu,
    base::RepeatingClosure context_lost_cb)
    : cast_environment_(std::move(cast_environment)),
      gpu_(gpu),
      context_lost_cb_(std::move(context_lost_cb)) {}

MirroringGpuFactoriesFactory::MirroringGpuFactoriesFactory(
    MirroringGpuFactoriesFactory&&) = default;
MirroringGpuFactoriesFactory& MirroringGpuFactoriesFactory::operator=(
    MirroringGpuFactoriesFactory&&) = default;
MirroringGpuFactoriesFactory::~MirroringGpuFactoriesFactory() {
  CHECK(cast_environment_->CurrentlyOn(CastEnvironment::ThreadId::kVideo));
  if (instance_) {
    DestroyInstance();
  }
  context_provider_->RemoveObserver(this);
  context_provider_ = nullptr;
}

media::GpuVideoAcceleratorFactories&
MirroringGpuFactoriesFactory::GetInstance() {
  // If we have a valid context, return the current instance as it is still
  // valid.
  if (instance_) {
    return *instance_;
  }

  // Finally, create and return a new instance.
  static constexpr int32_t kStreamId = 0;

  // TODO(crbug.com/282984511): experiment with creation attributes.
  gpu::ContextCreationAttribs creation_attribs;
  creation_attribs.gpu_preference = gl::GpuPreference::kHighPerformance,
  creation_attribs.bind_generates_resource = false,
  creation_attribs.enable_gles2_interface = true,
  creation_attribs.context_type = gpu::CONTEXT_TYPE_OPENGLES3;

  auto gpu_channel_host = gpu_->EstablishGpuChannelSync();
  context_provider_ = base::MakeRefCounted<viz::ContextProviderCommandBuffer>(
      gpu_channel_host, kStreamId, gpu::SchedulingPriority::kHigh,
      GURL(std::string("chrome://gpu/CastStreaming")),
      /* automatic_flushes = */ false, /* support_locking = */ false,
      gpu::SharedMemoryLimits::ForMailboxContext(), creation_attribs,
      viz::command_buffer_metrics::ContextType::VIDEO_CAPTURE);

  // NOTE: this Unretained is safe because `this` is deleted on the VIDEO
  // thread.
  cast_environment_->PostTask(
      CastEnvironment::ThreadId::kVideo, FROM_HERE,
      base::BindOnce(&MirroringGpuFactoriesFactory::BindOnVideoThread,
                     base::Unretained(this)));

  mojo::PendingRemote<media::mojom::VideoEncodeAcceleratorProvider>
      vea_provider;
  gpu_->CreateVideoEncodeAcceleratorProvider(
      vea_provider.InitWithNewPipeAndPassReceiver());

  auto codec_factory = std::make_unique<media::MojoCodecFactoryDefault>(
      cast_environment_->GetTaskRunner(CastEnvironment::ThreadId::kVideo),
      context_provider_,
      /*enable_video_decode_accelerator=*/false,
      /*enable_video_encode_accelerator=*/true, std::move(vea_provider));

  instance_ = media::MojoGpuVideoAcceleratorFactories::Create(
      std::move(gpu_channel_host),
      cast_environment_->GetTaskRunner(CastEnvironment::ThreadId::kMain),
      cast_environment_->GetTaskRunner(CastEnvironment::ThreadId::kVideo),
      context_provider_, std::move(codec_factory),
      /*enable_video_gpu_memory_buffers=*/true,
      /*enable_media_stream_gpu_memory_buffers=*/false,
      /*enable_video_decode_accelerator=*/false,
      /*enable_video_encode_accelerator=*/true);

  return *instance_;
}

void MirroringGpuFactoriesFactory::BindOnVideoThread() {
  CHECK(cast_environment_->CurrentlyOn(CastEnvironment::ThreadId::kVideo));
  CHECK(context_provider_);
  if (context_provider_->BindToCurrentSequence() !=
      gpu::ContextResult::kSuccess) {
    OnContextLost();
    return;
  }
  context_provider_->AddObserver(this);
}

void MirroringGpuFactoriesFactory::OnContextLost() {
  cast_environment_->PostTask(
      CastEnvironment::ThreadId::kVideo, FROM_HERE,
      base::BindOnce(&media::MojoGpuVideoAcceleratorFactories::DestroyContext,
                     base::Unretained(instance_.get())));

  std::move(context_lost_cb_).Run();
  DestroyInstance();
}

void MirroringGpuFactoriesFactory::DestroyInstance() {
  CHECK(instance_);
  // The GPU factories object, after construction, must only be accessed on the
  // video encoding thread (including for deletion).
  cast_environment_->GetTaskRunner(CastEnvironment::ThreadId::kVideo)
      ->DeleteSoon(FROM_HERE, std::move(instance_));
}

}  // namespace mirroring
