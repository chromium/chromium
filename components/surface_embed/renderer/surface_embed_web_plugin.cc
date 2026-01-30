// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/surface_embed/renderer/surface_embed_web_plugin.h"

#include "base/notimplemented.h"
#include "cc/layers/solid_color_layer.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"

namespace surface_embed {

// static
SurfaceEmbedWebPlugin* SurfaceEmbedWebPlugin::Create(
    content::RenderFrame* render_frame,
    const blink::WebPluginParams& params) {
  return new SurfaceEmbedWebPlugin(render_frame, params);
}

SurfaceEmbedWebPlugin::SurfaceEmbedWebPlugin(
    content::RenderFrame* render_frame,
    const blink::WebPluginParams& params) {}

SurfaceEmbedWebPlugin::~SurfaceEmbedWebPlugin() = default;

bool SurfaceEmbedWebPlugin::Initialize(blink::WebPluginContainer* container) {
  container_ = container;

  // As a placeholder, start with a red rectangle to represent the plugin area.
  layer_ = cc::SolidColorLayer::Create();
  layer_->SetBackgroundColor(SkColor4f::FromColor(SK_ColorRED));
  layer_->SetIsDrawable(true);

  container_->SetCcLayer(layer_.get());

  return true;
}

void SurfaceEmbedWebPlugin::Destroy() {
  if (container_) {
    container_->SetCcLayer(nullptr);
    container_ = nullptr;
  }
  layer_ = nullptr;

  delete this;
}

blink::WebPluginContainer* SurfaceEmbedWebPlugin::Container() const {
  return container_;
}

void SurfaceEmbedWebPlugin::UpdateAllLifecyclePhases(
    blink::DocumentUpdateReason reason) {}

void SurfaceEmbedWebPlugin::Paint(cc::PaintCanvas* canvas,
                                  const gfx::Rect& rect) {
  // No action needed as we're using a compositor layer to render the red
  // placeholder rectangle.
}

void SurfaceEmbedWebPlugin::UpdateGeometry(const gfx::Rect& window_rect,
                                           const gfx::Rect& clip_rect,
                                           const gfx::Rect& unobscured_rect,
                                           bool is_visible) {
  if (plugin_rect_ == window_rect) {
    return;
  }

  plugin_rect_ = window_rect;

  if (layer_) {
    layer_->SetBounds(window_rect.size());
  }
}

void SurfaceEmbedWebPlugin::UpdateFocus(bool focused,
                                        blink::mojom::FocusType focus_type) {
  NOTIMPLEMENTED();
}

void SurfaceEmbedWebPlugin::UpdateVisibility(bool visible) {
  NOTIMPLEMENTED();
}

blink::WebInputEventResult SurfaceEmbedWebPlugin::HandleInputEvent(
    const blink::WebCoalescedInputEvent& event,
    ui::Cursor* cursor) {
  return blink::WebInputEventResult::kNotHandled;
}

void SurfaceEmbedWebPlugin::DidReceiveResponse(
    const blink::WebURLResponse& response) {
  NOTIMPLEMENTED();
}

void SurfaceEmbedWebPlugin::DidReceiveData(base::span<const char> data) {
  NOTIMPLEMENTED();
}

void SurfaceEmbedWebPlugin::DidFinishLoading() {
  NOTIMPLEMENTED();
}

void SurfaceEmbedWebPlugin::DidFailLoading(const blink::WebURLError& error) {
  NOTIMPLEMENTED();
}

}  // namespace surface_embed
