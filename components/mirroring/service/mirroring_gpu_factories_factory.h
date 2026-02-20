// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_MIRRORING_GPU_FACTORIES_FACTORY_H_
#define COMPONENTS_MIRRORING_SERVICE_MIRRORING_GPU_FACTORIES_FACTORY_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/unguessable_token.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "media/cast/cast_environment.h"
#include "media/mojo/clients/mojo_gpu_video_accelerator_factories.h"
#include "media/video/gpu_video_accelerator_factories.h"

namespace viz {
class Gpu;
}

namespace mirroring {

// Responsible for creating and owning an instance of the
// MojoGpuVideoAcceleratorFactories implementation. Subscribes to the lifetime
// of the viz context and is responsible for handling lost context
// notifications.
//
// As part of the mirroring service, we generally expect that there is only
// When the context
// is lost, the `context_lost_cb` is called and the instance should be assumed
// to be in an invalid state.
// The `context_configured_cb` is called once the GPU channel token and route ID
// are available, enabling hardware video encoding.
class MirroringGpuFactoriesFactory : public viz::ContextLostObserver {
 public:
  using ContextConfiguredCallback =
      base::OnceCallback<void(const base::UnguessableToken&, int32_t)>;

  MirroringGpuFactoriesFactory(
      scoped_refptr<media::cast::CastEnvironment> cast_environment,
      viz::Gpu& gpu,
      base::OnceClosure context_lost_cb,
      ContextConfiguredCallback context_configured_cb);

  MirroringGpuFactoriesFactory(const MirroringGpuFactoriesFactory&) = delete;
  MirroringGpuFactoriesFactory& operator=(const MirroringGpuFactoriesFactory&) =
      delete;
  MirroringGpuFactoriesFactory(MirroringGpuFactoriesFactory&&);
  MirroringGpuFactoriesFactory& operator=(MirroringGpuFactoriesFactory&&);
  ~MirroringGpuFactoriesFactory() override;

  media::GpuVideoAcceleratorFactories& GetInstance();

 private:
  // Executes bind calls that must occur on the VIDEO thread.
  void BindOnVideoThread();

  void OnChannelTokenReady(int32_t route_id,
                           const base::UnguessableToken& channel_token);

  // viz::ContextLostObserver overrides.
  void OnContextLost() override;

  // Properly destructs `instance_` and `context_provider_` on the VIDEO thread.
  void DestroyInstanceOnVideoThread();

  scoped_refptr<media::cast::CastEnvironment> cast_environment_;
  raw_ref<viz::Gpu, DisableDanglingPtrDetection> gpu_;
  scoped_refptr<viz::ContextProviderCommandBuffer> context_provider_;
  base::OnceClosure context_lost_cb_;
  ContextConfiguredCallback context_configured_cb_;
  std::unique_ptr<media::MojoGpuVideoAcceleratorFactories> instance_;
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_MIRRORING_GPU_FACTORIES_FACTORY_H_
