// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/secure_embed/renderer/create_plugin.h"

#include "components/secure_embed/common/constants.h"
#include "components/secure_embed/common/secure_embed.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/web/web_plugin_params.h"

namespace secure_embed {

bool MaybeCreatePlugin(
    content::RenderFrame* render_frame,
    const blink::WebPluginParams& params,
    blink::WebPlugin** plugin) {
  if (params.mime_type == secure_embed::kInternalPluginMimeType) {
    mojo::AssociatedRemote<secure_embed::mojom::SecureEmbedHost> secure_embed_host;
    render_frame->GetRemoteAssociatedInterfaces()->GetInterface(&secure_embed_host);
    return true;
  }
  return false;
}

}  // namespace secure_embed