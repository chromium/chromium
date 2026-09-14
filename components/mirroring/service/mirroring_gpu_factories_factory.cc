// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/mirroring_gpu_factories_factory.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "media/mojo/mojom/video_encode_accelerator.mojom.h"
#include "services/viz/public/cpp/gpu/command_buffer_metrics.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "services/viz/public/cpp/gpu/gpu.h"

namespace mirroring {

namespace {

using media::cast::CastEnvironment;

}

std::optional<MirroringGpuFactoriesFactory::UniquePtr>
MirroringGpuFactoriesFactory::Create(
    scoped_refptr<CastEnvironment> cast_environment,
    viz::Gpu& gpu,
    base::OnceClosure context_lost_cb,
    ContextConfiguredCallback context_configured_cb) {
  CHECK(cast_environment->CurrentlyOn(CastEnvironment::ThreadId::kMain));

  auto gpu_channel_host = gpu.EstablishGpuChannelSync();
  if (!gpu_channel_host) {
    return std::nullopt;
  }

  auto video_runner =
      cast_environment->GetTaskRunner(CastEnvironment::ThreadId::kVideo);

  mojo::PendingRemote<media::mojom::VideoEncodeAcceleratorProvider>
      vea_provider;
  gpu.CreateVideoEncodeAcceleratorProvider(
      vea_provider.InitWithNewPipeAndPassReceiver());

  return UniquePtr(new MirroringGpuFactoriesFactory(
                       std::move(cast_environment), std::move(gpu_channel_host),
                       std::move(vea_provider), std::move(context_lost_cb),
                       std::move(context_configured_cb)),
                   base::OnTaskRunnerDeleter(std::move(video_runner)));
}

MirroringGpuFactoriesFactory::MirroringGpuFactoriesFactory(
    scoped_refptr<CastEnvironment> cast_environment,
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
    mojo::PendingRemote<media::mojom::VideoEncodeAcceleratorProvider>
        vea_provider,
    base::OnceClosure context_lost_cb,
    ContextConfiguredCallback context_configured_cb)
    : cast_environment_(std::move(cast_environment)),
      context_lost_cb_(std::move(context_lost_cb)),
      context_configured_cb_(std::move(context_configured_cb)) {
  CHECK(cast_environment_->CurrentlyOn(CastEnvironment::ThreadId::kMain));
  CHECK(gpu_channel_host);

  static constexpr int32_t kStreamId = 0;

  context_provider_ = viz::ContextProviderCommandBuffer::CreateForRaster(
      gpu_channel_host, kStreamId, gpu::SchedulingPriority::kHigh,
      GURL("chrome://gpu/CastStreaming"), /*automatic_flushes=*/false,
      /*support_locking=*/false, gpu::SharedMemoryLimits::ForMailboxContext(),
      viz::command_buffer_metrics::ContextType::VIDEO_CAPTURE);

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

  // NOTE: this Unretained is safe because deletion of `this` is posted to the
  // VIDEO thread via base::OnTaskRunnerDeleter, so this task will always run
  // before `this` is destroyed.
  cast_environment_->PostTask(
      CastEnvironment::ThreadId::kVideo, FROM_HERE,
      base::BindOnce(&MirroringGpuFactoriesFactory::BindOnVideoThread,
                     base::Unretained(this)));
}

MirroringGpuFactoriesFactory::~MirroringGpuFactoriesFactory() {
  CHECK(cast_environment_->CurrentlyOn(CastEnvironment::ThreadId::kVideo));
  if (context_provider_) {
    context_provider_->RemoveObserver(this);
  }
  // Destroy the accelerator factories before releasing `context_provider_`,
  // since `instance_` holds a reference to it. Done explicitly rather than
  // relying on reverse member-declaration order.
  instance_.reset();
}

media::GpuVideoAcceleratorFactories&
MirroringGpuFactoriesFactory::GetInstance() {
  CHECK(cast_environment_->CurrentlyOn(CastEnvironment::ThreadId::kMain));
  CHECK(instance_);
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

  auto* command_buffer_proxy = context_provider_->GetCommandBufferProxy();
  if (command_buffer_proxy) {
    command_buffer_proxy->GetGpuChannel().GetChannelToken(base::BindOnce(
        &MirroringGpuFactoriesFactory::OnChannelTokenReady,
        video_weak_factory_.GetWeakPtr(), command_buffer_proxy->route_id()));
  }
}

void MirroringGpuFactoriesFactory::OnChannelTokenReady(
    int32_t route_id,
    const base::UnguessableToken& channel_token) {
  CHECK(cast_environment_->CurrentlyOn(CastEnvironment::ThreadId::kVideo));
  if (context_configured_cb_) {
    cast_environment_->PostTask(
        CastEnvironment::ThreadId::kMain, FROM_HERE,
        base::BindOnce(std::move(context_configured_cb_), channel_token,
                       route_id));
  }
}

void MirroringGpuFactoriesFactory::OnContextLost() {
  CHECK(cast_environment_->CurrentlyOn(CastEnvironment::ThreadId::kVideo));
  if (context_lost_cb_) {
    // `context_lost_cb_` may destroy `this`, so it is important that it is
    // called last in this method.
    std::move(context_lost_cb_).Run();
  }
}

}  // namespace mirroring
