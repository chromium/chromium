// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/renderer/resource_provider_impl.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "components/cast_streaming/common/public/cast_streaming_url.h"
#include "components/cast_streaming/common/public/features.h"
#include "components/cast_streaming/renderer/frame/frame_injecting_demuxer.h"
#include "media/base/demuxer.h"

namespace cast_streaming {

// static
std::unique_ptr<ResourceProvider> CreateResourceProvider() {
  return std::make_unique<ResourceProviderImpl>();
}

ResourceProviderImpl::ResourceProviderImpl() : weak_factory_(this) {
  per_frame_resources_ =
      std::make_unique<PerRenderFrameResources>(base::BindOnce(
          &ResourceProviderImpl::OnError, weak_factory_.GetWeakPtr()));
}

ResourceProviderImpl::~ResourceProviderImpl() = default;

void ResourceProviderImpl::BindRendererController(
    mojo::PendingAssociatedReceiver<mojom::RendererController> receiver) {
  if (per_frame_resources_) {
    per_frame_resources_->renderer_controller_proxy().BindRendererController(
        std::move(receiver));
  }
}

void ResourceProviderImpl::BindDemuxerConnector(
    mojo::PendingAssociatedReceiver<mojom::DemuxerConnector> receiver) {
  if (per_frame_resources_) {
    per_frame_resources_->demuxer_connector().BindReceiver(std::move(receiver));
  }
}

void ResourceProviderImpl::OnError() {
  per_frame_resources_.reset();
}

ResourceProvider::ReceiverBinder<mojom::RendererController>
ResourceProviderImpl::GetRendererControllerBinder() {
  return base::BindRepeating(&ResourceProviderImpl::BindRendererController,
                             weak_factory_.GetWeakPtr());
}

ResourceProvider::ReceiverBinder<mojom::DemuxerConnector>
ResourceProviderImpl::GetDemuxerConnectorBinder() {
  return base::BindRepeating(&ResourceProviderImpl::BindDemuxerConnector,
                             weak_factory_.GetWeakPtr());
}

std::unique_ptr<media::Demuxer> ResourceProviderImpl::MaybeGetDemuxerOverride(
    const GURL& url,
    scoped_refptr<base::SequencedTaskRunner> media_task_runner) {
  // Do not create a FrameInjectingDemuxer if the Cast Streaming MessagePort
  // was not set in the browser process. This will manifest as an unbound
  // DemuxerConnector object in the renderer process.
  // TODO(crbug.com/40131115): Simplify the instantiation conditions for the
  // FrameInjectingDemuxer.
  if (per_frame_resources_ && IsCastStreamingMediaSourceUrl(url) &&
      per_frame_resources_->demuxer_connector().IsBound()) {
    return std::make_unique<FrameInjectingDemuxer>(
        &per_frame_resources_->demuxer_connector(),
        std::move(media_task_runner));
  }

  return nullptr;
}

mojo::PendingReceiver<media::mojom::Renderer>
ResourceProviderImpl::GetRendererCommandReceiver() {
  DCHECK(per_frame_resources_);
  return per_frame_resources_->renderer_controller_proxy().GetReceiver();
}

ResourceProviderImpl::PerRenderFrameResources::PerRenderFrameResources(
    base::OnceClosure on_error)
    : renderer_controller_proxy_(std::move(on_error)) {}

ResourceProviderImpl::PerRenderFrameResources::~PerRenderFrameResources() =
    default;

}  // namespace cast_streaming
