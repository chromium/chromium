// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/surface_embed/browser/surface_embed_host.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/supports_user_data.h"
#include "components/guest_contents/browser/guest_contents_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/surface_embed_connector.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "third_party/blink/public/mojom/frame/intrinsic_sizing_info.mojom.h"
#include "third_party/blink/public/mojom/frame/viewport_intersection_state.mojom.h"

namespace surface_embed {

// static
size_t SurfaceEmbedHost::instance_count_for_testing_ = 0;
size_t SurfaceEmbedHost::attached_instance_count_for_testing_ = 0;

SurfaceEmbedHost::SurfaceEmbedHost(content::RenderFrameHost* render_frame_host)
    : render_frame_host_id_(render_frame_host->GetGlobalId()),
      surface_embed_() {
  ++instance_count_for_testing_;
}

SurfaceEmbedHost::~SurfaceEmbedHost() {
  --instance_count_for_testing_;
  DetachConnector();
}

// static
void SurfaceEmbedHost::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingAssociatedReceiver<mojom::SurfaceEmbedHost> receiver) {
  mojo::MakeSelfOwnedAssociatedReceiver(
      base::WrapUnique(new SurfaceEmbedHost(render_frame_host)),
      std::move(receiver));
}

void SurfaceEmbedHost::SetSurfaceEmbed(
    mojo::PendingAssociatedRemote<mojom::SurfaceEmbed> surface_embed) {
  surface_embed_.Bind(std::move(surface_embed));
  surface_embed_.set_disconnect_handler(base::BindOnce(
      &SurfaceEmbedHost::OnSurfaceEmbedDisconnected, base::Unretained(this)));
}

void SurfaceEmbedHost::AttachConnector(int64_t content_id) {
  // Should never call attach without having a valid SurfaceEmbed remote already
  // bound.
  CHECK(surface_embed_);

  int guest_id = static_cast<int>(content_id);
  if (guest_id <= 0) {
    mojo::ReportBadMessage(
        "Invalid content_id in SurfaceEmbedHost::AttachConnector");
    return;
  }

  guest_contents::GuestContentsHandle* guest_handle =
      guest_contents::GuestContentsHandle::FromID(guest_id);

  if (!guest_handle) {
    mojo::ReportBadMessage(
        "GuestContentsHandle not found for content_id in "
        "SurfaceEmbedHost::AttachConnector");
    return;
  }

  content::WebContents* web_contents_to_attach = guest_handle->web_contents();
  if (!web_contents_to_attach) {
    mojo::ReportBadMessage(
        "WebContents not found for GuestContentsHandle in "
        "SurfaceEmbedHost::AttachConnector");
    return;
  }

  // If the guest WebContents is already attached to a SurfaceEmbedConnector, we
  // need to detach it first. Since we're detaching some other host we need to
  // notify it of the detachment so the host and SurfaceEmbedWebPlugin stay in
  // sync.
  if (auto* connector = web_contents_to_attach->GetSurfaceEmbedConnector()) {
    connector->GetDelegate()->DetachedByHost();
    CHECK(web_contents_to_attach->GetSurfaceEmbedConnector() == nullptr);
  }

  // If this host already has a guest attached, we need to detach it first. Note
  // that this request comes from the embedder side, so we don't notify the
  // SurfaceEmbed as it initiated the detachment.
  DetachConnector();

  guest_contents_ = web_contents_to_attach->GetWeakPtr();
  content::WebContents* parent_web_contents =
      content::WebContents::FromRenderFrameHost(
          content::RenderFrameHost::FromID(render_frame_host_id_));
  content::SurfaceEmbedConnector::Attach(parent_web_contents,
                                         web_contents_to_attach, this);

  ++attached_instance_count_for_testing_;

  if (web_contents_to_attach->IsCrashed()) {
    // The child process may have crashed before the renderer for embedder
    // got chance to attach it.
    surface_embed_->ChildProcessGone();
  } else {
    auto* connector = GetConnector();
    CHECK(connector->GetFrameSinkId().is_valid());
    surface_embed_->SetFrameSinkId(connector->GetFrameSinkId());
  }
}

void SurfaceEmbedHost::DetachConnector() {
  if (GetConnector()) {
    content::SurfaceEmbedConnector::Detach(guest_contents_.get());
    guest_contents_ = nullptr;
    attached_instance_count_for_testing_--;
  }
  know_have_focus_ = false;
  CHECK(!guest_contents_);
}

void SurfaceEmbedHost::SynchronizeVisualProperties(
    const blink::FrameVisualProperties& visual_properties,
    bool is_visible) {
  if (content::SurfaceEmbedConnector* connector = GetConnector()) {
    // TODO(surface-embed): We need to figure out when we're out of viewport.
    connector->OnVisibilityChanged(
        is_visible ? blink::mojom::FrameVisibility::kRenderedInViewport
                   : blink::mojom::FrameVisibility::kNotRendered);
    connector->OnSynchronizeVisualProperties(visual_properties);
  }
}

void SurfaceEmbedHost::SetFocus(bool focused,
                                blink::mojom::FocusType focus_type) {
  know_have_focus_ = false;
  if (content::SurfaceEmbedConnector* connector = GetConnector()) {
    connector->SetFocus(focused, focus_type);
    if (focused) {
      know_have_focus_ = true;
    }
  }
}

// static
size_t SurfaceEmbedHost::GetInstanceCountForTesting() {
  return instance_count_for_testing_;
}

// static
size_t SurfaceEmbedHost::GetAttachedInstanceCountForTesting() {
  return attached_instance_count_for_testing_;
}

void SurfaceEmbedHost::OnSurfaceEmbedDisconnected() {
  // This will get hit when the renderer's SurfaceEmbedWebPlugin is destroyed.
  // In that scenario, `this` will get destroyed next as its lifetime is managed
  // by a SelfOwnedAssociatedReceiver.
  surface_embed_.reset();
}

void SurfaceEmbedHost::SetFrameSinkId(const viz::FrameSinkId& frame_sink_id) {
  if (surface_embed_) {
    surface_embed_->SetFrameSinkId(frame_sink_id);
  }
}

void SurfaceEmbedHost::UpdateLocalSurfaceIdFromChild(
    const ::viz::LocalSurfaceId& local_surface_id) {
  if (surface_embed_) {
    surface_embed_->UpdateLocalSurfaceIdFromChild(local_surface_id);
  }
}

void SurfaceEmbedHost::ChildProcessGone() {
  if (surface_embed_) {
    surface_embed_->ChildProcessGone();
  }
}

void SurfaceEmbedHost::DetachedByHost() {
  // We're being forcibly detached (guest being re-attached elsewhere).
  CHECK(guest_contents_);

  if (surface_embed_) {
    // Notify the renderer's SurfaceEmbedWebPlugin that the host initiated
    // the detachment.
    surface_embed_->DetachPlugin();
  }
  DetachConnector();
}

void SurfaceEmbedHost::FocusInEmbedder(
    content::SurfaceEmbedConnector::FocusOperation focus_op) {
  if (!surface_embed_) {
    return;
  }

  if (focus_op ==
          content::SurfaceEmbedConnector::FocusOperation::kFocusSurface &&
      know_have_focus_) {
    return;
  }

  mojom::FocusOperation mojo_focus_op;
  switch (focus_op) {
    case content::SurfaceEmbedConnector::FocusOperation::kFocusSurface:
      mojo_focus_op = mojom::FocusOperation::kFocusSurface;
      break;
    case content::SurfaceEmbedConnector::FocusOperation::kFocusBeforeSurface:
      mojo_focus_op = mojom::FocusOperation::kFocusBeforeSurface;
      break;
    case content::SurfaceEmbedConnector::FocusOperation::kFocusAfterSurface:
      mojo_focus_op = mojom::FocusOperation::kFocusAfterSurface;
      break;
  }

  surface_embed_->RequestFocus(mojo_focus_op);
}

content::SurfaceEmbedConnector* SurfaceEmbedHost::GetConnector() {
  return guest_contents_ ? guest_contents_->GetSurfaceEmbedConnector()
                         : nullptr;
}

bool SurfaceEmbedHost::IsAttachedForTesting() const {
  return guest_contents_ != nullptr;
}

}  // namespace surface_embed
