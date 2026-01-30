// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SURFACE_EMBED_RENDERER_CREATE_PLUGIN_H_
#define COMPONENTS_SURFACE_EMBED_RENDERER_CREATE_PLUGIN_H_

#include "base/component_export.h"

namespace blink {
class WebPlugin;
struct WebPluginParams;
}  // namespace blink

namespace content {
class RenderFrame;
}  // namespace content

namespace surface_embed {

// Returns true if a SurfaceEmbedWebPlugin is created.
COMPONENT_EXPORT(SURFACE_EMBED_RENDERER)
bool MaybeCreatePlugin(content::RenderFrame* render_frame,
                       const blink::WebPluginParams& params,
                       blink::WebPlugin** plugin);

}  // namespace surface_embed

#endif  // COMPONENTS_SURFACE_EMBED_RENDERER_CREATE_PLUGIN_H_
