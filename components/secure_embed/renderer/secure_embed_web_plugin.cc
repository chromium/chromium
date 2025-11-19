// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/secure_embed/renderer/secure_embed_web_plugin.h"

#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "cc/layers/surface_layer.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/frame/frame_visual_properties.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "v8/include/v8-local-handle.h"

namespace secure_embed {

// static
SecureEmbedWebPlugin* SecureEmbedWebPlugin::Create(
    content::RenderFrame* render_frame,
    const blink::WebPluginParams& params) {
  mojo::AssociatedRemote<mojom::SecureEmbedHost> host;
  render_frame->GetRemoteAssociatedInterfaces()->GetInterface(&host);

  // Read the content ID from the data-content-id attribute
  int contents_id = -1;
  for (size_t i = 0; i < params.attribute_names.size(); ++i) {
    if (params.attribute_names[i].Utf8() == "data-content-id") {
      base::StringToInt(params.attribute_values[i].Utf8(), &contents_id);
      break;
    }
  }

  return new SecureEmbedWebPlugin(std::move(host), contents_id);
}

SecureEmbedWebPlugin::SecureEmbedWebPlugin(
    mojo::AssociatedRemote<mojom::SecureEmbedHost> host,
    int contents_id)
    : contents_id_(contents_id),
      host_(std::move(host)),
      parent_local_surface_id_allocator_(
          std::make_unique<viz::ParentLocalSurfaceIdAllocator>()) {}

SecureEmbedWebPlugin::~SecureEmbedWebPlugin() = default;

bool SecureEmbedWebPlugin::Initialize(blink::WebPluginContainer* container) {
  container_ = container;

  // We'll be embedding an outside surface layer.
  layer_ = cc::SurfaceLayer::Create();
  layer_->SetIsDrawable(true);
  layer_->SetSurfaceHitTestable(true);

  // Provide the layer to the container
  container_->SetCcLayer(layer_.get());

  if (host_) {
    mojo::PendingAssociatedRemote<mojom::SecureEmbed> pending_remote =
        receiver_.BindNewEndpointAndPassRemote();
    host_.set_disconnect_handler(
        base::BindOnce(&SecureEmbedWebPlugin::OnSecureEmbedHostDisconnected,
                       base::Unretained(this)));
    receiver_.set_disconnect_handler(
        base::BindOnce(&SecureEmbedWebPlugin::OnSecureEmbedHostDisconnected,
                       base::Unretained(this)));

    // Set up the SecureEmbed interface first.
    host_->SetSecureEmbed(std::move(pending_remote));

    // Then attach with the content ID.
    host_->Attach(contents_id_);
  }
  return true;
}

void SecureEmbedWebPlugin::Destroy() {
  if (container_ && layer_) {
    container_->SetCcLayer(nullptr);
  }
  layer_ = nullptr;

  receiver_.reset();
  host_.reset();

  delete this;
}

void SecureEmbedWebPlugin::OnSecureEmbedHostDisconnected() {
  receiver_.reset();
  host_.reset();

  // If the browser side of the connection goes down, we're in an unexpected
  // state and likely need to flag this plugin as broken.
  NOTREACHED();
}

blink::WebPluginContainer* SecureEmbedWebPlugin::Container() const {
  return container_;
}

v8::Local<v8::Object> SecureEmbedWebPlugin::V8ScriptableObject(
    v8::Isolate* isolate) {
  return v8::Local<v8::Object>();
}

void SecureEmbedWebPlugin::UpdateAllLifecyclePhases(
    blink::DocumentUpdateReason reason) {}

void SecureEmbedWebPlugin::Paint(cc::PaintCanvas* canvas,
                                 const gfx::Rect& rect) {
  // No-op: rendering is now handled by the compositor layer
  NOTREACHED();
}

viz::FrameSinkId SecureEmbedWebPlugin::GetFrameSinkId() {
  return frame_sink_id_;
}

void SecureEmbedWebPlugin::UpdateGeometry(const gfx::Rect& window_rect,
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

void SecureEmbedWebPlugin::SynchronizeVisualProperties() {
  // Note: This is largely based on RemoteFrame's SynchronizeVisualProperties().

  // TODO(secure-embed): The following properties or pieces of functionality
  // have not yet been vetted as needed or correct implementation:
  // - css zoom between ancestor widget and embedding element (inclusive).
  // - viewport segments, do these need any adjustment for plugin location/size?
  // - compositor viewport, does it need to be more accurate (See RemoteFrame)?
  //   Right now it's the part of the plugin that's visible.
  // - compositing scale factor
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
  DCHECK(ancestor_widget);

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
          pending_visual_properties.capture_sequence_number;

  if (synchronized_props_changed) {
    parent_local_surface_id_allocator_->GenerateId();
  }

  auto local_surface_id =
      parent_local_surface_id_allocator_->GetCurrentLocalSurfaceId();
  pending_visual_properties.local_surface_id = local_surface_id;

  viz::SurfaceId surface_id(frame_sink_id_,
                            pending_visual_properties.local_surface_id);
  DCHECK(surface_id.is_valid());
  layer_->SetSurfaceId(surface_id, cc::DeadlinePolicy::UseDefaultDeadline());

  if (synchronized_props_changed) {
    host_->SynchronizeVisualProperties(pending_visual_properties,
                                       last_is_visible_);
    sent_visual_properties_ = pending_visual_properties;
    container_->ScheduleAnimation();
  }
}

void SecureEmbedWebPlugin::UpdateFocus(bool focused,
                                       blink::mojom::FocusType focus_type) {
  host_->SetFocus(focused, focus_type);
}

void SecureEmbedWebPlugin::UpdateVisibility(bool is_visible) {}

blink::WebInputEventResult SecureEmbedWebPlugin::HandleInputEvent(
    const blink::WebCoalescedInputEvent& coalesced_event,
    ui::Cursor* cursor) {
  return blink::WebInputEventResult::kNotHandled;
}

void SecureEmbedWebPlugin::DidReceiveResponse(
    const blink::WebURLResponse& response) {
  NOTIMPLEMENTED();
}

void SecureEmbedWebPlugin::DidReceiveData(base::span<const char> data) {
  NOTIMPLEMENTED();
}

void SecureEmbedWebPlugin::DidFinishLoading() {
  NOTIMPLEMENTED();
}

void SecureEmbedWebPlugin::DidFailLoading(const blink::WebURLError& error) {
  NOTIMPLEMENTED();
}

bool SecureEmbedWebPlugin::SupportsKeyboardFocus() const {
  // TODO(secure-embed): Test with pages with nothing focusable.
  return true;
}

void SecureEmbedWebPlugin::SetFrameSinkId(
    const ::viz::FrameSinkId& frame_sink_id) {
  // The same ParentLocalSurfaceIdAllocator cannot provide LocalSurfaceIds for
  // two different frame sinks, so recreate it here.
  if (frame_sink_id_ != frame_sink_id) {
    parent_local_surface_id_allocator_ =
        std::make_unique<viz::ParentLocalSurfaceIdAllocator>();
  }
  frame_sink_id_ = frame_sink_id;
  frame_sink_id_changed_ = true;

  // Any visual properties previously sent are now invalid.
  sent_visual_properties_ = std::nullopt;

  SynchronizeVisualProperties();
}

void SecureEmbedWebPlugin::UpdateLocalSurfaceIdFromChild(
    const ::viz::LocalSurfaceId& local_surface_id) {
  if (!parent_local_surface_id_allocator_->UpdateFromChild(local_surface_id)) {
    return;
  }

  // The viz::LocalSurfaceId has changed so we call SynchronizeVisualProperties
  // here to embed it.
  SynchronizeVisualProperties();
}

void SecureEmbedWebPlugin::RequestFocus(mojom::FocusOperation focus_op) {
  if (container_) {
    container_->GetElement().Focus();
    auto* frame = container_->GetDocument().GetFrame();
    if (frame) {
      if (focus_op == mojom::FocusOperation::kFocusBeforePlugin) {
        frame->View()->AdvanceFocus(/*reverse=*/true);
      } else if (focus_op == mojom::FocusOperation::kFocusAfterPlugin) {
        frame->View()->AdvanceFocus(/*reverse=*/false);
      }
    }
  }
}

}  // namespace secure_embed
