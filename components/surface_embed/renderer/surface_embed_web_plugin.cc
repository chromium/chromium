// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/surface_embed/renderer/surface_embed_web_plugin.h"

#include "base/notimplemented.h"
#include "cc/layers/surface_layer.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/frame/frame_visual_properties.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
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
    const blink::WebPluginParams& params) {
  render_frame->GetBrowserInterfaceBroker().GetInterface(
      host_.BindNewPipeAndPassReceiver());
}

SurfaceEmbedWebPlugin::~SurfaceEmbedWebPlugin() = default;

bool SurfaceEmbedWebPlugin::Initialize(blink::WebPluginContainer* container) {
  container_ = container;
  InitializeSurfaceLayer();

  CHECK(host_);
  mojo::PendingRemote<mojom::SurfaceEmbed> pending_remote =
      receiver_.BindNewPipeAndPassRemote();
  host_.set_disconnect_handler(base::BindOnce(
      &SurfaceEmbedWebPlugin::OnHostDisconnected, base::Unretained(this)));
  receiver_.set_disconnect_handler(base::BindOnce(
      &SurfaceEmbedWebPlugin::OnHostDisconnected, base::Unretained(this)));

  // Set up the SurfaceEmbed interface first.
  host_->SetSurfaceEmbed(std::move(pending_remote));

  return true;
}

void SurfaceEmbedWebPlugin::Destroy() {
  if (container_) {
    container_->SetCcLayer(nullptr);
    container_ = nullptr;
  }
  layer_.reset();

  receiver_.reset();
  host_.reset();

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
  // Note: Layer bounds are set by WebPluginContainerImpl::Paint()
  // so we don't need to set them here for the time being.

  if (last_window_rect_ == window_rect && last_clip_rect_ == clip_rect &&
      last_unobscured_rect_ == unobscured_rect &&
      last_is_visible_ == is_visible && !frame_sink_id_changed_) {
    return;
  }

  last_window_rect_ = window_rect;
  last_clip_rect_ = clip_rect;
  last_unobscured_rect_ = unobscured_rect;
  last_is_visible_ = is_visible;
  frame_sink_id_changed_ = false;

  if (frame_sink_id_.is_valid()) {
    SynchronizeVisualProperties();
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

void SurfaceEmbedWebPlugin::InitializeSurfaceLayer() {
  // We'll be embedding an outside surface layer.
  layer_ = cc::SurfaceLayer::Create();
  layer_->SetIsDrawable(true);
  layer_->SetContentsOpaque(true);
  layer_->SetBackgroundColor(SkColors::kTransparent);
  layer_->SetSurfaceHitTestable(true);
  // Don't use the layer for `container_` yet because it does not have a valid
  // surface id.
}

void SurfaceEmbedWebPlugin::SynchronizeVisualProperties() {
  // Note: This is largely based on RemoteFrame's SynchronizeVisualProperties().
  // TODO(surface-embed): The following properties or pieces of functionality
  // have not yet been vetted as needed or correct implementation:
  // - css zoom between ancestor widget and embedding element (inclusive).
  // - viewport segments, do these need any adjustment for plugin location/size?
  // - compositor viewport, does it need to be more accurate (See RemoteFrame)?
  //   Right now it's the part of the plugin that's visible.
  // - capture_sequence_number
  // - cursor_accessibility_scale_factor
  // - paint holding
  // - propagate parameter (see RemoteFrame's implementation, do we need to do
  //   anything to propagate these changes through the embedded WebContents?)

  if (!frame_sink_id_.is_valid()) {
    return;
  }

  blink::FrameVisualProperties pending_visual_properties;

  // No support for auto-resize.
  pending_visual_properties.auto_resize_enabled = false;
  pending_visual_properties.min_size_for_auto_resize = gfx::Size();
  pending_visual_properties.max_size_for_auto_resize = gfx::Size();

  blink::WebFrameWidget* ancestor_widget =
      container_->GetDocument().GetFrame()->LocalRoot()->FrameWidget();
  CHECK(ancestor_widget);

  pending_visual_properties.zoom_level = ancestor_widget->GetZoomLevel();
  pending_visual_properties.css_zoom_factor =
      ancestor_widget->GetCSSZoomFactor();
  pending_visual_properties.page_scale_factor = container_->PageScaleFactor();
  pending_visual_properties.is_pinch_gesture_active =
      ancestor_widget->PinchGestureActiveInMainFrame();
  pending_visual_properties.screen_infos = ancestor_widget->GetScreenInfos();

  // For separate WebContents acting like an iframe, the "visible viewport" is
  // the portion of the plugin that is visible within the plugin bounds.
  pending_visual_properties.visible_viewport_size =
      gfx::Size(last_clip_rect_.width(), last_clip_rect_.height());

  const std::vector<gfx::Rect>& viewport_segments =
      ancestor_widget->ViewportSegments();
  pending_visual_properties.root_widget_viewport_segments.assign(
      viewport_segments.begin(), viewport_segments.end());
  pending_visual_properties.rect_in_local_root = last_window_rect_;
  pending_visual_properties.local_frame_size =
      gfx::Size(last_window_rect_.width(), last_window_rect_.height());
  pending_visual_properties.compositor_viewport = last_clip_rect_;
  pending_visual_properties.compositing_scale_factor = 1.0f;
  pending_visual_properties.capture_sequence_number = 0;
  pending_visual_properties.cursor_accessibility_scale_factor = 1.0f;

  bool synchronized_props_changed =
      !sent_visual_properties_ ||
      sent_visual_properties_->auto_resize_enabled !=
          pending_visual_properties.auto_resize_enabled ||
      sent_visual_properties_->min_size_for_auto_resize !=
          pending_visual_properties.min_size_for_auto_resize ||
      sent_visual_properties_->max_size_for_auto_resize !=
          pending_visual_properties.max_size_for_auto_resize ||
      sent_visual_properties_->local_frame_size !=
          pending_visual_properties.local_frame_size ||
      sent_visual_properties_->rect_in_local_root !=
          pending_visual_properties.rect_in_local_root ||
      sent_visual_properties_->screen_infos !=
          pending_visual_properties.screen_infos ||
      sent_visual_properties_->zoom_level !=
          pending_visual_properties.zoom_level ||
      sent_visual_properties_->css_zoom_factor !=
          pending_visual_properties.css_zoom_factor ||
      sent_visual_properties_->page_scale_factor !=
          pending_visual_properties.page_scale_factor ||
      sent_visual_properties_->compositing_scale_factor !=
          pending_visual_properties.compositing_scale_factor ||
      sent_visual_properties_->cursor_accessibility_scale_factor !=
          pending_visual_properties.cursor_accessibility_scale_factor ||
      sent_visual_properties_->is_pinch_gesture_active !=
          pending_visual_properties.is_pinch_gesture_active ||
      sent_visual_properties_->visible_viewport_size !=
          pending_visual_properties.visible_viewport_size ||
      sent_visual_properties_->compositor_viewport !=
          pending_visual_properties.compositor_viewport ||
      sent_visual_properties_->root_widget_viewport_segments !=
          pending_visual_properties.root_widget_viewport_segments ||
      sent_visual_properties_->capture_sequence_number !=
          pending_visual_properties.capture_sequence_number ||
      !sent_last_is_visible_ || sent_last_is_visible_ != last_is_visible_;

  if (synchronized_props_changed) {
    parent_local_surface_id_allocator_->GenerateId();
  }

  auto local_surface_id =
      parent_local_surface_id_allocator_->GetCurrentLocalSurfaceId();
  pending_visual_properties.local_surface_id = local_surface_id;

  viz::SurfaceId surface_id(frame_sink_id_,
                            pending_visual_properties.local_surface_id);
  CHECK(surface_id.is_valid());
  layer_->SetSurfaceId(surface_id, cc::DeadlinePolicy::UseDefaultDeadline());

  if (synchronized_props_changed) {
    host_->SynchronizeVisualProperties(pending_visual_properties,
                                       last_is_visible_);
    sent_visual_properties_ = pending_visual_properties;
    sent_last_is_visible_ = last_is_visible_;
    container_->ScheduleAnimation();
  }
}

void SurfaceEmbedWebPlugin::OnHostDisconnected() {
  // If the browser side of the connection goes down, we're in an unexpected
  // state. We expect the pipe to only be closed by the renderer.
  NOTREACHED();
}

void SurfaceEmbedWebPlugin::SetFrameSinkId(
    const ::viz::FrameSinkId& frame_sink_id) {
  CHECK(container_);
  CHECK(frame_sink_id.is_valid());
  // Make sure we use a normal layer, not crash one.
  // TODO(surface-embed): draw sad tab in a PictureLayer for crashed pages.
  container_->SetCcLayer(layer_.get());

  // The same ParentLocalSurfaceIdAllocator cannot provide LocalSurfaceIds for
  // two different frame sinks, so recreate it here.
  if (frame_sink_id_ != frame_sink_id) {
    parent_local_surface_id_allocator_ =
        std::make_unique<viz::ParentLocalSurfaceIdAllocator>();
  }
  frame_sink_id_ = frame_sink_id;
  frame_sink_id_changed_ = true;

  SynchronizeVisualProperties();
}

}  // namespace surface_embed
