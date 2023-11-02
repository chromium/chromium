// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NO_STATE_PREFETCH_RENDERER_PRERENDER_RENDER_FRAME_OBSERVER_H_
#define COMPONENTS_NO_STATE_PREFETCH_RENDERER_PRERENDER_RENDER_FRAME_OBSERVER_H_

#include "components/no_state_prefetch/common/render_frame_prerender_messages.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace content {
class RenderFrame;
}

namespace prerender {

class PrerenderRenderFrameObserver
    : public content::RenderFrameObserver,
      public prerender::mojom::PrerenderMessages {
 public:
  explicit PrerenderRenderFrameObserver(content::RenderFrame* render_frame);
  ~PrerenderRenderFrameObserver() override;

  // prerender::mojom::PrerenderMessages:
  void SetIsPrerendering(const std::string& histogram_prefix) override;

 private:
  // RenderFrameObserver implementation.
  void OnDestruct() override;

  void OnRenderFrameObserverRequest(
      mojo::PendingAssociatedReceiver<prerender::mojom::PrerenderMessages>
          receiver);

  mojo::AssociatedReceiverSet<prerender::mojom::PrerenderMessages> receivers_;
};

}  // namespace prerender

#endif  // COMPONENTS_NO_STATE_PREFETCH_RENDERER_PRERENDER_RENDER_FRAME_OBSERVER_H_
