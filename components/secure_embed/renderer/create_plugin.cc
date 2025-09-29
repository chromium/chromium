// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/secure_embed/renderer/create_plugin.h"

#include "components/secure_embed/common/constants.h"
#include "third_party/blink/public/web/web_plugin_params.h"

namespace secure_embed {

bool MayCreatePlugin(
    content::RenderFrame* render_frame,
    const blink::WebPluginParams& params,
    blink::WebPlugin** plugin) {
  if (params.mime_type == secure_embed::kInternalPluginMimeType) {
    return true;
  }
  return false;
}

}  // namespace secure_embed