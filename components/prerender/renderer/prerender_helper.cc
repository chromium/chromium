// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prerender/renderer/prerender_helper.h"

#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "components/prerender//common/prerender_url_loader_throttle.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_view.h"

namespace prerender {

PrerenderHelper::PrerenderHelper(content::RenderFrame* render_frame,
                                 prerender::mojom::PrerenderMode prerender_mode,
                                 const std::string& histogram_prefix)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<PrerenderHelper>(render_frame),
      prerender_mode_(prerender_mode),
      histogram_prefix_(histogram_prefix),
      start_time_(base::TimeTicks::Now()) {
  DCHECK_NE(prerender_mode_, prerender::mojom::PrerenderMode::kNoPrerender);
}

PrerenderHelper::~PrerenderHelper() = default;

// static
std::unique_ptr<blink::URLLoaderThrottle> PrerenderHelper::MaybeCreateThrottle(
    int render_frame_id) {
  content::RenderFrame* render_frame =
      content::RenderFrame::FromRoutingID(render_frame_id);
  auto* prerender_helper =
      render_frame ? PrerenderHelper::Get(
                         render_frame->GetRenderView()->GetMainRenderFrame())
                   : nullptr;
  if (!prerender_helper)
    return nullptr;

  mojo::PendingRemote<prerender::mojom::PrerenderCanceler> canceler;
  render_frame->GetBrowserInterfaceBroker()->GetInterface(
      canceler.InitWithNewPipeAndPassReceiver());

  auto throttle = std::make_unique<PrerenderURLLoaderThrottle>(
      prerender_helper->prerender_mode(), prerender_helper->histogram_prefix(),
      std::move(canceler));
  prerender_helper->AddThrottle(*throttle);
  return throttle;
}

// static.
bool PrerenderHelper::IsPrerendering(const content::RenderFrame* render_frame) {
  return PrerenderHelper::GetPrerenderMode(render_frame) !=
         prerender::mojom::PrerenderMode::kNoPrerender;
}

// static.
prerender::mojom::PrerenderMode PrerenderHelper::GetPrerenderMode(
    const content::RenderFrame* render_frame) {
  PrerenderHelper* helper = PrerenderHelper::Get(render_frame);
  if (!helper)
    return prerender::mojom::PrerenderMode::kNoPrerender;

  DCHECK_NE(helper->prerender_mode_,
            prerender::mojom::PrerenderMode::kNoPrerender);
  return helper->prerender_mode_;
}

void PrerenderHelper::DidFinishDocumentLoad() {
  if (prerender_mode_ != prerender::mojom::PrerenderMode::kPrefetchOnly)
    return;

  parsed_time_ = base::TimeTicks::Now();
  prefetch_finished_ = true;
  if (prefetch_count_ == 0)
    SendPrefetchFinished();
}

void PrerenderHelper::OnDestruct() {
  delete this;
}

void PrerenderHelper::AddThrottle(PrerenderURLLoaderThrottle& throttle) {
  // Keep track of how many pending throttles we have, as we want to defer
  // sending the "prefetch finished" signal until they are destroyed. This is
  // important since that signal tells the browser that it can tear down this
  // renderer which could interrupt subresource prefetching.
  if (prerender_mode_ == prerender::mojom::PrerenderMode::kPrefetchOnly) {
    prefetch_count_++;
    throttle.set_destruction_closure(base::BindOnce(
        &PrerenderHelper::OnThrottleDestroyed, weak_factory_.GetWeakPtr()));
  }
}

void PrerenderHelper::OnThrottleDestroyed() {
  if (--prefetch_count_ == 0 && prefetch_finished_) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Prerender.NoStatePrefetchRendererLifetimeExtension",
        base::TimeTicks::Now() - parsed_time_);
    SendPrefetchFinished();
  }
}

void PrerenderHelper::SendPrefetchFinished() {
  DCHECK(prefetch_count_ == 0 && prefetch_finished_);
  UMA_HISTOGRAM_MEDIUM_TIMES("Prerender.NoStatePrefetchRendererParseTime",
                             parsed_time_ - start_time_);

  mojo::Remote<prerender::mojom::PrerenderCanceler> canceler;
  render_frame()->GetBrowserInterfaceBroker()->GetInterface(
      canceler.BindNewPipeAndPassReceiver());
  canceler->CancelPrerenderForNoStatePrefetch();
}

}  // namespace prerender
