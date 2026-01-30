// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SURFACE_EMBED_RENDERER_SURFACE_EMBED_WEB_PLUGIN_H_
#define COMPONENTS_SURFACE_EMBED_RENDERER_SURFACE_EMBED_WEB_PLUGIN_H_

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "cc/layers/solid_color_layer.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {
struct WebPluginParams;
}  // namespace blink

namespace content {
class RenderFrame;
}

namespace surface_embed {

// WebPlugin implementation that embeds a WebContents surface, identified via
// the `kInternalPluginMimeType` mime type and the data-content-id attribute on
// an <embed> element. The 1:1 browser process counterpart is the
// SurfaceEmbedHost.
class SurfaceEmbedWebPlugin : public blink::WebPlugin {
 public:
  static SurfaceEmbedWebPlugin* Create(content::RenderFrame* render_frame,
                                       const blink::WebPluginParams& params);

  ~SurfaceEmbedWebPlugin() override;

  SurfaceEmbedWebPlugin(const SurfaceEmbedWebPlugin&) = delete;
  SurfaceEmbedWebPlugin& operator=(const SurfaceEmbedWebPlugin&) = delete;

  // blink::WebPlugin implementation:
  bool Initialize(blink::WebPluginContainer* container) override;
  void Destroy() override;
  blink::WebPluginContainer* Container() const override;
  void UpdateAllLifecyclePhases(blink::DocumentUpdateReason reason) override;
  void Paint(cc::PaintCanvas* canvas, const gfx::Rect& rect) override;
  void UpdateGeometry(const gfx::Rect& window_rect,
                      const gfx::Rect& clip_rect,
                      const gfx::Rect& unobscured_rect,
                      bool is_visible) override;
  void UpdateFocus(bool focused, blink::mojom::FocusType focus_type) override;
  void UpdateVisibility(bool visible) override;
  blink::WebInputEventResult HandleInputEvent(
      const blink::WebCoalescedInputEvent& event,
      ui::Cursor* cursor) override;
  void DidReceiveResponse(const blink::WebURLResponse& response) override;
  void DidReceiveData(base::span<const char> data) override;
  void DidFinishLoading() override;
  void DidFailLoading(const blink::WebURLError& error) override;

 private:
  SurfaceEmbedWebPlugin(content::RenderFrame* render_frame,
                        const blink::WebPluginParams& params);

  raw_ptr<blink::WebPluginContainer> container_ = nullptr;
  scoped_refptr<cc::SolidColorLayer> layer_;
  gfx::Rect plugin_rect_;
};

}  // namespace surface_embed

#endif  // COMPONENTS_SURFACE_EMBED_RENDERER_SURFACE_EMBED_WEB_PLUGIN_H_
