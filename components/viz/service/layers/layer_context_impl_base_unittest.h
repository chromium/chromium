// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_LAYERS_LAYER_CONTEXT_IMPL_BASE_UNITTEST_H_
#define COMPONENTS_VIZ_SERVICE_LAYERS_LAYER_CONTEXT_IMPL_BASE_UNITTEST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/test/fake_compositor_frame_sink_client.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "services/viz/public/mojom/compositing/layer_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {

class LayerContextImplTest : public testing::Test {
 public:
  // Default layer tree property values
  static constexpr float kDefaultPageScaleFactor = 1.0f;
  static constexpr float kDefaultMinPageScaleFactor = 0.5f;
  static constexpr float kDefaultMaxPageScaleFactor = 2.0f;
  static const gfx::Rect kDefaultDeviceViewportRect;
  static const SkColor4f kDefaultBackgroundColor;
  static constexpr float kDefaultExternalPageScaleFactor = 1.0f;
  static constexpr float kDefaultDeviceScaleFactor = 1.0f;
  static constexpr float kDefaultPaintedDeviceScaleFactor = 1.0f;
  static constexpr float kDefaultTopControlsShownRatio = 1.0f;
  static constexpr float kDefaultBottomControlsShownRatio = 1.0f;
  static const FrameSinkId kDefaultFrameSinkId;
  static const LocalSurfaceId kDefaultLocalSurfaceId;
  static const SurfaceId kDefaultSurfaceId;
  static const SurfaceRange kDefaultSurfaceRange;

  // Default Layer property values
  static const gfx::Size kDefaultLayerBounds;

  // Default TextureLayer property values
  static constexpr bool kDefaultBlendBackgroundColor = false;
  static constexpr bool kDefaultForceTextureToOpaque = false;
  static const gfx::PointF kDefaultUVTopLeft;
  static const gfx::PointF kDefaultUVBottomRight;

  // Default SurfaceLayer property values
  static constexpr uint32_t kDefaultDeadlineInFrames = 0u;
  static constexpr bool kDefaultStretchContentToFillBounds = false;
  static constexpr bool kDefaultSurfaceHitTestable = false;
  static constexpr bool kDefaultHasPointerEventsNone = false;
  static constexpr bool kDefaultIsReflection = false;
  static constexpr bool kDefaultWillDrawNeedsReset = false;
  static constexpr bool kDefaultOverrideChildPaintFlags = false;

  // Default ScrollbarLayerBaseExtra property values
  static const cc::ElementId kDefaultScrollElementId;
  static constexpr bool kDefaultIsOverlayScrollbar = false;
  static constexpr bool kDefaultIsWebTest = false;
  static constexpr float kDefaultThumbThicknessScaleFactor = 0.f;
  static constexpr float kDefaultCurrentPos = 0.f;
  static constexpr float kDefaultClipLayerLength = 0.f;
  static constexpr float kDefaultScrollLayerLength = 0.f;
  static constexpr float kDefaultVerticalAdjust = 0.f;
  static constexpr bool kDefaultHasFindInPageTickmarks = false;
  static constexpr bool kDefaultIsHorizontalOrientation = false;
  static constexpr bool kDefaultIsLeftSideVerticalScrollbar = false;

  // Default SolidColorScrollbarLayer property values
  static const SkColor4f kDefaultSolidColorScrollbarColor;
  static constexpr int kDefaultSolidColorScrollbarThumbThickness = 0;
  static constexpr int kDefaultSolidColorScrollbarTrackStart = 0;

  // Default NinePatchThumbScrollbarLayer property values
  static constexpr int kDefaultNinePatchThumbScrollbarThumbThickness = 0;
  static constexpr int kDefaultNinePatchThumbScrollbarThumbLength = 0;
  static constexpr int kDefaultNinePatchThumbScrollbarTrackStart = 0;
  static constexpr int kDefaultNinePatchThumbScrollbarTrackLength = 0;
  static const gfx::Size kDefaultNinePatchThumbScrollbarImageBounds;
  static const gfx::Rect kDefaultNinePatchThumbScrollbarAperture;
  static const cc::UIResourceId
      kDefaultNinePatchThumbScrollbarThumbUIResourceId;
  static const cc::UIResourceId
      kDefaultNinePatchThumbScrollbarTrackAndButtonsUIResourceId;

  // Default PaintedScrollbarLayer property values
  static constexpr float kDefaultPaintedScrollbarInternalContentsScale = 0.f;
  static const gfx::Size kDefaultPaintedScrollbarInternalContentBounds;
  static constexpr bool kDefaultPaintedScrollbarJumpOnTrackClick = false;
  static constexpr bool kDefaultPaintedScrollbarSupportsDragSnapBack = false;
  static constexpr int kDefaultPaintedScrollbarThumbThickness = 0;
  static constexpr int kDefaultPaintedScrollbarThumbLength = 0;
  static const gfx::Rect kDefaultPaintedScrollbarBackButtonRect;
  static const gfx::Rect kDefaultPaintedScrollbarForwardButtonRect;
  static const gfx::Rect kDefaultPaintedScrollbarTrackRect;
  static constexpr cc::UIResourceId
      kDefaultPaintedScrollbarTrackAndButtonsUIResourceId = 0;
  static constexpr cc::UIResourceId kDefaultPaintedScrollbarThumbUIResourceId =
      0;
  static constexpr bool kDefaultPaintedScrollbarUsesNinePatchTrackAndButtons =
      false;
  static constexpr float kDefaultPaintedScrollbarPaintedOpacity = 1.f;
  static const std::optional<SkColor4f> kDefaultPaintedScrollbarThumbColor;
  static const gfx::Size kDefaultPaintedScrollbarTrackAndButtonsImageBounds;
  static const gfx::Rect kDefaultPaintedScrollbarTrackAndButtonsAperture;

  // Default MirrorLayer property values
  static constexpr int kDefaultMirrorLayerMirroredLayerId = 0;

  // Default ViewTransitionContentLayer property values
  static const ViewTransitionElementResourceId
      kDefaultViewTransitionContentLayerResourceId;
  static constexpr bool kDefaultViewTransitionContentLayerIsLiveContentLayer =
      false;
  static const gfx::RectF kDefaultViewTransitionContentLayerMaxExtentsRect;

  // Default TileDisplayLayer property values
  static const std::optional<SkColor4f> kDefaultTileDisplaySolidColor;
  static constexpr bool kDefaultTileDisplayIsBackdropFilterMask = false;

  // Default UIResourceLayer property values
  static constexpr cc::UIResourceId kDefaultUIResourceId = 12;
  static const gfx::Size kDefaultUIResourceImageBounds;
  static const gfx::PointF kDefaultUIResourceUVTopLeft;
  static const gfx::PointF kDefaultUIResourceUVBottomRight;

  // Default NinePatchLayer property values
  static constexpr cc::UIResourceId kDefaultNinePatchUIResourceId = 23;
  static const gfx::Size kDefaultNinePatchImageBounds;
  static const gfx::PointF kDefaultNinePatchUVTopLeft;
  static const gfx::PointF kDefaultNinePatchUVBottomRight;

  static const gfx::Rect kDefaultNinePatchAperture;
  static const gfx::Rect kDefaultNinePatchBorder;
  static const gfx::Rect kDefaultNinePatchLayerOcclusion;
  static constexpr bool kDefaultNinePatchFillCenter = true;

  LayerContextImplTest();
  ~LayerContextImplTest() override;

  void SetUp() override;

  void ResetTestState();

  mojom::LayerTreeUpdatePtr CreateDefaultUpdate();

  void AddDefaultPropertyUpdates(mojom::LayerTreeUpdate* update);

  void AddFirstTimeDefaultProperties(mojom::LayerTreeUpdate* update);

  int AddTransformNode(mojom::LayerTreeUpdate* update, int parent);

  int AddClipNode(mojom::LayerTreeUpdate* update, int parent);

  int AddEffectNode(mojom::LayerTreeUpdate* update, int parent);

  int AddScrollNode(mojom::LayerTreeUpdate* update, int parent);

  mojom::ScrollbarLayerBaseExtraPtr CreateDefaultScrollbarBaseExtra();

  mojom::LayerExtraPtr CreateDefaultLayerExtra(cc::mojom::LayerType type);

  // Helper to add a default layer to the update.
  // Returns the ID of the added layer.
  int AddDefaultLayerToUpdate(
      mojom::LayerTreeUpdate* update,
      cc::mojom::LayerType type = cc::mojom::LayerType::kLayer,
      int id = -1);

  // Helper to manually add a layer to an update, bypassing AddDefaultLayer.
  // This is useful for testing specific ID scenarios or invalid properties.
  mojom::LayerPtr CreateManualLayer(
      int id,
      cc::mojom::LayerType type = cc::mojom::LayerType::kLayer,
      const gfx::Size& bounds = kDefaultLayerBounds,
      int transform_idx = cc::kSecondaryRootPropertyNodeId,
      int clip_idx = cc::kRootPropertyNodeId,
      int effect_idx = cc::kSecondaryRootPropertyNodeId,
      int scroll_idx = cc::kSecondaryRootPropertyNodeId);

  void RemoveLayerInUpdate(mojom::LayerTreeUpdate* update, int id);

  TransferableResource MakeFakeResource(gfx::Size size);

  mojom::TransferableUIResourceRequestPtr CreateUIResourceRequest(
      int uid,
      mojom::TransferableUIResourceRequest::Type type);

 protected:
  cc::LayerImpl* GetLayerFromActiveTree(int layer_id);
  void RecreateLayerContextImplWithSettings(
      mojom::LayerContextSettingsPtr settings);

  FakeCompositorFrameSinkClient dummy_client_;
  FrameSinkManagerImpl frame_sink_manager_;

  std::unique_ptr<CompositorFrameSinkSupport> compositor_frame_sink_support_;
  std::unique_ptr<LayerContextImpl> layer_context_impl_;
  bool first_update_ = true;
  // Layer IDs start at 1, as 0 is reserved for cc::kInvalidLayerId.
  int next_layer_id_ = 1;
  // Property tree IDs start at 0.
  int next_transform_id_ = 0;
  int next_clip_id_ = 0;
  int next_effect_id_ = 0;
  int next_scroll_id_ = 0;
  cc::ViewportPropertyIds viewport_property_ids;
  std::vector<int> layer_order_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_LAYERS_LAYER_CONTEXT_IMPL_BASE_UNITTEST_H_
