// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_RENDERER_RESOURCE_PROVIDER_IMPL_H_
#define COMPONENTS_CAST_STREAMING_RENDERER_RESOURCE_PROVIDER_IMPL_H_

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "components/cast_streaming/public/mojom/renderer_controller.mojom.h"
#include "components/cast_streaming/renderer/demuxer_connector.h"
#include "components/cast_streaming/renderer/public/resource_provider.h"
#include "components/cast_streaming/renderer/renderer_controller_proxy.h"
#include "content/public/renderer/render_frame_observer.h"
#include "media/mojo/mojom/renderer.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "url/gurl.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {
class RenderFrame;
}  // namespace content

namespace media {
class Demuxer;
}  // namespace media

namespace cast_streaming {

// Provides the per-render-frame singleton instances used to pass data and
// playback commands received from the remote sender in the browser process to
// the renderer process.
class ResourceProviderImpl : public ResourceProvider {
 public:
  ResourceProviderImpl();
  ~ResourceProviderImpl() override;

  ResourceProviderImpl(const ResourceProviderImpl&) = delete;
  ResourceProviderImpl& operator=(const ResourceProviderImpl&) = delete;

 private:
  // Resources to be associated with each RenderFrame instance. This class
  // serves to tie their lifetimes to that of the |render_frame| with which
  // they are associated, and ensures that their destruction occurs when any
  // such resource becomes invalid.
  class PerRenderFrameResources : public content::RenderFrameObserver {
   public:
    using EndOfLifeCB = base::OnceCallback<void()>;

    // |end_of_life_callback| is the callback to be provided to both
    // |cast_streaming_receiver_| as the RenderFrameDeletionCB and to
    // |renderer_controller_proxy_| as the MojoDisconnectCB. It is expected to
    // delete this instance.
    PerRenderFrameResources(content::RenderFrame* render_frame,
                            EndOfLifeCB end_of_life_cb);
    ~PerRenderFrameResources() override;

    DemuxerConnector& demuxer_connector() { return demuxer_connector_; }

    RendererControllerProxy& renderer_controller_proxy() {
      DCHECK(renderer_controller_proxy_);
      return renderer_controller_proxy_.value();
    }

    bool has_renderer_controller_proxy() const {
      return !!renderer_controller_proxy_;
    }

   private:
    // content::RenderFrameObserver implementation.
    void OnDestruct() override;

    // The singleton associated with forming the mojo connection used to pass
    // DecoderBuffers from the browser process into the renderer process's
    // DemuxerStream used by the media pipeline.
    DemuxerConnector demuxer_connector_;

    // The singleton associated with sending playback commands from the browser
    // to the renderer process. Only populated if remoting is enabled.
    absl::optional<RendererControllerProxy> renderer_controller_proxy_;

    // Callback to be called when the first of |cast_streaming_receiver_| or
    // |renderer_controller_proxy_| becomes invalid.
    EndOfLifeCB end_of_life_cb_;
  };

  // ResourceProvider overrides.
  void RenderFrameCreated(content::RenderFrame* render_frame) override;
  std::unique_ptr<media::Demuxer> OverrideDemuxerForUrl(
      content::RenderFrame* render_frame,
      const GURL& url,
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner) override;
  mojo::PendingReceiver<media::mojom::Renderer> GetReceiverImpl(
      content::RenderFrame* render_frame) override;

  // Called by this instance when its corresponding RenderFrame is in the
  // process of being deleted.
  void OnRenderFrameDeleted(int render_frame_id);

  // Map of RenderFrame ID to per-render-frame resources.
  std::map<int, std::unique_ptr<PerRenderFrameResources>>
      render_frame_id_to_resources_map_;
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_RENDERER_RESOURCE_PROVIDER_IMPL_H_
