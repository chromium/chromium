// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/layers/layer_context_impl_base_unittest.h"

#include <utility>

#include "components/viz/service/layers/layer_context_impl.h"

namespace viz {

const gfx::Rect LayerContextImplTest::kDefaultDeviceViewportRect(100, 100);
const SkColor4f LayerContextImplTest::kDefaultBackgroundColor =
    SkColors::kTransparent;
const FrameSinkId LayerContextImplTest::kDefaultFrameSinkId = FrameSinkId(1, 1);
const LocalSurfaceId LayerContextImplTest::kDefaultLocalSurfaceId(
    1,
    base::UnguessableToken::CreateForTesting(2u, 3u));
const SurfaceId LayerContextImplTest::kDefaultSurfaceId(kDefaultFrameSinkId,
                                                        kDefaultLocalSurfaceId);
const SurfaceRange LayerContextImplTest::kDefaultSurfaceRange(
    std::nullopt,
    kDefaultSurfaceId);
const gfx::Size LayerContextImplTest::kDefaultLayerBounds(10, 10);
const gfx::PointF LayerContextImplTest::kDefaultUVTopLeft = gfx::PointF();
const gfx::PointF LayerContextImplTest::kDefaultUVBottomRight =
    gfx::PointF(1.f, 1.f);
const cc::ElementId LayerContextImplTest::kDefaultScrollElementId =
    cc::ElementId();
const SkColor4f LayerContextImplTest::kDefaultSolidColorScrollbarColor =
    SkColors::kTransparent;
const gfx::Size
    LayerContextImplTest::kDefaultNinePatchThumbScrollbarImageBounds =
        gfx::Size();
const gfx::Rect LayerContextImplTest::kDefaultNinePatchThumbScrollbarAperture =
    gfx::Rect();
const cc::UIResourceId
    LayerContextImplTest::kDefaultNinePatchThumbScrollbarThumbUIResourceId = 0;
const cc::UIResourceId LayerContextImplTest::
    kDefaultNinePatchThumbScrollbarTrackAndButtonsUIResourceId = 0;
const gfx::Size
    LayerContextImplTest::kDefaultPaintedScrollbarInternalContentBounds =
        gfx::Size();
const gfx::Rect LayerContextImplTest::kDefaultPaintedScrollbarBackButtonRect =
    gfx::Rect();
const gfx::Rect
    LayerContextImplTest::kDefaultPaintedScrollbarForwardButtonRect =
        gfx::Rect();
const gfx::Rect LayerContextImplTest::kDefaultPaintedScrollbarTrackRect =
    gfx::Rect();
const std::optional<SkColor4f>
    LayerContextImplTest::kDefaultPaintedScrollbarThumbColor = std::nullopt;
const gfx::Size
    LayerContextImplTest::kDefaultPaintedScrollbarTrackAndButtonsImageBounds =
        gfx::Size();
const gfx::Rect
    LayerContextImplTest::kDefaultPaintedScrollbarTrackAndButtonsAperture =
        gfx::Rect();
const ViewTransitionElementResourceId
    LayerContextImplTest::kDefaultViewTransitionContentLayerResourceId =
        ViewTransitionElementResourceId(blink::ViewTransitionToken(), 1, false);
const gfx::RectF
    LayerContextImplTest::kDefaultViewTransitionContentLayerMaxExtentsRect;
const std::optional<SkColor4f>
    LayerContextImplTest::kDefaultTileDisplaySolidColor = std::nullopt;

// Default UIResourceLayer property values
const gfx::Size LayerContextImplTest::kDefaultUIResourceImageBounds =
    gfx::Size();
const gfx::PointF LayerContextImplTest::kDefaultUIResourceUVTopLeft =
    gfx::PointF(0.0f, 0.0f);
const gfx::PointF LayerContextImplTest::kDefaultUIResourceUVBottomRight =
    gfx::PointF(1.0f, 1.0f);

// Default NinePatchLayer property values
const gfx::Size LayerContextImplTest::kDefaultNinePatchImageBounds =
    gfx::Size();
const gfx::PointF LayerContextImplTest::kDefaultNinePatchUVTopLeft =
    gfx::PointF(0.0f, 0.0f);
const gfx::PointF LayerContextImplTest::kDefaultNinePatchUVBottomRight =
    gfx::PointF(1.0f, 1.0f);

const gfx::Rect LayerContextImplTest::kDefaultNinePatchAperture = gfx::Rect();
const gfx::Rect LayerContextImplTest::kDefaultNinePatchBorder = gfx::Rect();
const gfx::Rect LayerContextImplTest::kDefaultNinePatchLayerOcclusion =
    gfx::Rect();

LayerContextImplTest::LayerContextImplTest()
    : frame_sink_manager_(FrameSinkManagerImpl::InitParams()) {}

LayerContextImplTest::~LayerContextImplTest() = default;

void LayerContextImplTest::SetUp() {
  compositor_frame_sink_support_ = std::make_unique<CompositorFrameSinkSupport>(
      &dummy_client_, &frame_sink_manager_, kDefaultFrameSinkId,
      /*is_root=*/true);
  auto settings = mojom::LayerContextSettings::New();
  settings->draw_mode_is_gpu = true;
  settings->enable_edge_anti_aliasing = true;
  layer_context_impl_ = LayerContextImpl::CreateForTesting(
      compositor_frame_sink_support_.get(), std::move(settings));
}

void LayerContextImplTest::RecreateLayerContextImplWithSettings(
    mojom::LayerContextSettingsPtr settings) {
  layer_context_impl_ = LayerContextImpl::CreateForTesting(
      compositor_frame_sink_support_.get(), std::move(settings));
}

void LayerContextImplTest::ResetTestState() {
  // Property tree node IDs and layers are reinitialized in
  // CreateDefaultUpdate if first_update_ is true.
  first_update_ = true;
}

mojom::LayerTreeUpdatePtr LayerContextImplTest::CreateDefaultUpdate() {
  auto update = mojom::LayerTreeUpdate::New();

  if (first_update_) {
    AddFirstTimeDefaultProperties(update.get());
    first_update_ = false;
  }
  AddDefaultPropertyUpdates(update.get());

  return update;
}

void LayerContextImplTest::AddDefaultPropertyUpdates(
    mojom::LayerTreeUpdate* update) {
  update->source_frame_number = 1;
  update->trace_id = 1;
  update->primary_main_frame_item_sequence_number = 1;
  update->device_viewport = kDefaultDeviceViewportRect;
  update->background_color = kDefaultBackgroundColor;

  // Valid scale factors by default
  update->page_scale_factor = kDefaultPageScaleFactor;
  update->min_page_scale_factor = kDefaultMinPageScaleFactor;
  update->max_page_scale_factor = kDefaultMaxPageScaleFactor;
  update->external_page_scale_factor = kDefaultExternalPageScaleFactor;
  update->device_scale_factor = kDefaultDeviceScaleFactor;
  update->painted_device_scale_factor = kDefaultPaintedDeviceScaleFactor;

  update->top_controls_shown_ratio =
      LayerContextImplTest::kDefaultTopControlsShownRatio;
  update->bottom_controls_shown_ratio = kDefaultBottomControlsShownRatio;

  update->num_transform_nodes = next_transform_id_;
  update->num_clip_nodes = next_clip_id_;
  update->num_effect_nodes = next_effect_id_;
  update->num_scroll_nodes = next_scroll_id_;

  // Viewport property IDs
  update->overscroll_elasticity_transform =
      viewport_property_ids.overscroll_elasticity_transform;
  update->page_scale_transform = viewport_property_ids.page_scale_transform;
  update->inner_scroll = viewport_property_ids.inner_scroll;
  update->outer_clip = viewport_property_ids.outer_clip;
  update->outer_scroll = viewport_property_ids.outer_scroll;

  // Other defaults
  update->display_color_spaces = gfx::DisplayColorSpaces();
  update->local_surface_id_from_parent = kDefaultLocalSurfaceId;
  update->current_local_surface_id = kDefaultLocalSurfaceId;
  update->next_frame_token = 1;

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta interval = base::Milliseconds(16);
  update->begin_frame_args = BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, BeginFrameArgs::kStartingSourceId,
      BeginFrameArgs::kStartingFrameNumber, now, now + interval, interval,
      BeginFrameArgs::NORMAL);
}

void LayerContextImplTest::AddFirstTimeDefaultProperties(
    mojom::LayerTreeUpdate* update) {
  // Set internal state to defaults.
  next_layer_id_ = 1;
  next_transform_id_ = 0;
  next_clip_id_ = 0;
  next_effect_id_ = 0;
  next_scroll_id_ = 0;
  layer_order_.clear();

  // Root & Secondary transform nodes are always expected 1st
  AddTransformNode(update, cc::kInvalidPropertyNodeId);
  AddTransformNode(update, cc::kRootPropertyNodeId);

  // Updating the page scale requires that a page_scale_transform node is
  // set up.
  viewport_property_ids.overscroll_elasticity_transform =
      AddTransformNode(update, cc::kSecondaryRootPropertyNodeId);
  viewport_property_ids.page_scale_transform = AddTransformNode(
      update, viewport_property_ids.overscroll_elasticity_transform);
  update->transform_nodes.back()->in_subtree_of_page_scale_layer = true;

  // Root & Secondary clip nodes are always expected
  AddClipNode(update, cc::kInvalidPropertyNodeId);
  AddClipNode(update, cc::kRootPropertyNodeId);

  // Root & Secondary effect nodes are always expected
  AddEffectNode(update, cc::kInvalidPropertyNodeId);
  AddEffectNode(update, cc::kRootPropertyNodeId);
  update->effect_nodes.back()->render_surface_reason =
      cc::RenderSurfaceReason::kRoot;
  update->effect_nodes.back()->element_id = cc::ElementId(1ULL);

  // Root & Secondary scroll nodes are always expected
  AddScrollNode(update, cc::kInvalidPropertyNodeId);
  viewport_property_ids.outer_scroll =
      AddScrollNode(update, cc::kRootPropertyNodeId);
  update->scroll_nodes.back()->element_id = cc::ElementId(1ULL);
  viewport_property_ids.inner_scroll =
      AddScrollNode(update, viewport_property_ids.outer_scroll);
  update->scroll_nodes.back()->element_id = cc::ElementId(1ULL);

  // Root layer
  AddDefaultLayerToUpdate(update);
}

int LayerContextImplTest::AddTransformNode(mojom::LayerTreeUpdate* update,
                                           int parent) {
  auto node = mojom::TransformNode::New();
  int id = next_transform_id_++;
  node->id = id;
  node->parent_id = parent;
  update->transform_nodes.push_back(std::move(node));
  update->num_transform_nodes = next_transform_id_;
  return id;
}

int LayerContextImplTest::AddClipNode(mojom::LayerTreeUpdate* update,
                                      int parent) {
  auto node = mojom::ClipNode::New();
  int id = next_clip_id_++;
  node->id = id;
  node->parent_id = parent;
  node->transform_id = viewport_property_ids.page_scale_transform;
  update->clip_nodes.push_back(std::move(node));
  update->num_clip_nodes = next_clip_id_;
  return id;
}

int LayerContextImplTest::AddEffectNode(mojom::LayerTreeUpdate* update,
                                        int parent) {
  auto node = mojom::EffectNode::New();
  int id = next_effect_id_++;
  node->id = id;
  node->parent_id = parent;
  node->transform_id = viewport_property_ids.page_scale_transform;
  node->clip_id = cc::kRootPropertyNodeId;
  node->target_id = cc::kRootPropertyNodeId;
  update->effect_nodes.push_back(std::move(node));
  update->num_effect_nodes = next_effect_id_;
  return id;
}

int LayerContextImplTest::AddScrollNode(mojom::LayerTreeUpdate* update,
                                        int parent) {
  auto node = mojom::ScrollNode::New();
  int id = next_scroll_id_++;
  node->id = id;
  node->parent_id = parent;
  node->transform_id = viewport_property_ids.page_scale_transform;
  update->scroll_nodes.push_back(std::move(node));
  update->num_scroll_nodes = next_scroll_id_;
  return id;
}

mojom::ScrollbarLayerBaseExtraPtr
LayerContextImplTest::CreateDefaultScrollbarBaseExtra() {
  auto base_extra = mojom::ScrollbarLayerBaseExtra::New();
  base_extra->scroll_element_id = kDefaultScrollElementId;
  base_extra->is_overlay_scrollbar = kDefaultIsOverlayScrollbar;
  base_extra->is_web_test = kDefaultIsWebTest;
  base_extra->thumb_thickness_scale_factor = kDefaultThumbThicknessScaleFactor;
  base_extra->current_pos = kDefaultCurrentPos;
  base_extra->clip_layer_length = kDefaultClipLayerLength;
  base_extra->scroll_layer_length = kDefaultScrollLayerLength;
  base_extra->vertical_adjust = kDefaultVerticalAdjust;
  base_extra->has_find_in_page_tickmarks = kDefaultHasFindInPageTickmarks;
  base_extra->is_horizontal_orientation = kDefaultIsHorizontalOrientation;
  base_extra->is_left_side_vertical_scrollbar =
      kDefaultIsLeftSideVerticalScrollbar;
  return base_extra;
}

mojom::LayerExtraPtr LayerContextImplTest::CreateDefaultLayerExtra(
    cc::mojom::LayerType type) {
  switch (type) {
    case cc::mojom::LayerType::kTexture: {
      auto extra = mojom::TextureLayerExtra::New();
      extra->blend_background_color = kDefaultBlendBackgroundColor;
      extra->force_texture_to_opaque = kDefaultForceTextureToOpaque;
      extra->uv_top_left = kDefaultUVTopLeft;
      extra->uv_bottom_right = kDefaultUVBottomRight;
      return mojom::LayerExtra::NewTextureLayerExtra(std::move(extra));
    }
    case cc::mojom::LayerType::kSurface: {
      auto extra = mojom::SurfaceLayerExtra::New();
      extra->surface_range = kDefaultSurfaceRange;
      extra->deadline_in_frames = kDefaultDeadlineInFrames;
      extra->stretch_content_to_fill_bounds =
          kDefaultStretchContentToFillBounds;
      extra->surface_hit_testable = kDefaultSurfaceHitTestable;
      extra->has_pointer_events_none = kDefaultHasPointerEventsNone;
      extra->is_reflection = kDefaultIsReflection;
      extra->will_draw_needs_reset = kDefaultWillDrawNeedsReset;
      extra->override_child_paint_flags = kDefaultOverrideChildPaintFlags;
      return mojom::LayerExtra::NewSurfaceLayerExtra(std::move(extra));
    }
    case cc::mojom::LayerType::kSolidColorScrollbar: {
      auto extra = mojom::SolidColorScrollbarLayerExtra::New();
      extra->scrollbar_base_extra = CreateDefaultScrollbarBaseExtra();
      extra->color = kDefaultSolidColorScrollbarColor;
      extra->thumb_thickness = kDefaultSolidColorScrollbarThumbThickness;
      extra->track_start = kDefaultSolidColorScrollbarTrackStart;
      return mojom::LayerExtra::NewSolidColorScrollbarLayerExtra(
          std::move(extra));
    }
    case cc::mojom::LayerType::kNinePatchThumbScrollbar: {
      auto extra = mojom::NinePatchThumbScrollbarLayerExtra::New();
      extra->scrollbar_base_extra = CreateDefaultScrollbarBaseExtra();
      extra->thumb_thickness = kDefaultNinePatchThumbScrollbarThumbThickness;
      extra->thumb_length = kDefaultNinePatchThumbScrollbarThumbLength;
      extra->track_start = kDefaultNinePatchThumbScrollbarTrackStart;
      extra->track_length = kDefaultNinePatchThumbScrollbarTrackLength;
      extra->image_bounds = kDefaultNinePatchThumbScrollbarImageBounds;
      extra->aperture = kDefaultNinePatchThumbScrollbarAperture;
      extra->thumb_ui_resource_id =
          kDefaultNinePatchThumbScrollbarThumbUIResourceId;
      extra->track_and_buttons_ui_resource_id =
          kDefaultNinePatchThumbScrollbarTrackAndButtonsUIResourceId;
      return mojom::LayerExtra::NewNinePatchThumbScrollbarLayerExtra(
          std::move(extra));
    }
    case cc::mojom::LayerType::kMirror: {
      auto extra = mojom::MirrorLayerExtra::New();
      extra->mirrored_layer_id = kDefaultMirrorLayerMirroredLayerId;
      return mojom::LayerExtra::NewMirrorLayerExtra(std::move(extra));
    }
    case cc::mojom::LayerType::kNinePatch: {
      auto extra = mojom::NinePatchLayerExtra::New();
      extra->image_aperture = kDefaultNinePatchAperture;
      extra->border = kDefaultNinePatchBorder;
      extra->layer_occlusion = kDefaultNinePatchLayerOcclusion;
      extra->fill_center = kDefaultNinePatchFillCenter;
      extra->ui_resource_id = kDefaultNinePatchUIResourceId;
      extra->image_bounds = kDefaultNinePatchImageBounds;
      extra->uv_top_left = kDefaultNinePatchUVTopLeft;
      extra->uv_bottom_right = kDefaultNinePatchUVBottomRight;
      return mojom::LayerExtra::NewNinePatchLayerExtra(std::move(extra));
    }
    case cc::mojom::LayerType::kPaintedScrollbar: {
      auto extra = mojom::PaintedScrollbarLayerExtra::New();
      extra->scrollbar_base_extra = CreateDefaultScrollbarBaseExtra();
      extra->internal_contents_scale =
          kDefaultPaintedScrollbarInternalContentsScale;
      extra->internal_content_bounds =
          kDefaultPaintedScrollbarInternalContentBounds;
      extra->jump_on_track_click = kDefaultPaintedScrollbarJumpOnTrackClick;
      extra->supports_drag_snap_back =
          kDefaultPaintedScrollbarSupportsDragSnapBack;
      extra->thumb_thickness = kDefaultPaintedScrollbarThumbThickness;
      extra->thumb_length = kDefaultPaintedScrollbarThumbLength;
      extra->track_and_buttons_ui_resource_id =
          kDefaultPaintedScrollbarTrackAndButtonsUIResourceId;
      extra->thumb_ui_resource_id = kDefaultPaintedScrollbarThumbUIResourceId;
      extra->uses_nine_patch_track_and_buttons =
          kDefaultPaintedScrollbarUsesNinePatchTrackAndButtons;
      extra->painted_opacity = kDefaultPaintedScrollbarPaintedOpacity;
      extra->thumb_color = kDefaultPaintedScrollbarThumbColor;
      extra->track_and_buttons_image_bounds =
          kDefaultPaintedScrollbarTrackAndButtonsImageBounds;
      extra->track_and_buttons_aperture =
          kDefaultPaintedScrollbarTrackAndButtonsAperture;
      return mojom::LayerExtra::NewPaintedScrollbarLayerExtra(std::move(extra));
    }
    case cc::mojom::LayerType::kViewTransitionContent: {
      auto extra = mojom::ViewTransitionContentLayerExtra::New();
      extra->resource_id = kDefaultViewTransitionContentLayerResourceId;
      extra->is_live_content_layer =
          kDefaultViewTransitionContentLayerIsLiveContentLayer;
      extra->max_extents_rect =
          kDefaultViewTransitionContentLayerMaxExtentsRect;
      return mojom::LayerExtra::NewViewTransitionContentLayerExtra(
          std::move(extra));
    }
    case cc::mojom::LayerType::kTileDisplay: {
      auto extra = mojom::TileDisplayLayerExtra::New();
      extra->solid_color = kDefaultTileDisplaySolidColor;
      extra->is_backdrop_filter_mask = kDefaultTileDisplayIsBackdropFilterMask;
      return mojom::LayerExtra::NewTileDisplayLayerExtra(std::move(extra));
    }
    case cc::mojom::LayerType::kUIResource: {
      auto extra = mojom::UIResourceLayerExtra::New();
      extra->ui_resource_id = kDefaultUIResourceId;
      extra->image_bounds = kDefaultUIResourceImageBounds;
      extra->uv_top_left = kDefaultUIResourceUVTopLeft;
      extra->uv_bottom_right = kDefaultUIResourceUVBottomRight;
      return mojom::LayerExtra::NewUiResourceLayerExtra(std::move(extra));
    }

    default:
      // TODO(vmiura): Add each layer type's initialization.
      return nullptr;
  }
}

// Helper to add a default layer to the update.
// Returns the ID of the added layer.
int LayerContextImplTest::AddDefaultLayerToUpdate(
    mojom::LayerTreeUpdate* update,
    cc::mojom::LayerType type,
    int id) {
  auto layer = mojom::Layer::New();
  if (id == -1) {
    id = next_layer_id_++;
  }
  layer->id = id;
  layer->type = type;
  layer->transform_tree_index = cc::kSecondaryRootPropertyNodeId;
  layer->clip_tree_index = cc::kRootPropertyNodeId;
  layer->effect_tree_index = cc::kSecondaryRootPropertyNodeId;
  layer->bounds = kDefaultLayerBounds;
  layer->layer_extra = CreateDefaultLayerExtra(type);

  update->layers.push_back(std::move(layer));

  // Update the local layer order, and in the LayerTreeUpdate.
  layer_order_.push_back(id);
  update->layer_order = layer_order_;
  return id;
}

void LayerContextImplTest::RemoveLayerInUpdate(mojom::LayerTreeUpdate* update,
                                               int id) {
  // Remove the ID from the local list of layers if it exists.
  auto it = std::find(layer_order_.begin(), layer_order_.end(), id);
  if (it != layer_order_.end()) {
    layer_order_.erase(it);
  }

  // Update the layer order in the LayerTreeUpdate.
  update->layer_order = layer_order_;
}

TransferableResource LayerContextImplTest::MakeFakeResource(gfx::Size size) {
  auto sync_token =
      gpu::SyncToken(gpu::CommandBufferNamespace::GPU_IO,
                     gpu::CommandBufferId::FromUnsafeValue(0x234), 0x456);
  auto shared_image = gpu::ClientSharedImage::CreateForTesting(
      {SinglePlaneFormat::kRGBA_8888, size, gfx::ColorSpace(),
       kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
       gpu::SHARED_IMAGE_USAGE_DISPLAY_READ},
      GL_TEXTURE_2D);

  return TransferableResource::Make(
      shared_image, TransferableResource::ResourceSource::kTest, sync_token);
}

mojom::TransferableUIResourceRequestPtr
LayerContextImplTest::CreateUIResourceRequest(
    int uid,
    mojom::TransferableUIResourceRequest::Type type) {
  auto request = mojom::TransferableUIResourceRequest::New();
  request->uid = uid;
  request->type = type;
  if (type == mojom::TransferableUIResourceRequest::Type::kCreate) {
    // Add a minimal valid resource.
    request->transferable_resource = MakeFakeResource(gfx::Size(1, 1));
  }
  return request;
}

cc::LayerImpl* LayerContextImplTest::GetLayerFromActiveTree(int layer_id) {
  return layer_context_impl_->host_impl()->active_tree()->LayerById(layer_id);
}

mojom::LayerPtr LayerContextImplTest::CreateManualLayer(
    int id,
    cc::mojom::LayerType type,
    const gfx::Size& bounds,
    int transform_idx,
    int clip_idx,
    int effect_idx,
    int scroll_idx) {
  auto layer = mojom::Layer::New();
  layer->id = id;
  layer->type = type;
  layer->bounds = bounds;
  layer->transform_tree_index = transform_idx;
  layer->clip_tree_index = clip_idx;
  layer->effect_tree_index = effect_idx;
  layer->scroll_tree_index = scroll_idx;
  layer->layer_extra = CreateDefaultLayerExtra(type);
  return layer;
}

}  // namespace viz
