// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/no_state_prefetch/renderer/no_state_prefetch_helper.h"

#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "components/no_state_prefetch/common/prerender_url_loader_throttle.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_view.h"

namespace prerender {

NoStatePrefetchHelper::NoStatePrefetchHelper(
    content::RenderFrame* render_frame,
    const std::string& histogram_prefix)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<NoStatePrefetchHelper>(render_frame),
      histogram_prefix_(histogram_prefix),
      start_time_(base::TimeTicks::Now()) {}

NoStatePrefetchHelper::~NoStatePrefetchHelper() = default;

// static
std::unique_ptr<blink::URLLoaderThrottle>
NoStatePrefetchHelper::MaybeCreateThrottle(int render_frame_id) {
  content::RenderFrame* render_frame =
      content::RenderFrame::FromRoutingID(render_frame_id);
  auto* helper =
      render_frame
          ? NoStatePrefetchHelper::Get(render_frame->GetMainRenderFrame())
          : nullptr;
  if (!helper)
    return nullptr;

  mojo::PendingRemote<mojom::PrerenderCanceler> canceler;
  render_frame->GetBrowserInterfaceBroker()->GetInterface(
      canceler.InitWithNewPipeAndPassReceiver());

  auto throttle = std::make_unique<PrerenderURLLoaderThrottle>(
      helper->histogram_prefix(), std::move(canceler));
  helper->AddThrottle(*throttle);
  return throttle;
}

// static.
bool NoStatePrefetchHelper::IsPrefetching(
    const content::RenderFrame* render_frame) {
  return NoStatePrefetchHelper::Get(render_frame) != nullptr;
}

void NoStatePrefetchHelper::DidDispatchDOMContentLoadedEvent() {
  parsed_time_ = base::TimeTicks::Now();
  prefetch_finished_ = true;
  if (prefetch_count_ == 0)
    SendPrefetchFinished();
}

void NoStatePrefetchHelper::OnDestruct() {
  delete this;
}

void NoStatePrefetchHelper::AddThrottle(PrerenderURLLoaderThrottle& throttle) {
  // Keep track of how many pending throttles we have, as we want to defer
  // sending the "prefetch finished" signal until they are destroyed. This is
  // important since that signal tells the browser that it can tear down this
  // renderer which could interrupt subresource prefetching.
  prefetch_count_++;
  throttle.set_destruction_closure(base::BindOnce(
      &NoStatePrefetchHelper::OnThrottleDestroyed, weak_factory_.GetWeakPtr()));
}

void NoStatePrefetchHelper::OnThrottleDestroyed() {
  if (--prefetch_count_ == 0 && prefetch_finished_) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Prerender.NoStatePrefetchRendererLifetimeExtension",
        base::TimeTicks::Now() - parsed_time_);
    SendPrefetchFinished();
  }
}

void NoStatePrefetchHelper::SendPrefetchFinished() {
  DCHECK(prefetch_count_ == 0 && prefetch_finished_);
  UMA_HISTOGRAM_MEDIUM_TIMES("Prerender.NoStatePrefetchRendererParseTime",
                             parsed_time_ - start_time_);

  mojo::Remote<mojom::PrerenderCanceler> canceler;
  render_frame()->GetBrowserInterfaceBroker()->GetInterface(
      canceler.BindNewPipeAndPassReceiver());
  canceler->CancelPrerenderForNoStatePrefetch();
}

}  // namespace prerender
