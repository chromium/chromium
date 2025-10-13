// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURE_EMBED_RENDERER_CREATE_PLUGIN_H_
#define COMPONENTS_SECURE_EMBED_RENDERER_CREATE_PLUGIN_H_

#include "base/component_export.h"

namespace blink {
class WebPlugin;
struct WebPluginParams;
}  // namespace blink

namespace content {
class RenderFrame;
}  // namespace content

namespace secure_embed {

// Returns true if a SecureEmbedWebPlugin is created.
COMPONENT_EXPORT(SECURE_EMBED)
bool MaybeCreatePlugin(content::RenderFrame* render_frame,
                       const blink::WebPluginParams& params,
                       blink::WebPlugin** plugin);

}  // namespace secure_embed

#endif  // COMPONENTS_SECURE_EMBED_RENDERER_CREATE_PLUGIN_H_
