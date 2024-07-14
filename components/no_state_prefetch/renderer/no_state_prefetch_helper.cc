// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/no_state_prefetch/renderer/no_state_prefetch_helper.h"

#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "components/no_state_prefetch/common/no_state_prefetch_url_loader_throttle.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/web_local_frame.h"

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
NoStatePrefetchHelper::MaybeCreateThrottle(
    const blink::LocalFrameToken& frame_token) {
  // Currently NoStatePrefetchHelper doesn't work on the background thread.
  if (!content::RenderThread::IsMainThread()) {
    return nullptr;
  }
  blink::WebLocalFrame* web_frame =
      blink::WebLocalFrame::FromFrameToken(frame_token);
  if (!web_frame) {
    return nullptr;
  }
  content::RenderFrame* render_frame =
      content::RenderFrame::FromWebFrame(web_frame);
  auto* helper =
      render_frame
          ? NoStatePrefetchHelper::Get(render_frame->GetMainRenderFrame())
          : nullptr;
  if (!helper)
    return nullptr;

  mojo::PendingRemote<mojom::PrerenderCanceler> canceler;
  render_frame->GetBrowserInterfaceBroker().GetInterface(
      canceler.InitWithNewPipeAndPassReceiver());

  auto throttle =
      std::make_unique<NoStatePrefetchURLLoaderThrottle>(std::move(canceler));
  helper->AddThrottle(*throttle);
  return throttle;
}

// static.
bool NoStatePrefetchHelper::IsPrefetching(
    const content::RenderFrame* render_frame) {
  return NoStatePrefetchHelper::Get(render_frame) != nullptr;
}

void NoStatePrefetchHelper::DidDispatchDOMContentLoadedEvent() {
  prefetch_finished_ = true;
  if (prefetch_count_ == 0)
    SendPrefetchFinished();
}

void NoStatePrefetchHelper::OnDestruct() {
  delete this;
}

void NoStatePrefetchHelper::AddThrottle(
    NoStatePrefetchURLLoaderThrottle& throttle) {
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
    SendPrefetchFinished();
  }
}

void NoStatePrefetchHelper::SendPrefetchFinished() {
  DCHECK(prefetch_count_ == 0 && prefetch_finished_);

  mojo::Remote<mojom::PrerenderCanceler> canceler;
  render_frame()->GetBrowserInterfaceBroker().GetInterface(
      canceler.BindNewPipeAndPassReceiver());
  canceler->CancelPrerenderForNoStatePrefetch();
}

}  // namespace prerender
