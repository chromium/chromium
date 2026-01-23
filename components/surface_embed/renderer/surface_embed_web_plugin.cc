// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/surface_embed/renderer/surface_embed_web_plugin.h"

#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "cc/layers/picture_layer.h"
#include "cc/layers/surface_layer.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image_builder.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/frame/frame_visual_properties.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/platform/platform.h"
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

namespace surface_embed {

// static
SurfaceEmbedWebPlugin* SurfaceEmbedWebPlugin::Create(
    content::RenderFrame* render_frame,
    const blink::WebPluginParams& params) {
  mojo::AssociatedRemote<mojom::SurfaceEmbedHost> host;
  render_frame->GetRemoteAssociatedInterfaces()->GetInterface(&host);

  // Read the content ID from the data-content-id attribute
  int contents_id = -1;
  for (size_t i = 0; i < params.attribute_names.size(); ++i) {
    if (params.attribute_names[i].Utf8() == "data-content-id") {
      base::StringToInt(params.attribute_values[i].Utf8(), &contents_id);
      break;
    }
  }

  return new SurfaceEmbedWebPlugin(std::move(host), contents_id);
}

SurfaceEmbedWebPlugin::SurfaceEmbedWebPlugin(
    mojo::AssociatedRemote<mojom::SurfaceEmbedHost> host,
    int contents_id)
    : contents_id_(contents_id),
      host_(std::move(host)),
      parent_local_surface_id_allocator_(
          std::make_unique<viz::ParentLocalSurfaceIdAllocator>()) {}

SurfaceEmbedWebPlugin::~SurfaceEmbedWebPlugin() = default;

bool SurfaceEmbedWebPlugin::Initialize(blink::WebPluginContainer* container) {
  container_ = container;

  InitializeSurfaceLayer();

  if (host_) {
    mojo::PendingAssociatedRemote<mojom::SurfaceEmbed> pending_remote =
        receiver_.BindNewEndpointAndPassRemote();
    host_.set_disconnect_handler(
        base::BindOnce(&SurfaceEmbedWebPlugin::OnSurfaceEmbedHostDisconnected,
                       base::Unretained(this)));
    receiver_.set_disconnect_handler(
        base::BindOnce(&SurfaceEmbedWebPlugin::OnSurfaceEmbedHostDisconnected,
                       base::Unretained(this)));

    // Set up the SurfaceEmbed interface first.
    host_->SetSurfaceEmbed(std::move(pending_remote));

    // Then attach with the content ID.
    if (contents_id_ > 0) {
      host_->AttachConnector(contents_id_);
    }
  }
  return true;
}

void SurfaceEmbedWebPlugin::Destroy() {
  if (container_) {
    container_->SetCcLayer(nullptr);
  }
  layer_.reset();
  crashed_layer_.reset();

  receiver_.reset();
  host_.reset();

  delete this;
}

void SurfaceEmbedWebPlugin::OnSurfaceEmbedHostDisconnected() {
  receiver_.reset();
  host_.reset();

  // If the browser side of the connection goes down, we're in an unexpected
  // state and likely need to flag this plugin as broken.
  NOTREACHED();
}

void SurfaceEmbedWebPlugin::InitializeSurfaceLayer() {
  // We'll be embedding an outside surface layer.
  layer_ = cc::SurfaceLayer::Create();
  layer_->SetIsDrawable(true);
  layer_->SetSurfaceHitTestable(true);
  container_->SetCcLayer(layer_.get());
}

blink::WebPluginContainer* SurfaceEmbedWebPlugin::Container() const {
  return container_;
}

v8::Local<v8::Object> SurfaceEmbedWebPlugin::V8ScriptableObject(
    v8::Isolate* isolate) {
  return v8::Local<v8::Object>();
}

void SurfaceEmbedWebPlugin::UpdateAllLifecyclePhases(
    blink::DocumentUpdateReason reason) {}

void SurfaceEmbedWebPlugin::Paint(cc::PaintCanvas* canvas,
                                  const gfx::Rect& rect) {
  // No-op: rendering is now handled by the compositor layer
  NOTREACHED();
}

viz::FrameSinkId SurfaceEmbedWebPlugin::GetFrameSinkId() {
  return frame_sink_id_;
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

void SurfaceEmbedWebPlugin::SynchronizeVisualProperties() {
  // Note: This is largely based on RemoteFrame's SynchronizeVisualProperties().

  // TODO(surface-embed): The following properties or pieces of functionality
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
  DCHECK(surface_id.is_valid());
  layer_->SetSurfaceId(surface_id, cc::DeadlinePolicy::UseDefaultDeadline());

  if (synchronized_props_changed) {
    host_->SynchronizeVisualProperties(pending_visual_properties,
                                       last_is_visible_);
    sent_visual_properties_ = pending_visual_properties;
    sent_last_is_visible_ = last_is_visible_;
    container_->ScheduleAnimation();
  }
}

void SurfaceEmbedWebPlugin::UpdateFocus(bool focused,
                                        blink::mojom::FocusType focus_type) {
  host_->SetFocus(focused, focus_type);
}

void SurfaceEmbedWebPlugin::UpdateVisibility(bool is_visible) {}

void SurfaceEmbedWebPlugin::UpdateDataAttribute(
    const blink::WebString& attribute_name,
    const blink::WebString& attribute_value) {
  if (attribute_name.Utf8() != "data-content-id") {
    return;
  }

  int new_contents_id = -1;
  if (!base::StringToInt(attribute_value.Utf8(), &new_contents_id) ||
      new_contents_id == contents_id_) {
    return;
  }

  contents_id_ = new_contents_id;
  if (host_) {
    if (contents_id_ <= 0) {
      host_->DetachConnector();
      DetachInternal();
    } else {
      host_->AttachConnector(contents_id_);
    }
  }
}

blink::WebInputEventResult SurfaceEmbedWebPlugin::HandleInputEvent(
    const blink::WebCoalescedInputEvent& coalesced_event,
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

bool SurfaceEmbedWebPlugin::SupportsKeyboardFocus() const {
  // TODO(surface-embed): Test with pages with nothing focusable.
  return true;
}

void SurfaceEmbedWebPlugin::SetFrameSinkId(
    const ::viz::FrameSinkId& frame_sink_id) {
  // Make sure we use a normal layer, not crash one.
  container_->SetCcLayer(layer_.get());
  crashed_layer_.reset();

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
  sent_last_is_visible_ = std::nullopt;

  SynchronizeVisualProperties();
}

void SurfaceEmbedWebPlugin::UpdateLocalSurfaceIdFromChild(
    const ::viz::LocalSurfaceId& local_surface_id) {
  if (!parent_local_surface_id_allocator_->UpdateFromChild(local_surface_id)) {
    return;
  }

  // The viz::LocalSurfaceId has changed so we call SynchronizeVisualProperties
  // here to embed it.
  SynchronizeVisualProperties();
}

void SurfaceEmbedWebPlugin::ChildProcessGone() {
  crashed_layer_ = cc::PictureLayer::Create(this);
  crashed_layer_->SetMasksToBounds(true);
  crashed_layer_->SetIsDrawable(true);
  container_->SetCcLayer(crashed_layer_.get());
  container_->ScheduleAnimation();
}

void SurfaceEmbedWebPlugin::DetachPlugin() {
  // The browser forcibly detached the guest that was previously attached to
  // to this plugin. This can happen when the guest is being re-attached
  // elsewhere.
  contents_id_ = 0;
  // We send this change back to the renderer so that the data-content-id
  // attribute is updated accordingly. It'll be async but will allow detection
  // of the detachment eventually.
  container_->GetElement().SetAttribute("data-content-id", "0");
  DetachInternal();
}

void SurfaceEmbedWebPlugin::DetachInternal() {
  InitializeSurfaceLayer();
  SetFrameSinkId(viz::FrameSinkId());
  container_->ScheduleAnimation();
}

void SurfaceEmbedWebPlugin::RequestFocus(mojom::FocusOperation focus_op) {
  if (container_) {
    container_->GetElement().Focus();
    auto* frame = container_->GetDocument().GetFrame();
    if (frame) {
      if (focus_op == mojom::FocusOperation::kFocusBeforeSurface) {
        frame->View()->AdvanceFocus(/*reverse=*/true);
      } else if (focus_op == mojom::FocusOperation::kFocusAfterSurface) {
        frame->View()->AdvanceFocus(/*reverse=*/false);
      }
    }
  }
}

scoped_refptr<cc::DisplayItemList>
SurfaceEmbedWebPlugin::PaintContentsToDisplayList() {
  blink::WebFrameWidget* ancestor_widget =
      container_->GetDocument().GetFrame()->LocalRoot()->FrameWidget();
  auto device_scale_factor =
      ancestor_widget->GetOriginalScreenInfo().device_scale_factor;
  // Adapted from ChildFrameCompositingHelper::PaintContentsToDisplayList().
  auto layer_size = crashed_layer_->bounds();
  auto display_list = base::MakeRefCounted<cc::DisplayItemList>();
  display_list->StartPaint();
  display_list->push<cc::DrawColorOp>(SkColors::kGray, SkBlendMode::kSrc);

  SkBitmap* sad_bitmap = blink::Platform::Current()->GetSadPageBitmap();
  if (sad_bitmap) {
    float paint_width = sad_bitmap->width() * device_scale_factor;
    float paint_height = sad_bitmap->height() * device_scale_factor;
    if (layer_size.width() >= paint_width &&
        layer_size.height() >= paint_height) {
      float x = (layer_size.width() - paint_width) / 2.0f;
      float y = (layer_size.height() - paint_height) / 2.0f;
      if (device_scale_factor != 1.f) {
        display_list->push<cc::SaveOp>();
        display_list->push<cc::TranslateOp>(x, y);
        display_list->push<cc::ScaleOp>(device_scale_factor,
                                        device_scale_factor);
        x = 0;
        y = 0;
      }

      auto image = cc::PaintImageBuilder::WithDefault()
                       .set_id(cc::PaintImage::GetNextId())
                       .set_image(SkImages::RasterFromBitmap(*sad_bitmap),
                                  cc::PaintImage::GetNextContentId())
                       .TakePaintImage();
      display_list->push<cc::DrawImageOp>(image, x, y);

      if (device_scale_factor != 1.f) {
        display_list->push<cc::RestoreOp>();
      }
    }
  }
  display_list->EndPaintOfUnpaired(gfx::Rect(layer_size));
  display_list->Finalize();
  return display_list;
}

bool SurfaceEmbedWebPlugin::FillsBoundsCompletely() const {
  return true;
}

}  // namespace surface_embed
