// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/secure_embed/browser/secure_embed_host.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "components/guest_contents/browser/guest_contents_handle.h"
#include "content/public/browser/guest_frame.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/secure_embed_delegate.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "third_party/blink/public/mojom/frame/intrinsic_sizing_info.mojom.h"
#include "third_party/blink/public/mojom/frame/viewport_intersection_state.mojom.h"

namespace secure_embed {

// static
size_t SecureEmbedHost::instance_count_for_testing_ = 0;

SecureEmbedHost::SecureEmbedHost(content::RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host), secure_embed_() {
  ++instance_count_for_testing_;
}

SecureEmbedHost::~SecureEmbedHost() {
  --instance_count_for_testing_;
}

// static
void SecureEmbedHost::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingAssociatedReceiver<mojom::SecureEmbedHost> receiver) {
  mojo::MakeSelfOwnedAssociatedReceiver(
      base::WrapUnique(new SecureEmbedHost(render_frame_host)),
      std::move(receiver));
}

void SecureEmbedHost::SetSecureEmbed(
    mojo::PendingAssociatedRemote<mojom::SecureEmbed> secure_embed) {
  secure_embed_.Bind(std::move(secure_embed));
  secure_embed_.set_disconnect_handler(base::BindOnce(
      &SecureEmbedHost::OnSecureEmbedDisconnected, base::Unretained(this)));
}

void SecureEmbedHost::Attach(int64_t content_id) {
  // Should never call Attach without having a valid SecureEmbed remote already
  // bound.
  CHECK(secure_embed_);

  int guest_id = static_cast<int>(content_id);
  guest_contents::GuestContentsHandle* guest_handle =
      guest_contents::GuestContentsHandle::FromID(guest_id);

  // TODO(secure-embed): Temporary - remove when there's a real SecureEmbed
  // method to call. This is here to verify that the mojo connection is set up
  // correctly and that the browser can call back into the renderer.
  secure_embed_->OnAttached();

  // TODO(secure-embed): These LOG's should probably be ReportBadMessage.
  if (!guest_handle) {
    LOG(ERROR) << "GuestContentsHandle not found for content_id: "
               << content_id;
    return;
  }

  content::WebContents* web_contents_to_attach = guest_handle->web_contents();
  if (!web_contents_to_attach) {
    LOG(ERROR) << "WebContents not found for GuestContentsHandle";
    return;
  }

  if (!web_contents_to_attach->GetSecureEmbedDelegate()) {
    LOG(ERROR) << "WebContents doesn't have a SecureEmbedDelegate";
    return;
  }

  // TODO(secure-embed): Use web_contents_to_attach to complete the attachment.
  LOG(INFO) << "Successfully retrieved WebContents for content_id: "
            << content_id;

  guest_frame_ =
      content::GuestFrame::Create(web_contents_to_attach);
  secure_embed_->SetFrameSinkId(guest_frame_->GetFrameSinkId());
}

void SecureEmbedHost::SetLocalSurfaceId(
    const ::viz::LocalSurfaceId& local_surface_id) {
  if (guest_frame_) {
    guest_frame_->SetLocalSurfaceId(local_surface_id);
  }
}

// static
size_t SecureEmbedHost::GetInstanceCountForTesting() {
  return instance_count_for_testing_;
}

void SecureEmbedHost::OnSecureEmbedDisconnected() {
  // This will get hit when the renderer's SecureEmbedWebPlugin is destroyed. In
  // that scenario, `this` will get destroyed next as its lifetime is managed by
  // a SelfOwnedAssociatedReceiver.
  secure_embed_.reset();
}

}  // namespace secure_embed
