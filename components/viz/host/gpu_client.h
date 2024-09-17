// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_HOST_GPU_CLIENT_H_
#define COMPONENTS_VIZ_HOST_GPU_CLIENT_H_

#include <map>
#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "base/task/single_thread_task_runner.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/host/gpu_client_delegate.h"
#include "components/viz/host/gpu_host_impl.h"
#include "components/viz/host/viz_host_export.h"
#include "gpu/ipc/common/client_gmb_interface.mojom.h"
#include "gpu/ipc/common/gpu_disk_cache_type.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/viz/public/mojom/gpu.mojom.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"

namespace viz {

class VIZ_HOST_EXPORT GpuClient : public mojom::Gpu {
 public:
  using ConnectionErrorHandlerClosure =
      base::OnceCallback<void(GpuClient* client)>;

  // GpuClient must be destroyed on the thread associated with |task_runner|.
  GpuClient(std::unique_ptr<GpuClientDelegate> delegate,
            int client_id,
            uint64_t client_tracing_id,
            scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  GpuClient(const GpuClient&) = delete;
  GpuClient& operator=(const GpuClient&) = delete;

  ~GpuClient() override;

  // This needs to be run on the thread associated with |task_runner_|.
  void Add(mojo::PendingReceiver<mojom::Gpu> receiver);

  void PreEstablishGpuChannel();

  // Sets the PID of the client that will use this channel once the PID is
  // known.
  void SetClientPid(base::ProcessId client_pid);

  // Sets/removes disk cache handle(s) that can be used by this channel.
  void SetDiskCacheHandle(const gpu::GpuDiskCacheHandle& handle);
  void RemoveDiskCacheHandles();

  base::WeakPtr<GpuClient> GetWeakPtr();
  void BindWebNNContextProvider(
      mojo::PendingReceiver<webnn::mojom::WebNNContextProvider> receiver);

  // mojom::ClientGmbInterface is direct interface between renderer and GPU
  // process to create GpuMemoryBuffers.
  void CreateClientGpuMemoryBufferFactory(
      mojo::PendingReceiver<gpu::mojom::ClientGmbInterface> receiver) override;

  void EstablishGpuChannel(EstablishGpuChannelCallback callback) override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void CreateJpegDecodeAccelerator(
      mojo::PendingReceiver<chromeos_camera::mojom::MjpegDecodeAccelerator>
          jda_receiver) override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  void CreateVideoEncodeAcceleratorProvider(
      mojo::PendingReceiver<media::mojom::VideoEncodeAcceleratorProvider>
          vea_provider_receiver) override;

 private:
  enum class ErrorReason {
    // OnError() is being called from the destructor.
    kInDestructor,
    // OnError() is being called because the connection was lost.
    kConnectionLost
  };
  void OnError(ErrorReason reason);
  void OnEstablishGpuChannel(
      mojo::ScopedMessagePipeHandle channel_handle,
      const gpu::GPUInfo& gpu_info,
      const gpu::GpuFeatureInfo& gpu_feature_info,
      const gpu::SharedImageCapabilities& shared_image_capabilities,
      GpuHostImpl::EstablishChannelStatus status);
  void ClearCallback();

  std::unique_ptr<GpuClientDelegate> delegate_;
  const int client_id_;
  const uint64_t client_tracing_id_;

  mojo::ReceiverSet<mojom::Gpu> gpu_receivers_;
  bool gpu_channel_requested_ = false;
  EstablishGpuChannelCallback callback_;
  mojo::ScopedMessagePipeHandle channel_handle_;
  gpu::GPUInfo gpu_info_;
  gpu::GpuFeatureInfo gpu_feature_info_;
  gpu::SharedImageCapabilities shared_image_capabilities_;
  // |task_runner_| is associated with the thread |gpu_bindings_| is bound
  // on. GpuClient instance is bound to this thread, and must be destroyed on
  // this thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtrFactory<GpuClient> weak_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_HOST_GPU_CLIENT_H_
