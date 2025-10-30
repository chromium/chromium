// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/secure_embed/browser/secure_embed_host.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/supports_user_data.h"
#include "components/guest_contents/browser/guest_contents_handle.h"
#include "content/public/browser/guest_frame.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/secure_embed_delegate.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "third_party/blink/public/mojom/frame/intrinsic_sizing_info.mojom.h"
#include "third_party/blink/public/mojom/frame/viewport_intersection_state.mojom.h"

namespace secure_embed {

struct SecureEmbedHostUserData : public base::SupportsUserData::Data {
  constexpr static char kKey[] = "secure-embed-host-user-data";
  explicit SecureEmbedHostUserData(SecureEmbedHost* secure_embed_host)
      : secure_embed_host(secure_embed_host) {}

  raw_ptr<SecureEmbedHost> secure_embed_host;
};

// static
size_t SecureEmbedHost::instance_count_for_testing_ = 0;

SecureEmbedHost::SecureEmbedHost(content::RenderFrameHost*) : secure_embed_() {
  ++instance_count_for_testing_;
}

SecureEmbedHost::~SecureEmbedHost() {
  if (guest_contents_) {
    guest_contents_->RemoveUserData(SecureEmbedHostUserData::kKey);
  }
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

// static
SecureEmbedHost* SecureEmbedHost::GetFrom(content::WebContents* web_contents) {
  auto* user_data = static_cast<SecureEmbedHostUserData*>(
      web_contents->GetUserData(SecureEmbedHostUserData::kKey));
  return user_data ? user_data->secure_embed_host : nullptr;
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

  if (guest_contents_) {
    guest_contents_->RemoveUserData(SecureEmbedHostUserData::kKey);
  }
  know_have_focus_ = false;
  guest_contents_ = web_contents_to_attach->GetWeakPtr();
  guest_contents_->SetUserData(SecureEmbedHostUserData::kKey,
                               std::make_unique<SecureEmbedHostUserData>(this));
  guest_frame_ = content::GuestFrame::Create(web_contents_to_attach, this);
  secure_embed_->SetFrameSinkId(guest_frame_->GetFrameSinkId());
}

void SecureEmbedHost::SynchronizeVisualProperties(
    const blink::FrameVisualProperties& visual_properties) {
  if (guest_frame_) {
    guest_frame_->OnSynchronizeVisualProperties(visual_properties);
  }
}

void SecureEmbedHost::DispatchKeyboardEvent(
    std::unique_ptr<blink::WebCoalescedInputEvent> key_event) {
  if (!key_event || key_event->CoalescedEventSize() != 1 ||
      !blink::WebInputEvent::IsKeyboardEventType(
          key_event->Event().GetType())) {
    mojo::ReportBadMessage(
        "Unexpected message type in SecureEmbedHost::DispatchKeyboardEvent");
    return;
  }
  if (guest_frame_) {
    guest_frame_->ForwardKeyboardEvent(
        static_cast<const blink::WebKeyboardEvent&>(key_event->Event()));
  }
}

void SecureEmbedHost::SetFocus(bool focused,
                               blink::mojom::FocusType focus_type) {
  know_have_focus_ = false;
  if (guest_frame_) {
    guest_frame_->SetFocus(focused, focus_type);
    if (focused) {
      know_have_focus_ = true;
    }
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

void SecureEmbedHost::SetFrameSinkId(const viz::FrameSinkId& frame_sink_id) {
  if (secure_embed_) {
    secure_embed_->SetFrameSinkId(frame_sink_id);
  }
}

void SecureEmbedHost::RequestFocus(
    content::SecureEmbedDelegate::FocusOperation focus_op) {
  if (!secure_embed_) {
    return;
  }

  if (focus_op == content::SecureEmbedDelegate::FocusOperation::kFocusPlugin &&
      know_have_focus_) {
    return;
  }

  mojom::FocusOperation mojo_focus_op;
  switch (focus_op) {
    case content::SecureEmbedDelegate::FocusOperation::kFocusPlugin:
      mojo_focus_op = mojom::FocusOperation::kFocusPlugin;
      break;
    case content::SecureEmbedDelegate::FocusOperation::kFocusBeforePlugin:
      mojo_focus_op = mojom::FocusOperation::kFocusBeforePlugin;
      break;
    case content::SecureEmbedDelegate::FocusOperation::kFocusAfterPlugin:
      mojo_focus_op = mojom::FocusOperation::kFocusAfterPlugin;
      break;
  }

  secure_embed_->RequestFocus(mojo_focus_op);
}

}  // namespace secure_embed
