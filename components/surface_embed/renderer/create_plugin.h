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
// This function is called in ContentRendererClient::OverrideCreatePlugin
// overrides (e.g., ChromeContentRendererClient::OverrideCreatePlugin)
// and is gated to certain WebUI URLs (e.g., chrome://webui-browser).
// It attempts to get the SurfaceEmbedHost interface from the browser process.
// If it is not available, the mojo pipe will be closed, which subsequently
// reaches a NOTREACHED() in SurfaceEmbedWebPlugin::OnHostDisconnected().
// The URL check is performed both on the renderer side and on the browser side
// (e.g., in PopulateChromeWebUIFrameBindersPartsDesktop()). This ensures that a
// compromised renderer faking the URL check will still be caught by the
// browser-side check.
COMPONENT_EXPORT(SURFACE_EMBED_RENDERER)
bool MaybeCreatePlugin(content::RenderFrame* render_frame,
                       const blink::WebPluginParams& params,
                       blink::WebPlugin** plugin);

}  // namespace surface_embed

#endif  // COMPONENTS_SURFACE_EMBED_RENDERER_CREATE_PLUGIN_H_
