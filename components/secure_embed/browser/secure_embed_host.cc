// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/secure_embed/browser/secure_embed_host.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/supports_user_data.h"
#include "components/guest_contents/browser/guest_contents_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/secure_embed_connector.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "secure_embed_host.h"
#include "third_party/blink/public/mojom/frame/intrinsic_sizing_info.mojom.h"
#include "third_party/blink/public/mojom/frame/viewport_intersection_state.mojom.h"

namespace secure_embed {

// static
size_t SecureEmbedHost::instance_count_for_testing_ = 0;
size_t SecureEmbedHost::attached_instance_count_for_testing_ = 0;

SecureEmbedHost::SecureEmbedHost(content::RenderFrameHost* render_frame_host)
    : render_frame_host_id_(render_frame_host->GetGlobalId()), secure_embed_() {
  ++instance_count_for_testing_;
}

SecureEmbedHost::~SecureEmbedHost() {
  --instance_count_for_testing_;
  DetachConnector();
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

void SecureEmbedHost::AttachConnector(int64_t content_id) {
  // Should never call attach without having a valid SecureEmbed remote already
  // bound.
  CHECK(secure_embed_);

  int guest_id = static_cast<int>(content_id);
  if (guest_id <= 0) {
    mojo::ReportBadMessage(
        "Invalid content_id in SecureEmbedHost::AttachConnector");
    return;
  }

  guest_contents::GuestContentsHandle* guest_handle =
      guest_contents::GuestContentsHandle::FromID(guest_id);

  if (!guest_handle) {
    mojo::ReportBadMessage(
        "GuestContentsHandle not found for content_id in "
        "SecureEmbedHost::AttachConnector");
    return;
  }

  content::WebContents* web_contents_to_attach = guest_handle->web_contents();
  if (!web_contents_to_attach) {
    mojo::ReportBadMessage(
        "WebContents not found for GuestContentsHandle in "
        "SecureEmbedHost::AttachConnector");
    return;
  }

  // If the guest WebContents is already attached to a SecureEmbedConnector, we
  // need to detach it first. Since we're detaching some other host we need to
  // notify it of the detachment so the host and SecureEmbedWebPlugin stay in
  // sync.
  if (auto* connector = web_contents_to_attach->GetSecureEmbedConnector()) {
    connector->GetDelegate()->DetachedByHost();
    CHECK(web_contents_to_attach->GetSecureEmbedConnector() == nullptr);
  }

  // If this host already has a guest attached, we need to detach it first. Note
  // that this request comes from the embedder side, so we don't notify the
  // SecureEmbed as it initiated the detachment.
  DetachConnector();

  guest_contents_ = web_contents_to_attach->GetWeakPtr();
  content::WebContents* parent_web_contents =
      content::WebContents::FromRenderFrameHost(
          content::RenderFrameHost::FromID(render_frame_host_id_));
  content::SecureEmbedConnector::Attach(parent_web_contents,
                                        web_contents_to_attach, this);

  ++attached_instance_count_for_testing_;

  if (web_contents_to_attach->IsCrashed()) {
    // The child process may have crashed before the renderer for embedder
    // got chance to attach it.
    secure_embed_->ChildProcessGone();
  } else {
    auto* connector = GetConnector();
    CHECK(connector->GetFrameSinkId().is_valid());
    secure_embed_->SetFrameSinkId(connector->GetFrameSinkId());
  }
}

void SecureEmbedHost::DetachConnector() {
  if (GetConnector()) {
    content::SecureEmbedConnector::Detach(guest_contents_.get());
    guest_contents_ = nullptr;
    attached_instance_count_for_testing_--;
  }
  know_have_focus_ = false;
  CHECK(!guest_contents_);
}

void SecureEmbedHost::SynchronizeVisualProperties(
    const blink::FrameVisualProperties& visual_properties,
    bool is_visible) {
  if (content::SecureEmbedConnector* connector = GetConnector()) {
    // TODO(secure-embed): We need to figure out when we're out of viewport.
    connector->OnVisibilityChanged(
        is_visible ? blink::mojom::FrameVisibility::kRenderedInViewport
                   : blink::mojom::FrameVisibility::kNotRendered);
    connector->OnSynchronizeVisualProperties(visual_properties);
  }
}

void SecureEmbedHost::SetFocus(bool focused,
                               blink::mojom::FocusType focus_type) {
  know_have_focus_ = false;
  if (content::SecureEmbedConnector* connector = GetConnector()) {
    connector->SetFocus(focused, focus_type);
    if (focused) {
      know_have_focus_ = true;
    }
  }
}

// static
size_t SecureEmbedHost::GetInstanceCountForTesting() {
  return instance_count_for_testing_;
}

// static
size_t SecureEmbedHost::GetAttachedInstanceCountForTesting() {
  return attached_instance_count_for_testing_;
}

void SecureEmbedHost::OnSecureEmbedDisconnected() {
  // This will get hit when the renderer's SecureEmbedWebPlugin is destroyed. In
  // that scenario, `this` will get destroyed next as its lifetime is managed by
  // a SelfOwnedAssociatedReceiver.
  secure_embed_.reset();
}

void SecureEmbedHost::SetFrameSinkId(const viz::FrameSinkId& frame_sink_id) {
  if (secure_embed_) {
    secure_embed_->SetFrameSinkId(frame_sink_id);
  }
}

void SecureEmbedHost::UpdateLocalSurfaceIdFromChild(
    const ::viz::LocalSurfaceId& local_surface_id) {
  if (secure_embed_) {
    secure_embed_->UpdateLocalSurfaceIdFromChild(local_surface_id);
  }
}

void SecureEmbedHost::ChildProcessGone() {
  if (secure_embed_) {
    secure_embed_->ChildProcessGone();
  }
}

void SecureEmbedHost::DetachedByHost() {
  // We're being forcibly detached (guest being re-attached elsewhere).
  CHECK(guest_contents_);

  if (secure_embed_) {
    // Notify the renderer's SecureEmbedWebPlugin that the host initiated
    // the detachment.
    secure_embed_->DetachPlugin();
  }
  DetachConnector();
}

void SecureEmbedHost::FocusInEmbedder(
    content::SecureEmbedConnector::FocusOperation focus_op) {
  if (!secure_embed_) {
    return;
  }

  if (focus_op == content::SecureEmbedConnector::FocusOperation::kFocusPlugin &&
      know_have_focus_) {
    return;
  }

  mojom::FocusOperation mojo_focus_op;
  switch (focus_op) {
    case content::SecureEmbedConnector::FocusOperation::kFocusPlugin:
      mojo_focus_op = mojom::FocusOperation::kFocusPlugin;
      break;
    case content::SecureEmbedConnector::FocusOperation::kFocusBeforePlugin:
      mojo_focus_op = mojom::FocusOperation::kFocusBeforePlugin;
      break;
    case content::SecureEmbedConnector::FocusOperation::kFocusAfterPlugin:
      mojo_focus_op = mojom::FocusOperation::kFocusAfterPlugin;
      break;
  }

  secure_embed_->RequestFocus(mojo_focus_op);
}

content::SecureEmbedConnector* SecureEmbedHost::GetConnector() {
  return guest_contents_ ? guest_contents_->GetSecureEmbedConnector() : nullptr;
}

bool SecureEmbedHost::IsAttachedForTesting() const {
  return guest_contents_ != nullptr;
}

}  // namespace secure_embed
