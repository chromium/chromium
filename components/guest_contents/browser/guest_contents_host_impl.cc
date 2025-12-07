// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_contents/browser/guest_contents_host_impl.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "components/guest_contents/browser/guest_contents_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace guest_contents {

// static
void GuestContentsHostImpl::Create(
    content::WebContents* outer_web_contents,
    mojo::PendingReceiver<guest_contents::mojom::GuestContentsHost> receiver) {
  mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(new GuestContentsHostImpl(outer_web_contents)),
      std::move(receiver));
}

GuestContentsHostImpl::GuestContentsHostImpl(
    content::WebContents* outer_web_contents)
    : content::WebContentsObserver(outer_web_contents),
      outer_web_contents_(outer_web_contents) {}

void GuestContentsHostImpl::WebContentsDestroyed() {
  // This class is owned by the GuestContentsHost mojo receiver, which may or
  // may not outlive the outer WebContents. Reset the raw_ptr on WebContents
  // destruction to avoid dangling pointer.
  outer_web_contents_ = nullptr;
}

void GuestContentsHostImpl::Attach(
    const blink::LocalFrameToken& token_of_frame_to_swap,
    GuestId guest_contents_id,
    AttachCallback callback) {
  GuestContentsHandle* guest_handle =
      GuestContentsHandle::FromID(guest_contents_id);
  if (!guest_handle) {
    mojo::ReportBadMessage("GuestContentsHandle not found");
    return;
  }

  CHECK(outer_web_contents_);
  // This code assumes that the outer delegate frame to be swapped is in the
  // same process as the main frame of the outer web contents.
  content::RenderFrameHost* frame_to_swap =
      content::RenderFrameHost::FromFrameToken(
          content::GlobalRenderFrameHostToken(
              outer_web_contents_->GetPrimaryMainFrame()
                  ->GetProcess()
                  ->GetDeprecatedID(),
              token_of_frame_to_swap));

  if (!frame_to_swap) {
    mojo::ReportBadMessage("Outer delegate frame not found");
    return;
  }

  guest_handle->AttachToOuterWebContents(frame_to_swap);

  std::move(callback).Run(/*success=*/true);
}

}  // namespace guest_contents
