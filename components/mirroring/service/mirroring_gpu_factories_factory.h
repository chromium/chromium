// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_MIRRORING_GPU_FACTORIES_FACTORY_H_
#define COMPONENTS_MIRRORING_SERVICE_MIRRORING_GPU_FACTORIES_FACTORY_H_

#include <memory>
#include <optional>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/unguessable_token.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "media/cast/cast_environment.h"
#include "media/mojo/clients/mojo_gpu_video_accelerator_factories.h"
#include "media/video/gpu_video_accelerator_factories.h"

namespace gpu {
class GpuChannelHost;
}  // namespace gpu

namespace viz {
class ContextProviderCommandBuffer;
class Gpu;
}  // namespace viz

namespace mirroring {

// Responsible for creating and owning an instance of the
// MojoGpuVideoAcceleratorFactories implementation. Subscribes to the lifetime
// of the viz context and is responsible for handling lost context
// notifications.
//
// When the context is lost, the `context_lost_cb` is called and the instance
// should be assumed to be in an invalid state. The `context_configured_cb` is
// called once the GPU channel token and route ID are available, enabling
// hardware video encoding.
class COMPONENT_EXPORT(MIRRORING_SERVICE) MirroringGpuFactoriesFactory
    : public viz::ContextLostObserver {
 public:
  using ContextConfiguredCallback =
      base::OnceCallback<void(const base::UnguessableToken&, int32_t)>;

  // A custom unique_ptr that ensures the factory is deleted on the VIDEO
  // thread.
  using UniquePtr =
      std::unique_ptr<MirroringGpuFactoriesFactory, base::OnTaskRunnerDeleter>;

  // Factory method to ensure the object is always managed by a UniquePtr
  // that deletes it on the correct thread. Must be called on the MAIN thread.
  // Returns `std::nullopt` if the GPU channel cannot be established, in which
  // case the caller should fall back to software encoding.
  static std::optional<UniquePtr> Create(
      scoped_refptr<media::cast::CastEnvironment> cast_environment,
      viz::Gpu& gpu,
      base::OnceClosure context_lost_cb,
      ContextConfiguredCallback context_configured_cb);

  MirroringGpuFactoriesFactory(const MirroringGpuFactoriesFactory&) = delete;
  MirroringGpuFactoriesFactory& operator=(const MirroringGpuFactoriesFactory&) =
      delete;
  ~MirroringGpuFactoriesFactory() override;

  media::GpuVideoAcceleratorFactories& GetInstance();

  // viz::ContextLostObserver implementation.
  void OnContextLost() override;

 private:
  MirroringGpuFactoriesFactory(
      scoped_refptr<media::cast::CastEnvironment> cast_environment,
      scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
      mojo::PendingRemote<media::mojom::VideoEncodeAcceleratorProvider>
          vea_provider,
      base::OnceClosure context_lost_cb,
      ContextConfiguredCallback context_configured_cb);

  // Executes bind calls that must occur on the VIDEO thread.
  void BindOnVideoThread();

  void OnChannelTokenReady(int32_t route_id,
                           const base::UnguessableToken& channel_token);

  scoped_refptr<media::cast::CastEnvironment> cast_environment_;
  scoped_refptr<viz::ContextProviderCommandBuffer> context_provider_;
  base::OnceClosure context_lost_cb_;
  ContextConfiguredCallback context_configured_cb_;
  std::unique_ptr<media::MojoGpuVideoAcceleratorFactories> instance_;

  // Weak pointer factory used exclusively on the VIDEO thread.
  base::WeakPtrFactory<MirroringGpuFactoriesFactory> video_weak_factory_{this};
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_MIRRORING_GPU_FACTORIES_FACTORY_H_
