// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/renderer/public/demuxer_provider.h"

#include "base/task/single_thread_task_runner.h"
#include "components/cast_streaming/public/cast_streaming_url.h"
#include "components/cast_streaming/renderer/cast_streaming_demuxer.h"
#include "components/cast_streaming/renderer/cast_streaming_render_frame_observer.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/web/web_view.h"

namespace cast_streaming {

DemuxerProvider::DemuxerProvider() = default;

DemuxerProvider::~DemuxerProvider() = default;

void DemuxerProvider::OnRenderFrameDeleted(int render_frame_id) {
  size_t count = render_frame_id_to_observer_map_.erase(render_frame_id);
  DCHECK_EQ(count, 1u);
}

void DemuxerProvider::RenderFrameCreated(content::RenderFrame* render_frame) {
  int render_frame_id = render_frame->GetRoutingID();
  auto render_frame_observer =
      std::make_unique<CastStreamingRenderFrameObserver>(
          render_frame, base::BindOnce(&DemuxerProvider::OnRenderFrameDeleted,
                                       base::Unretained(this)));
  auto render_frame_observer_iter = render_frame_id_to_observer_map_.emplace(
      render_frame_id, std::move(render_frame_observer));
  DCHECK(render_frame_observer_iter.second);
}

std::unique_ptr<media::Demuxer> DemuxerProvider::OverrideDemuxerForUrl(
    content::RenderFrame* render_frame,
    const GURL& url,
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner) {
  if (IsCastStreamingMediaSourceUrl(url)) {
    auto iter =
        render_frame_id_to_observer_map_.find(render_frame->GetRoutingID());
    DCHECK(iter != render_frame_id_to_observer_map_.end());

    // Do not create a CastStreamingDemuxer if the Cast Streaming MessagePort
    // was not set in the browser process. This will manifest as an unbound
    // CastStreamingReceiver object in the renderer process.
    // TODO(crbug.com/1082821): Simplify the instantiation conditions for the
    // CastStreamingDemuxer.
    if (iter->second->cast_streaming_receiver()->IsBound()) {
      return std::make_unique<cast_streaming::CastStreamingDemuxer>(
          iter->second->cast_streaming_receiver(), media_task_runner);
    }
  }

  return nullptr;
}

}  // namespace cast_streaming
