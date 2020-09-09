// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prerender/renderer/prerender_render_frame_observer.h"

#include "components/prerender/common/prerender_types.mojom.h"
#include "components/prerender/renderer/prerender_helper.h"
#include "components/prerender/renderer/prerender_observer_list.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace prerender {

PrerenderRenderFrameObserver::PrerenderRenderFrameObserver(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {
  render_frame->GetAssociatedInterfaceRegistry()->AddInterface(
      base::BindRepeating(
          &PrerenderRenderFrameObserver::OnRenderFrameObserverRequest,
          base::Unretained(this)));
}

PrerenderRenderFrameObserver::~PrerenderRenderFrameObserver() = default;

void PrerenderRenderFrameObserver::OnRenderFrameObserverRequest(
    mojo::PendingAssociatedReceiver<prerender::mojom::PrerenderMessages>
        receiver) {
  receivers_.Add(this, std::move(receiver));
}

void PrerenderRenderFrameObserver::OnDestruct() {
  delete this;
}

void PrerenderRenderFrameObserver::SetIsPrerendering(
    prerender::mojom::PrerenderMode mode,
    const std::string& histogram_prefix) {
  bool is_prerendering = mode != prerender::mojom::PrerenderMode::kNoPrerender;
  if (is_prerendering) {
    // If the PrerenderHelper for this frame already exists, don't create it. It
    // can already be created for subframes during handling of
    // RenderFrameCreated, if the parent frame was prerendering at time of
    // subframe creation.
    auto* prerender_helper = prerender::PrerenderHelper::Get(render_frame());
    if (!prerender_helper) {
      // The PrerenderHelper will destroy itself either after recording
      // histograms or on destruction of the RenderView.
      prerender_helper = new prerender::PrerenderHelper(render_frame(), mode,
                                                        histogram_prefix);
    }
  }

  prerender::PrerenderObserverList::SetIsPrerenderingForFrame(render_frame(),
                                                              is_prerendering);
}

}  // namespace prerender
