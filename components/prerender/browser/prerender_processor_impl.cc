// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prerender/browser/prerender_processor_impl.h"

#include "components/prerender/browser/prerender_link_manager.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/referrer.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace prerender {

PrerenderProcessorImpl::PrerenderProcessorImpl(
    int render_process_id,
    int render_frame_id,
    const url::Origin& initiator_origin,
    std::unique_ptr<PrerenderProcessorImplDelegate> delegate)
    : render_process_id_(render_process_id),
      render_frame_id_(render_frame_id),
      initiator_origin_(initiator_origin),
      delegate_(std::move(delegate)) {}

PrerenderProcessorImpl::~PrerenderProcessorImpl() = default;

// static
void PrerenderProcessorImpl::Create(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<blink::mojom::PrerenderProcessor> receiver,
    std::unique_ptr<PrerenderProcessorImplDelegate> delegate) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<PrerenderProcessorImpl>(
          frame_host->GetProcess()->GetID(), frame_host->GetRoutingID(),
          frame_host->GetLastCommittedOrigin(), std::move(delegate)),
      std::move(receiver));
}

void PrerenderProcessorImpl::Start(
    blink::mojom::PrerenderAttributesPtr attributes,
    mojo::PendingRemote<blink::mojom::PrerenderProcessorClient> client) {
  if (!initiator_origin_.opaque() &&
      !content::ChildProcessSecurityPolicy::GetInstance()
           ->CanAccessDataForOrigin(render_process_id_,
                                    initiator_origin_.GetURL())) {
    mojo::ReportBadMessage("PPI_INVALID_INITIATOR_ORIGIN");
    return;
  }

  // Start() must be called only one time.
  if (prerender_id_) {
    mojo::ReportBadMessage("PPI_START_TWICE");
    return;
  }

  auto* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id_, render_frame_id_);
  if (!render_frame_host)
    return;

  auto* link_manager = GetPrerenderLinkManager();
  if (!link_manager)
    return;

  DCHECK(!prerender_id_);
  prerender_id_ = link_manager->OnStartPrerender(
      render_process_id_,
      render_frame_host->GetRenderViewHost()->GetRoutingID(),
      std::move(attributes), initiator_origin_, std::move(client));
}

void PrerenderProcessorImpl::Cancel() {
  if (!prerender_id_)
    return;
  auto* link_manager = GetPrerenderLinkManager();
  if (link_manager)
    link_manager->OnCancelPrerender(*prerender_id_);
}

void PrerenderProcessorImpl::Abandon() {
  if (!prerender_id_)
    return;
  auto* link_manager = GetPrerenderLinkManager();
  if (link_manager)
    link_manager->OnAbandonPrerender(*prerender_id_);
}

PrerenderLinkManager* PrerenderProcessorImpl::GetPrerenderLinkManager() {
  auto* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id_, render_frame_id_);
  if (!render_frame_host)
    return nullptr;
  return delegate_->GetPrerenderLinkManager(
      render_frame_host->GetProcess()->GetBrowserContext());
}

}  // namespace prerender
