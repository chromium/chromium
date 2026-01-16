// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SURFACE_EMBED_RENDERER_SURFACE_EMBED_WEB_PLUGIN_H_
#define COMPONENTS_SURFACE_EMBED_RENDERER_SURFACE_EMBED_WEB_PLUGIN_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "cc/layers/content_layer_client.h"
#include "components/surface_embed/common/surface_embed.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/web/web_plugin.h"

namespace cc {
class SurfaceLayer;
class PictureLayer;
}  // namespace cc

namespace blink {
struct WebPluginParams;
class WebPluginContainer;
}  // namespace blink

namespace content {
class RenderFrame;
}  // namespace content

namespace secure_embed {

class SecureEmbedWebPlugin : public blink::WebPlugin,
                             public mojom::SecureEmbed,
                             public cc::ContentLayerClient {
 public:
  static SecureEmbedWebPlugin* Create(content::RenderFrame* render_frame,
                                      const blink::WebPluginParams& params);

  SecureEmbedWebPlugin(const SecureEmbedWebPlugin&) = delete;
  SecureEmbedWebPlugin& operator=(const SecureEmbedWebPlugin&) = delete;
  ~SecureEmbedWebPlugin() override;

  // blink::WebPlugin:
  bool Initialize(blink::WebPluginContainer* container) override;
  void Destroy() override;
  blink::WebPluginContainer* Container() const override;
  v8::Local<v8::Object> V8ScriptableObject(v8::Isolate* isolate) override;
  void UpdateAllLifecyclePhases(blink::DocumentUpdateReason reason) override;
  void Paint(cc::PaintCanvas* canvas, const gfx::Rect& rect) override;
  viz::FrameSinkId GetFrameSinkId() override;
  void UpdateGeometry(const gfx::Rect& window_rect,
                      const gfx::Rect& clip_rect,
                      const gfx::Rect& unobscured_rect,
                      bool is_visible) override;
  void UpdateFocus(bool focused, blink::mojom::FocusType focus_type) override;
  void UpdateVisibility(bool is_visible) override;
  void UpdateDataAttribute(const blink::WebString& attribute_name,
                           const blink::WebString& attribute_value) override;
  blink::WebInputEventResult HandleInputEvent(
      const blink::WebCoalescedInputEvent& event,
      ui::Cursor* cursor) override;
  void DidReceiveResponse(const blink::WebURLResponse& response) override;
  void DidReceiveData(base::span<const char> data) override;
  void DidFinishLoading() override;
  void DidFailLoading(const blink::WebURLError& error) override;
  bool SupportsKeyboardFocus() const override;

  // mojom::SecureEmbed:
  void SetFrameSinkId(const ::viz::FrameSinkId& frame_sink_id) override;
  void UpdateLocalSurfaceIdFromChild(
      const ::viz::LocalSurfaceId& local_surface_id) override;
  void ChildProcessGone() override;
  void DetachPlugin() override;
  void RequestFocus(mojom::FocusOperation focus_op) override;

  // cc::ContentLayerClient, used only if we're painting a sad frame.
  scoped_refptr<cc::DisplayItemList> PaintContentsToDisplayList() override;
  bool FillsBoundsCompletely() const override;

 private:
  explicit SecureEmbedWebPlugin(
      mojo::AssociatedRemote<mojom::SecureEmbedHost> host,
      int contents_id);

  void OnSecureEmbedHostDisconnected();

  void SynchronizeVisualProperties();

  void DetachInternal();
  void InitializeSurfaceLayer();

  // The guest contents ID parsed from the `data-content-id` attribute.
  int contents_id_ = -1;

  raw_ptr<blink::WebPluginContainer> container_ = nullptr;
  scoped_refptr<cc::SurfaceLayer> layer_;

  // Only set if needed.
  scoped_refptr<cc::PictureLayer> crashed_layer_;

  std::optional<blink::FrameVisualProperties> sent_visual_properties_;
  std::optional<bool> sent_last_is_visible_;

  gfx::Rect last_window_rect_;
  gfx::Rect last_clip_rect_;
  gfx::Rect last_unobscured_rect_;
  bool last_is_visible_ = false;

  viz::FrameSinkId frame_sink_id_;
  bool frame_sink_id_changed_ = false;

  mojo::AssociatedRemote<mojom::SecureEmbedHost> host_;
  mojo::AssociatedReceiver<mojom::SecureEmbed> receiver_{this};
  std::unique_ptr<viz::ParentLocalSurfaceIdAllocator>
      parent_local_surface_id_allocator_;
};

}  // namespace secure_embed

#endif  // COMPONENTS_SURFACE_EMBED_RENDERER_SURFACE_EMBED_WEB_PLUGIN_H_
