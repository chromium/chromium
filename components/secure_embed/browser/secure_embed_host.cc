// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/secure_embed/browser/secure_embed_host.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/guest_contents/browser/guest_contents_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"

namespace secure_embed {

// static
size_t SecureEmbedHost::instance_count_for_testing_ = 0;

SecureEmbedHost::SecureEmbedHost(content::RenderFrameHost* render_frame_host) {
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

void SecureEmbedHost::Attach(int64_t content_id) {
  int guest_id = static_cast<int>(content_id);
  guest_contents::GuestContentsHandle* guest_handle =
      guest_contents::GuestContentsHandle::FromID(guest_id);

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

  // TODO(secure-embed): Use web_contents_to_attach to complete the attachment.
  LOG(INFO) << "Successfully retrieved WebContents for content_id: "
            << content_id;
}

// static
size_t SecureEmbedHost::GetInstanceCountForTesting() {
  return instance_count_for_testing_;
}

}  // namespace secure_embed
