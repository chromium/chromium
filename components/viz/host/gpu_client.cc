// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/host/gpu_client.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/numerics/checked_math.h"
#include "base/task/single_thread_task_runner.h"
#include "components/viz/host/gpu_host_impl.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "services/viz/privileged/mojom/gl/gpu_service.mojom.h"

namespace viz {

GpuClient::GpuClient(std::unique_ptr<GpuClientDelegate> delegate,
                     int client_id,
                     uint64_t client_tracing_id,
                     bool enable_extra_handles_validation,
                     scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : delegate_(std::move(delegate)),
      client_id_(client_id),
      client_tracing_id_(client_tracing_id),
      enable_extra_handles_validation_(enable_extra_handles_validation),
      task_runner_(std::move(task_runner)) {
  DCHECK(delegate_);
  gpu_receivers_.set_disconnect_handler(
      base::BindRepeating(&GpuClient::OnError, base::Unretained(this),
                          ErrorReason::kConnectionLost));
}

GpuClient::~GpuClient() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  gpu_receivers_.Clear();
  OnError(ErrorReason::kInDestructor);
}

void GpuClient::Add(mojo::PendingReceiver<mojom::Gpu> receiver) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  gpu_receivers_.Add(this, std::move(receiver));
}

void GpuClient::OnError(ErrorReason reason) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  ClearCallback();
}

void GpuClient::InitializeGpuChannelForNewRenderer(
    mojo::ScopedMessagePipeHandle invitation_handle) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!invitation_handle.is_valid()) {
    // Fall back to the legacy behavior of pre-allocating a channel via the
    // standard method.
    EstablishGpuChannel(EstablishGpuChannelCallback());
    return;
  }

  // We are launching a new renderer process. If this is a relaunch after a
  // crash, there might be a pending callback or stale channel from the dead
  // renderer. Reset all of them to guarantee a clean slate.
  ClearCallback();
  channel_handle_.reset();

  GpuHostImpl* gpu_host = delegate_->EnsureGpuHost();
  if (!gpu_host) {
    return;
  }

  // A new initialization request cancels any existing pending request (either
  // for a new renderer or standard establish). This is safe because
  // initialization for a new renderer implies a new renderer process, so any
  // previous requests for this client ID are stale (e.g. from a crashed
  // renderer). This prevents race conditions and live-lock situations where
  // stale requests interfere with new ones.
  if (init_with_invitation_pending_ || channel_handle_pending_) {
    gpu_host->CancelEstablishGpuChannel(client_id_);
  }

  channel_handle_pending_ = false;
  init_with_invitation_pending_ = true;

  // Since the client pipe is sent to the renderer via mojo invitation, pass an
  // empty pipe for the client end for `OnEstablishGpuChannel()`. Send the other
  // end of the invitation pipe to the GPU.
  EstablishGpuChannelInternal(/*is_for_init_with_invitation=*/true, gpu_host,
                              std::move(invitation_handle),
                              mojo::ScopedMessagePipeHandle());
}

void GpuClient::EstablishGpuChannelInternal(
    bool is_for_init_with_invitation,
    GpuHostImpl* gpu_host,
    mojo::ScopedMessagePipeHandle service_handle,
    mojo::ScopedMessagePipeHandle client_handle) {
  gpu_host->EstablishGpuChannel(
      client_id_, client_tracing_id_, /*is_gpu_host=*/false,
      enable_extra_handles_validation_, /*sync=*/false,
      std::move(service_handle),
      base::BindOnce(&GpuClient::OnEstablishGpuChannel,
                     weak_factory_.GetWeakPtr(), is_for_init_with_invitation,
                     std::move(client_handle)));
}

void GpuClient::SetClientPid(base::ProcessId client_pid) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GpuClient::SetClientPid,
                                  weak_factory_.GetWeakPtr(), client_pid));
    return;
  }

  if (GpuHostImpl* gpu_host = delegate_->EnsureGpuHost())
    gpu_host->SetChannelClientPid(client_id_, client_pid);
}

void GpuClient::SetDiskCacheHandle(const gpu::GpuDiskCacheHandle& handle) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&GpuClient::SetDiskCacheHandle,
                                          weak_factory_.GetWeakPtr(), handle));
    return;
  }

  if (GpuHostImpl* gpu_host = delegate_->EnsureGpuHost())
    gpu_host->SetChannelDiskCacheHandle(client_id_, handle);
}

void GpuClient::RemoveDiskCacheHandles() {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&GpuClient::RemoveDiskCacheHandles,
                                          weak_factory_.GetWeakPtr()));
    return;
  }

  if (GpuHostImpl* gpu_host = delegate_->EnsureGpuHost())
    gpu_host->RemoveChannelDiskCacheHandles(client_id_);
}

base::WeakPtr<GpuClient> GpuClient::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void GpuClient::BindWebNNContextProvider(
    mojo::PendingReceiver<webnn::mojom::WebNNContextProvider> receiver,
    bool is_incognito) {
  if (auto* gpu_host = delegate_->EnsureGpuHost()) {
    gpu_host->gpu_service()->BindWebNNContextProvider(
        std::move(receiver), client_id_, client_tracing_id_, is_incognito);
  }
}

void GpuClient::OnEstablishGpuChannel(
    bool is_for_init_with_invitation,
    mojo::ScopedMessagePipeHandle channel_handle,
    const gpu::GPUInfo& gpu_info,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    const gpu::SharedImageCapabilities& shared_image_capabilities,
    GpuHostImpl::EstablishChannelStatus status) {
  if (status != GpuHostImpl::EstablishChannelStatus::kSuccess) {
    // Since the pipes are created and then passed immediately to this function
    // as the callback to EstablishGpuChannel, it can be valid even though the
    // status is not kSuccess. In this case, reset the pipe.
    channel_handle.reset();
  } else {
    // For kSuccess cases, we must get a valid pipe, except for the new renderer
    // case, where we would always pass in an invalid pipe.
    DCHECK_EQ(channel_handle.is_valid(), !is_for_init_with_invitation);
  }

  if (is_for_init_with_invitation) {
    CHECK(!channel_handle_pending_);
    CHECK(init_with_invitation_pending_);
    init_with_invitation_pending_ = false;
  } else {
    CHECK(channel_handle_pending_);
    CHECK(!init_with_invitation_pending_);
    channel_handle_pending_ = false;
  }

  if (callback_for_testing_) {
    std::move(callback_for_testing_)
        .Run(status == GpuHostImpl::EstablishChannelStatus::kSuccess);
  }

  if (is_for_init_with_invitation) {
    // The client pipe is already sent to the renderer via mojo invitation
    // during renderer initialization. Note that if the channel establishment
    // failed, the renderer would notice the connection is lost and trigger a
    // regular EstablishGPUChannel call, so there's no need to handle that case
    // here too.
    return;
  }

  if (status == GpuHostImpl::EstablishChannelStatus::kGpuHostInvalid) {
    // GPU process may have crashed or been killed. Try again.
    EstablishGpuChannel(std::move(callback_));
    return;
  }

  if (callback_) {
    // A request is waiting.
    std::move(callback_).Run(client_id_, std::move(channel_handle), gpu_info,
                             gpu_feature_info, shared_image_capabilities);
    return;
  }

  // This is the case where we initialize a channel early before a request
  // arrives.
  if (status == GpuHostImpl::EstablishChannelStatus::kSuccess) {
    // Cache the channel for a future request.
    channel_handle_ = std::move(channel_handle);
    gpu_info_ = gpu_info;
    gpu_feature_info_ = gpu_feature_info;
    shared_image_capabilities_ = shared_image_capabilities;
  }
}

void GpuClient::ClearCallback() {
  if (!callback_)
    return;
  EstablishGpuChannelCallback callback = std::move(callback_);
  std::move(callback).Run(client_id_, mojo::ScopedMessagePipeHandle(),
                          gpu::GPUInfo(), gpu::GpuFeatureInfo(),
                          gpu::SharedImageCapabilities());
}

void GpuClient::EstablishGpuChannel(EstablishGpuChannelCallback callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  // At most one channel should be requested. So clear previous request first.
  ClearCallback();

  if (channel_handle_.is_valid()) {
    // If a channel has been initialized early and cached,
    //   1) if callback is valid, return it right away.
    //   2) if callback is empty, it's InitializeGpuChannelForNewRenderer()
    //   being called more than once, no need to do anything.
    if (callback) {
      std::move(callback).Run(client_id_, std::move(channel_handle_), gpu_info_,
                              gpu_feature_info_, shared_image_capabilities_);
      DCHECK(!channel_handle_.is_valid());
    }
    return;
  }

  GpuHostImpl* gpu_host = delegate_->EnsureGpuHost();
  if (init_with_invitation_pending_) {
    if (gpu_host) {
      // Cancel the early initialization request in `GpuHostImpl` to ensure its
      // reply doesn't consume the callback for the new request we are about to
      // make.
      gpu_host->CancelEstablishGpuChannel(client_id_);
    }
    init_with_invitation_pending_ = false;
  }

  if (!gpu_host) {
    if (callback) {
      std::move(callback).Run(client_id_, mojo::ScopedMessagePipeHandle(),
                              gpu::GPUInfo(), gpu::GpuFeatureInfo(),
                              gpu::SharedImageCapabilities());
    }
    return;
  }

  callback_ = std::move(callback);
  if (channel_handle_pending_) {
    // An `EstablishGpuChannel()` is already in-flight. Don't trigger another
    // call.
    return;
  }

  channel_handle_pending_ = true;

  // Allocate a new pipe. Pass the client end to be saved in `channel_handle_`
  // on `OnEstablishGpuChannel()`, send the other to the GPU.
  mojo::MessagePipe pipe;
  EstablishGpuChannelInternal(/*is_for_init_with_invitation=*/false, gpu_host,
                              std::move(pipe.handle1), std::move(pipe.handle0));
}

void GpuClient::SetEstablishGpuChannelCallbackForTesting(
    base::OnceCallback<void(bool)> callback) {
  callback_for_testing_ = std::move(callback);
}

#if BUILDFLAG(IS_CHROMEOS)
void GpuClient::CreateJpegDecodeAccelerator(
    mojo::PendingReceiver<chromeos_camera::mojom::MjpegDecodeAccelerator>
        jda_receiver) {
  if (auto* gpu_host = delegate_->EnsureGpuHost()) {
    gpu_host->gpu_service()->CreateJpegDecodeAccelerator(
        std::move(jda_receiver));
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void GpuClient::CreateVideoEncodeAcceleratorProvider(
    mojo::PendingReceiver<media::mojom::VideoEncodeAcceleratorProvider>
        vea_provider_receiver) {
  if (auto* gpu_host = delegate_->EnsureGpuHost()) {
    gpu_host->gpu_service()->CreateVideoEncodeAcceleratorProvider(
        std::move(vea_provider_receiver));
  }
}

}  // namespace viz
