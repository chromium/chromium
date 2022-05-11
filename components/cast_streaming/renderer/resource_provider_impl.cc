// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/renderer/resource_provider_impl.h"

#include "base/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "components/cast_streaming/public/cast_streaming_url.h"
#include "components/cast_streaming/public/features.h"
#include "components/cast_streaming/renderer/cast_streaming_demuxer.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "media/base/demuxer.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/web/web_view.h"

namespace cast_streaming {

// static
std::unique_ptr<ResourceProvider> ResourceProvider::Create() {
  return std::make_unique<ResourceProviderImpl>();
}

ResourceProviderImpl::ResourceProviderImpl() = default;

ResourceProviderImpl::~ResourceProviderImpl() = default;

void ResourceProviderImpl::OnRenderFrameDeleted(int render_frame_id) {
  render_frame_id_to_resources_map_.erase(render_frame_id);
}

void ResourceProviderImpl::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  DCHECK(render_frame);

  const int render_frame_id = render_frame->GetRoutingID();
  // base::Unretained() is safe here because this instance will outlive all
  // Render processes.
  auto deletion_cb =
      base::BindRepeating(&ResourceProviderImpl::OnRenderFrameDeleted,
                          base::Unretained(this), render_frame_id);
  std::unique_ptr<PerRenderFrameResources> frame_resources =
      std::make_unique<PerRenderFrameResources>(render_frame,
                                                std::move(deletion_cb));
  auto pair = render_frame_id_to_resources_map_.emplace(
      render_frame_id, std::move(frame_resources));
  DCHECK(pair.second);
}

std::unique_ptr<media::Demuxer> ResourceProviderImpl::OverrideDemuxerForUrl(
    content::RenderFrame* render_frame,
    const GURL& url,
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner) {
  DCHECK(render_frame);

  if (IsCastStreamingMediaSourceUrl(url)) {
    auto iter =
        render_frame_id_to_resources_map_.find(render_frame->GetRoutingID());
    DCHECK(iter != render_frame_id_to_resources_map_.end());

    // Do not create a CastStreamingDemuxer if the Cast Streaming MessagePort
    // was not set in the browser process. This will manifest as an unbound
    // DemuxerProvider object in the renderer process.
    // TODO(crbug.com/1082821): Simplify the instantiation conditions for the
    // CastStreamingDemuxer.
    DCHECK(iter->second);
    DemuxerConnector& demuxer_connector = iter->second->demuxer_connector();
    if (demuxer_connector.IsBound()) {
      return std::make_unique<CastStreamingDemuxer>(
          &demuxer_connector, std::move(media_task_runner));
    }
  }

  return nullptr;
}

mojo::PendingReceiver<media::mojom::Renderer>
ResourceProviderImpl::GetReceiverImpl(content::RenderFrame* frame) {
  DCHECK(frame);

  auto it = render_frame_id_to_resources_map_.find(frame->GetRoutingID());
  DCHECK(it != render_frame_id_to_resources_map_.end());
  DCHECK(it->second);
  DCHECK(it->second->has_renderer_controller_proxy());
  return it->second->renderer_controller_proxy().GetReceiver();
}

ResourceProviderImpl::PerRenderFrameResources::PerRenderFrameResources(
    content::RenderFrame* render_frame,
    EndOfLifeCB end_of_life_callback)
    : content::RenderFrameObserver(render_frame),
      demuxer_connector_(render_frame),
      end_of_life_cb_(std::move(end_of_life_callback)) {
  DCHECK(render_frame);
  DCHECK(end_of_life_cb_);
  if (IsCastRemotingEnabled()) {
    auto callbacks_pair = base::SplitOnceCallback(std::move(end_of_life_cb_));
    end_of_life_cb_ = std::move(callbacks_pair.first);
    renderer_controller_proxy_.emplace(render_frame,
                                       std::move(callbacks_pair.second));
  }
}

ResourceProviderImpl::PerRenderFrameResources::~PerRenderFrameResources() =
    default;

void ResourceProviderImpl::PerRenderFrameResources::OnDestruct() {
  std::move(end_of_life_cb_).Run();
}

}  // namespace cast_streaming
