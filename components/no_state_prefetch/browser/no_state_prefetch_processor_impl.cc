// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/no_state_prefetch/browser/no_state_prefetch_processor_impl.h"

#include "components/no_state_prefetch/browser/no_state_prefetch_link_manager.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/referrer.h"

namespace prerender {

NoStatePrefetchProcessorImpl::NoStatePrefetchProcessorImpl(
    int render_process_id,
    int render_frame_id,
    const url::Origin& initiator_origin,
    mojo::PendingReceiver<blink::mojom::NoStatePrefetchProcessor> receiver,
    std::unique_ptr<NoStatePrefetchProcessorImplDelegate> delegate)
    : render_process_id_(render_process_id),
      render_frame_id_(render_frame_id),
      initiator_origin_(initiator_origin),
      delegate_(std::move(delegate)) {
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(base::BindOnce(
      &NoStatePrefetchProcessorImpl::Abandon, base::Unretained(this)));
}

NoStatePrefetchProcessorImpl::~NoStatePrefetchProcessorImpl() = default;

// static
void NoStatePrefetchProcessorImpl::Create(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<blink::mojom::NoStatePrefetchProcessor> receiver,
    std::unique_ptr<NoStatePrefetchProcessorImplDelegate> delegate) {
  // NoStatePrefetchProcessorImpl is a self-owned object. This deletes itself on
  // the mojo disconnect handler.
  new NoStatePrefetchProcessorImpl(frame_host->GetProcess()->GetID(),
                                   frame_host->GetRoutingID(),
                                   frame_host->GetLastCommittedOrigin(),
                                   std::move(receiver), std::move(delegate));
}

void NoStatePrefetchProcessorImpl::Start(
    blink::mojom::PrerenderAttributesPtr attributes) {
  // TODO(crbug.com/40109437): Remove the exception for opaque origins below and
  // allow HostsOrigin() to always verify them, including checking their
  // precursor. This verification is currently enabled behind a kill switch.
  bool should_skip_checks_for_opaque_origin =
      initiator_origin_.opaque() &&
      !base::FeatureList::IsEnabled(
          features::kAdditionalOpaqueOriginEnforcements);
  if (!should_skip_checks_for_opaque_origin &&
      !content::ChildProcessSecurityPolicy::GetInstance()->HostsOrigin(
          render_process_id_, initiator_origin_)) {
    receiver_.ReportBadMessage("NSPPI_INVALID_INITIATOR_ORIGIN");
    // The above ReportBadMessage() closes |receiver_| but does not trigger its
    // disconnect handler, so we need to call the handler explicitly
    // here to do some necessary work.
    Abandon();
    return;
  }

  // Start() must be called only one time.
  if (link_trigger_id_) {
    receiver_.ReportBadMessage("NSPPI_START_TWICE");
    // The above ReportBadMessage() closes |receiver_| but does not trigger its
    // disconnect handler, so we need to call the handler explicitly
    // here to do some necessary work.
    Abandon();
    return;
  }

  auto* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id_, render_frame_id_);
  if (!render_frame_host)
    return;

  auto* link_manager = GetNoStatePrefetchLinkManager();
  if (!link_manager)
    return;

  DCHECK(!link_trigger_id_);
  link_trigger_id_ = link_manager->OnStartLinkTrigger(
      render_process_id_,
      render_frame_host->GetRenderViewHost()->GetRoutingID(), render_frame_id_,
      std::move(attributes), initiator_origin_);
}

void NoStatePrefetchProcessorImpl::Cancel() {
  if (!link_trigger_id_)
    return;
  auto* link_manager = GetNoStatePrefetchLinkManager();
  if (link_manager)
    link_manager->OnCancelLinkTrigger(*link_trigger_id_);
}

void NoStatePrefetchProcessorImpl::Abandon() {
  if (link_trigger_id_) {
    auto* link_manager = GetNoStatePrefetchLinkManager();
    if (link_manager)
      link_manager->OnAbandonLinkTrigger(*link_trigger_id_);
  }
  delete this;
}

NoStatePrefetchLinkManager*
NoStatePrefetchProcessorImpl::GetNoStatePrefetchLinkManager() {
  auto* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id_, render_frame_id_);
  if (!render_frame_host)
    return nullptr;
  return delegate_->GetNoStatePrefetchLinkManager(
      render_frame_host->GetProcess()->GetBrowserContext());
}

}  // namespace prerender
