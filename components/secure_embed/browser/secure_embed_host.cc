// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/secure_embed/browser/secure_embed_host.h"

#include "base/logging.h"

namespace secure_embed {

void SecureEmbedHost::BindSecureEmbedHost(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingAssociatedReceiver<secure_embed::mojom::SecureEmbedHost>
        receiver) {
  LOG(ERROR) << "BindSecureEmbedHost() called";
}

}  // namespace secure_embed
