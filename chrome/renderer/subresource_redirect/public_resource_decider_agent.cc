// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/subresource_redirect/public_resource_decider_agent.h"

#include "content/public/renderer/render_frame.h"

namespace subresource_redirect {

PublicResourceDeciderAgent::PublicResourceDeciderAgent(
    blink::AssociatedInterfaceRegistry* associated_interfaces,
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<PublicResourceDeciderAgent>(
          render_frame) {
  DCHECK(render_frame);
  // base::Unretained is safe here because |this| is created for the RenderFrame
  // and never destroyed.
  associated_interfaces->AddInterface(base::BindRepeating(
      &PublicResourceDeciderAgent::BindHintsReceiver, base::Unretained(this)));
}

PublicResourceDeciderAgent::~PublicResourceDeciderAgent() = default;

void PublicResourceDeciderAgent::OnDestruct() {
  delete this;
}

void PublicResourceDeciderAgent::NotifyCompressedResourceFetchFailed(
    base::TimeDelta retry_after) {
  if (!subresource_redirect_service_remote_) {
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
        &subresource_redirect_service_remote_);
  }
  subresource_redirect_service_remote_->NotifyCompressedImageFetchFailed(
      retry_after);
}

void PublicResourceDeciderAgent::BindHintsReceiver(
    mojo::PendingAssociatedReceiver<mojom::SubresourceRedirectHintsReceiver>
        receiver) {
  subresource_redirect_hints_receiver_.Bind(std::move(receiver));
}

}  // namespace subresource_redirect
