// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURE_EMBED_BROWSER_SECURE_EMBED_HOST_H_
#define COMPONENTS_SECURE_EMBED_BROWSER_SECURE_EMBED_HOST_H_

#include "components/secure_embed/common/secure_embed.mojom.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace secure_embed {

class SecureEmbedHost {
 public:
  static void BindSecureEmbedHost(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingAssociatedReceiver<secure_embed::mojom::SecureEmbedHost>
          receiver);
};

}  // namespace secure_embed

#endif  // COMPONENTS_SECURE_EMBED_BROWSER_SECURE_EMBED_HOST_H_
