// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/mirroring_gpu_factories_factory.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "media/mojo/mojom/video_encode_accelerator.mojom.h"
#include "services/viz/public/cpp/gpu/command_buffer_metrics.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "services/viz/public/cpp/gpu/gpu.h"

namespace mirroring {

namespace {

using media::cast::CastEnvironment;

}

MirroringGpuFactoriesFactory::UniquePtr MirroringGpuFactoriesFactory::Create(
    scoped_refptr<CastEnvironment> cast_environment,
    viz::Gpu& gpu,
    base::OnceClosure context_lost_cb,
    ContextConfiguredCallback context_configured_cb) {
  return UniquePtr(new MirroringGpuFactoriesFactory(
                       cast_environment, gpu, std::move(context_lost_cb),
                       std::move(context_configured_cb)),
                   base::OnTaskRunnerDeleter(cast_environment->GetTaskRunner(
                       CastEnvironment::ThreadId::kVideo)));
}

MirroringGpuFactoriesFactory::MirroringGpuFactoriesFactory(
    scoped_refptr<CastEnvironment> cast_environment,
    viz::Gpu& gpu,
    base::OnceClosure context_lost_cb,
    ContextConfiguredCallback context_configured_cb)
    : cast_environment_(std::move(cast_environment)),
      gpu_(gpu),
      context_lost_cb_(std::move(context_lost_cb)),
      context_configured_cb_(std::move(context_configured_cb)) {}

MirroringGpuFactoriesFactory::~MirroringGpuFactoriesFactory() {
  CHECK(cast_environment_->CurrentlyOn(CastEnvironment::ThreadId::kVideo));
  ResetGpuFactories();
}

media::GpuVideoAcceleratorFactories&
MirroringGpuFactoriesFactory::GetInstance() {
  CHECK(cast_environment_->CurrentlyOn(CastEnvironment::ThreadId::kMain));

  // If we have a valid context, return the current instance as it is still
  // valid.
  if (instance_) {
    return *instance_;
  }

  // Finally, create and return a new instance.
  static constexpr int32_t kStreamId = 0;

  auto gpu_channel_host = gpu_->EstablishGpuChannelSync();
  context_provider_ = viz::ContextProviderCommandBuffer::CreateForGL(
      gpu_channel_host, kStreamId, gpu::SchedulingPriority::kHigh,
      GURL(std::string("chrome://gpu/CastStreaming")),
      viz::command_buffer_metrics::ContextType::VIDEO_CAPTURE);

  cast_environment_->PostTask(
      CastEnvironment::ThreadId::kVideo, FROM_HERE,
      base::BindOnce(&MirroringGpuFactoriesFactory::BindOnVideoThread,
                     weak_factory_.GetWeakPtr()));

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

  auto* command_buffer_proxy = context_provider_->GetCommandBufferProxy();
  if (command_buffer_proxy) {
    command_buffer_proxy->GetGpuChannel().GetChannelToken(base::BindOnce(
        &MirroringGpuFactoriesFactory::OnChannelTokenReady,
        weak_factory_.GetWeakPtr(), command_buffer_proxy->route_id()));
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
  ResetGpuFactories();
  if (context_lost_cb_) {
    // `context_lost_cb_` may destroy `this`, so it is important that it is
    // called last in this method.
    std::move(context_lost_cb_).Run();
  }
}

void MirroringGpuFactoriesFactory::ResetGpuFactories() {
  // The GPU factories object, after construction, must only be accessed on the
  // video encoding thread (including for deletion).
  CHECK(cast_environment_->CurrentlyOn(CastEnvironment::ThreadId::kVideo));
  instance_.reset();
  if (context_provider_) {
    context_provider_->RemoveObserver(this);
    context_provider_ = nullptr;
  }
}

}  // namespace mirroring
