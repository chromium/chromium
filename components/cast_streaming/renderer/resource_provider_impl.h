// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_RENDERER_RESOURCE_PROVIDER_IMPL_H_
#define COMPONENTS_CAST_STREAMING_RENDERER_RESOURCE_PROVIDER_IMPL_H_

#include <map>
#include <memory>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/cast_streaming/common/public/mojom/demuxer_connector.mojom.h"
#include "components/cast_streaming/common/public/mojom/renderer_controller.mojom.h"
#include "components/cast_streaming/renderer/control/renderer_controller_proxy.h"
#include "components/cast_streaming/renderer/frame/demuxer_connector.h"
#include "components/cast_streaming/renderer/public/resource_provider.h"
#include "media/mojo/mojom/renderer.mojom.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "url/gurl.h"

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
  class PerRenderFrameResources {
   public:
    // |on_error| is the callback to be provided to |renderer_controller_proxy_|
    // as the MojoDisconnectCB. It is expected to delete this instance.
    explicit PerRenderFrameResources(base::OnceClosure on_error);
    ~PerRenderFrameResources();

    DemuxerConnector& demuxer_connector() { return demuxer_connector_; }

    RendererControllerProxy& renderer_controller_proxy() {
      return renderer_controller_proxy_;
    }

   private:
    // The singleton associated with forming the mojo connection used to pass
    // DecoderBuffers from the browser process into the renderer process's
    // DemuxerStream used by the media pipeline.
    DemuxerConnector demuxer_connector_;

    // The singleton associated with sending playback commands from the browser
    // to the renderer process.
    RendererControllerProxy renderer_controller_proxy_;
  };

  void BindRendererController(
      mojo::PendingAssociatedReceiver<mojom::RendererController> receiver);
  void BindDemuxerConnector(
      mojo::PendingAssociatedReceiver<mojom::DemuxerConnector> receiver);

  void OnError();

  // ResourceProvider overrides.
  ReceiverBinder<mojom::RendererController> GetRendererControllerBinder()
      override;
  ReceiverBinder<mojom::DemuxerConnector> GetDemuxerConnectorBinder() override;
  std::unique_ptr<media::Demuxer> MaybeGetDemuxerOverride(
      const GURL& url,
      scoped_refptr<base::SequencedTaskRunner> media_task_runner) override;
  mojo::PendingReceiver<media::mojom::Renderer> GetRendererCommandReceiver()
      override;

  std::unique_ptr<PerRenderFrameResources> per_frame_resources_;

  base::WeakPtrFactory<ResourceProviderImpl> weak_factory_;
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_RENDERER_RESOURCE_PROVIDER_IMPL_H_
