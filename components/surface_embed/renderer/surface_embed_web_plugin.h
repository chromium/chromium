// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SURFACE_EMBED_RENDERER_SURFACE_EMBED_WEB_PLUGIN_H_
#define COMPONENTS_SURFACE_EMBED_RENDERER_SURFACE_EMBED_WEB_PLUGIN_H_

#include <memory>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "cc/layers/surface_layer.h"
#include "components/surface_embed/common/surface_embed.mojom.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
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
class SurfaceEmbedWebPlugin : public blink::WebPlugin,
                              public mojom::SurfaceEmbed {
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

  void InitializeSurfaceLayer();

  // Synchronizes visual properties (e.g. LocalSurfaceId, viewport size) with
  // the browser process.
  void SynchronizeVisualProperties();

  // Called when the mojo channels disconnect.
  void OnHostDisconnected();

  // mojom::SurfaceEmbed implementation:
  void SetFrameSinkId(const ::viz::FrameSinkId& frame_sink_id) override;

  raw_ptr<blink::WebPluginContainer> container_ = nullptr;
  scoped_refptr<cc::SurfaceLayer> layer_;

  std::optional<blink::FrameVisualProperties> sent_visual_properties_;
  std::optional<bool> sent_last_is_visible_;

  gfx::Rect last_window_rect_;
  gfx::Rect last_clip_rect_;
  gfx::Rect last_unobscured_rect_;
  bool last_is_visible_ = false;
  bool frame_sink_id_changed_ = false;

  viz::FrameSinkId frame_sink_id_;
  std::unique_ptr<viz::ParentLocalSurfaceIdAllocator>
      parent_local_surface_id_allocator_;

  mojo::Remote<mojom::SurfaceEmbedHost> host_;
  mojo::Receiver<mojom::SurfaceEmbed> receiver_{this};
};

}  // namespace surface_embed

#endif  // COMPONENTS_SURFACE_EMBED_RENDERER_SURFACE_EMBED_WEB_PLUGIN_H_
