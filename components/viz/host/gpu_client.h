// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_HOST_GPU_CLIENT_H_
#define COMPONENTS_VIZ_HOST_GPU_CLIENT_H_

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/host/gpu_client_delegate.h"
#include "components/viz/host/gpu_host_impl.h"
#include "components/viz/host/viz_host_export.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/ws/public/mojom/gpu.mojom.h"

namespace viz {

class VIZ_HOST_EXPORT GpuClient : public ws::mojom::GpuMemoryBufferFactory,
                                  public ws::mojom::Gpu {
 public:
  using ConnectionErrorHandlerClosure =
      base::OnceCallback<void(GpuClient* client)>;

  // GpuClient must be destroyed on the thread associated with |task_runner|.
  GpuClient(std::unique_ptr<GpuClientDelegate> delegate,
            int client_id,
            uint64_t client_tracing_id,
            scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  ~GpuClient() override;

  // This needs to be run on the thread associated with |task_runner_|.
  void Add(ws::mojom::GpuRequest request);

  void PreEstablishGpuChannel();

  void SetConnectionErrorHandler(
      ConnectionErrorHandlerClosure connection_error_handler);

  base::WeakPtr<GpuClient> GetWeakPtr();

  // ws::mojom::GpuMemoryBufferFactory overrides:
  void CreateGpuMemoryBuffer(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      ws::mojom::GpuMemoryBufferFactory::CreateGpuMemoryBufferCallback callback)
      override;
  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                              const gpu::SyncToken& sync_token) override;

  // ws::mojom::Gpu overrides:
  void CreateGpuMemoryBufferFactory(
      ws::mojom::GpuMemoryBufferFactoryRequest request) override;
  void EstablishGpuChannel(EstablishGpuChannelCallback callback) override;
  void CreateJpegDecodeAccelerator(
      media::mojom::JpegDecodeAcceleratorRequest jda_request) override;
  void CreateVideoEncodeAcceleratorProvider(
      media::mojom::VideoEncodeAcceleratorProviderRequest vea_provider_request)
      override;

 private:
  enum class ErrorReason {
    // OnError() is being called from the destructor.
    kInDestructor,
    // OnError() is being called because the connection was lost.
    kConnectionLost
  };
  void OnError(ErrorReason reason);
  void OnEstablishGpuChannel(mojo::ScopedMessagePipeHandle channel_handle,
                             const gpu::GPUInfo& gpu_info,
                             const gpu::GpuFeatureInfo& gpu_feature_info,
                             GpuHostImpl::EstablishChannelStatus status);
  void OnCreateGpuMemoryBuffer(CreateGpuMemoryBufferCallback callback,
                               gfx::GpuMemoryBufferHandle handle);
  void ClearCallback();

  std::unique_ptr<GpuClientDelegate> delegate_;
  const int client_id_;
  const uint64_t client_tracing_id_;
  mojo::BindingSet<ws::mojom::GpuMemoryBufferFactory>
      gpu_memory_buffer_factory_bindings_;
  mojo::BindingSet<ws::mojom::Gpu> gpu_bindings_;
  bool gpu_channel_requested_ = false;
  EstablishGpuChannelCallback callback_;
  mojo::ScopedMessagePipeHandle channel_handle_;
  gpu::GPUInfo gpu_info_;
  gpu::GpuFeatureInfo gpu_feature_info_;
  ConnectionErrorHandlerClosure connection_error_handler_;
  // |task_runner_| is associated with the thread |gpu_bindings_| is bound on.
  // GpuClient instance is bound to this thread, and must be destroyed on this
  // thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtrFactory<GpuClient> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuClient);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_HOST_GPU_CLIENT_H_
