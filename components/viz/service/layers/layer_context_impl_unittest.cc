// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/layers/layer_context_impl.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "cc/layers/mirror_layer_impl.h"
#include "cc/layers/nine_patch_layer_impl.h"
#include "cc/layers/nine_patch_thumb_scrollbar_layer_impl.h"
#include "cc/layers/painted_scrollbar_layer_impl.h"
#include "cc/layers/solid_color_scrollbar_layer_impl.h"
#include "cc/layers/surface_layer_impl.h"
#include "cc/layers/texture_layer_impl.h"
#include "cc/layers/ui_resource_layer_impl.h"
#include "cc/layers/view_transition_content_layer_impl.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/property_tree.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/layers/layer_context_impl_base_unittest.h"
#include "components/viz/test/fake_compositor_frame_sink_client.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"
#include "services/viz/public/mojom/compositing/layer_context.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace viz {
namespace {

class LayerContextImplUpdateDisplayTreeUIResourceRequestTest
    : public LayerContextImplTest,
      public ::testing::WithParamInterface<std::tuple<gfx::Size, bool>> {};

TEST_P(LayerContextImplUpdateDisplayTreeUIResourceRequestTest, ResourceSize) {
  const gfx::Size resource_size = std::get<0>(GetParam());
  const bool is_valid = std::get<1>(GetParam());

  auto update = CreateDefaultUpdate();
  auto request = mojom::TransferableUIResourceRequest::New();
  request->type = mojom::TransferableUIResourceRequest::Type::kCreate;
  request->uid = 42;
  request->transferable_resource = MakeFakeResource(resource_size);
  update->ui_resource_requests.push_back(std::move(request));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));

  if (is_valid) {
    EXPECT_TRUE(result.has_value());
    EXPECT_NE(layer_context_impl_->host_impl()->ResourceIdForUIResource(
                  /*uid=*/42),
              kInvalidResourceId);
  } else {
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(),
              "Invalid dimensions for transferable UI resource.");
  }
}

INSTANTIATE_TEST_SUITE_P(
    UIResourceRequestDimensions,
    LayerContextImplUpdateDisplayTreeUIResourceRequestTest,
    ::testing::Values(std::make_tuple(gfx::Size(10, 10), true),
                      std::make_tuple(gfx::Size(0, 10), false),
                      std::make_tuple(gfx::Size(10, 0), false)),
    [](const testing::TestParamInfo<
        LayerContextImplUpdateDisplayTreeUIResourceRequestTest::ParamType>&
           info) {
      std::stringstream name;
      name << (std::get<1>(info.param) ? "Valid" : "Invalid") << "_"
           << info.index;
      return name.str();
    });

TEST_F(LayerContextImplUpdateDisplayTreeUIResourceRequestTest,
       CreateWithNullTransferableResourceFails) {
  auto update = CreateDefaultUpdate();
  auto request = mojom::TransferableUIResourceRequest::New();
  request->type = mojom::TransferableUIResourceRequest::Type::kCreate;
  request->uid = 42;
  request->transferable_resource = std::nullopt;  // Explicitly null
  update->ui_resource_requests.push_back(std::move(request));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            "Invalid transferable resource in UI resource creation");
}

TEST_F(LayerContextImplUpdateDisplayTreeUIResourceRequestTest,
       CreateWithEmptyTransferableResourceFails) {
  auto update = CreateDefaultUpdate();
  auto request = mojom::TransferableUIResourceRequest::New();
  request->type = mojom::TransferableUIResourceRequest::Type::kCreate;
  request->uid = 43;

  // A default-constructed TransferableResource has id = kInvalidResourceId,
  // making it empty.
  request->transferable_resource = TransferableResource();
  ASSERT_TRUE(request->transferable_resource->is_empty());

  update->ui_resource_requests.push_back(std::move(request));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            "Invalid transferable resource in UI resource creation");
}

class LayerContextImplUpdateDisplayTreeTilingTest
    : public LayerContextImplTest,
      public ::testing::WithParamInterface<std::tuple<gfx::Size, bool>> {};

TEST_P(LayerContextImplUpdateDisplayTreeTilingTest, TileSize) {
  const gfx::Size tile_size = std::get<0>(GetParam());
  const bool is_valid = std::get<1>(GetParam());
  auto update = CreateDefaultUpdate();
  int layer_id =
      AddDefaultLayerToUpdate(update.get(), cc::mojom::LayerType::kTileDisplay);

  auto tiling = mojom::Tiling::New();
  tiling->layer_id = layer_id;
  tiling->scale_key = 1.0f;
  tiling->raster_scale = gfx::Vector2dF(1.0f, 1.0f);
  tiling->tile_size = tile_size;
  tiling->tiling_rect = gfx::Rect(100, 100);

  auto tile = mojom::Tile::New();
  tile->column_index = 0;
  tile->row_index = 0;
  tile->contents = mojom::TileContents::NewSolidColor(SkColors::kRed);
  tiling->tiles.push_back(std::move(tile));

  update->tilings.push_back(std::move(tiling));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));

  if (is_valid) {
    EXPECT_TRUE(result.has_value());
    cc::LayerTreeImpl* active_tree =
        layer_context_impl_->host_impl()->active_tree();
    ASSERT_TRUE(active_tree);
    cc::LayerImpl* layer_impl = active_tree->LayerById(layer_id);
    ASSERT_TRUE(layer_impl);
    ASSERT_EQ(layer_impl->GetLayerType(), cc::mojom::LayerType::kTileDisplay);
    const auto* tile_display_layer =
        static_cast<const cc::TileDisplayLayerImpl*>(layer_impl);
    EXPECT_NE(nullptr,
              tile_display_layer->GetTilingForTesting(/*scale_key=*/1.0));
  } else {
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "Invalid tile_size dimensions in Tiling");
  }
}

INSTANTIATE_TEST_SUITE_P(
    TileSize,
    LayerContextImplUpdateDisplayTreeTilingTest,
    ::testing::Values(std::make_tuple(gfx::Size(10, 10), true),
                      std::make_tuple(gfx::Size(0, 10), false),
                      std::make_tuple(gfx::Size(10, 0), false)),
    [](const testing::TestParamInfo<
        LayerContextImplUpdateDisplayTreeTilingTest::ParamType>& info) {
      std::stringstream name;
      name << (std::get<1>(info.param) ? "Valid" : "Invalid") << "_"
           << info.index;
      return name.str();
    });

TEST_F(LayerContextImplTest, DrawModeIsGpuForwardedViaSettings) {
  auto settings = mojom::LayerContextSettings::New();
  settings->draw_mode_is_gpu = true;
  RecreateLayerContextImplWithSettings(std::move(settings));
  cc::LayerTreeHostImpl* host_impl = layer_context_impl_->host_impl();
  EXPECT_TRUE(host_impl->settings().display_tree_draw_mode_is_gpu);

  settings = mojom::LayerContextSettings::New();
  settings->draw_mode_is_gpu = false;
  RecreateLayerContextImplWithSettings(std::move(settings));
  host_impl = layer_context_impl_->host_impl();
  EXPECT_FALSE(host_impl->settings().display_tree_draw_mode_is_gpu);
}

TEST_F(LayerContextImplTest, EnableEdgeAntiAliasingForwardedViaSettings) {
  auto settings = mojom::LayerContextSettings::New();
  settings->enable_edge_anti_aliasing = true;
  RecreateLayerContextImplWithSettings(std::move(settings));
  cc::LayerTreeHostImpl* host_impl = layer_context_impl_->host_impl();
  EXPECT_TRUE(host_impl->settings().enable_edge_anti_aliasing);

  settings = mojom::LayerContextSettings::New();
  settings->enable_edge_anti_aliasing = false;
  RecreateLayerContextImplWithSettings(std::move(settings));
  host_impl = layer_context_impl_->host_impl();
  EXPECT_FALSE(host_impl->settings().enable_edge_anti_aliasing);
}

TEST_F(LayerContextImplTest, TransferableUIResourceLifecycleAndEdgeCases) {
  cc::LayerTreeHostImpl* host_impl = layer_context_impl_->host_impl();

  // Initial state: No resources.
  EXPECT_EQ(host_impl->ResourceIdForUIResource(1), kInvalidResourceId);
  EXPECT_EQ(host_impl->ResourceIdForUIResource(2), kInvalidResourceId);
  EXPECT_EQ(host_impl->ResourceIdForUIResource(3), kInvalidResourceId);

  // Test Case 1: Create UIResource 1. Verify it exists.
  auto update1 = CreateDefaultUpdate();
  update1->ui_resource_requests.push_back(CreateUIResourceRequest(
      1, mojom::TransferableUIResourceRequest::Type::kCreate));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  EXPECT_NE(host_impl->ResourceIdForUIResource(1), kInvalidResourceId);
  EXPECT_EQ(host_impl->ResourceIdForUIResource(2), kInvalidResourceId);

  // Test Case 2: Create UIResource 2. Verify both 1 and 2 exist.
  auto update2 = CreateDefaultUpdate();
  update2->ui_resource_requests.push_back(CreateUIResourceRequest(
      2, mojom::TransferableUIResourceRequest::Type::kCreate));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_NE(host_impl->ResourceIdForUIResource(1), kInvalidResourceId);
  EXPECT_NE(host_impl->ResourceIdForUIResource(2), kInvalidResourceId);

  // Test Case 3: Remove UIResource 1. Verify 1 is gone, 2 exists.
  auto update3 = CreateDefaultUpdate();
  update3->ui_resource_requests.push_back(CreateUIResourceRequest(
      1, mojom::TransferableUIResourceRequest::Type::kDelete));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  EXPECT_EQ(host_impl->ResourceIdForUIResource(1), kInvalidResourceId);
  EXPECT_NE(host_impl->ResourceIdForUIResource(2), kInvalidResourceId);

  // Test Case 4: Edge Case - Try to remove UIResource 1 again (already
  // removed). Verify no crash and 2 still exists.
  auto update4 = CreateDefaultUpdate();
  update4->ui_resource_requests.push_back(CreateUIResourceRequest(
      1, mojom::TransferableUIResourceRequest::Type::kDelete));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update4)).has_value());
  EXPECT_EQ(host_impl->ResourceIdForUIResource(1), kInvalidResourceId);
  EXPECT_NE(host_impl->ResourceIdForUIResource(2), kInvalidResourceId);

  // Test Case 5: Edge Case - Try to remove UIResource 3 (never created).
  // Verify no crash and 2 still exists.
  auto update5 = CreateDefaultUpdate();
  update5->ui_resource_requests.push_back(CreateUIResourceRequest(
      3, mojom::TransferableUIResourceRequest::Type::kDelete));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update5)).has_value());
  EXPECT_EQ(host_impl->ResourceIdForUIResource(3), kInvalidResourceId);
  EXPECT_NE(host_impl->ResourceIdForUIResource(2), kInvalidResourceId);

  // Test Case 6: Edge Case - Try to create UIResource 2 again (duplicate
  // create). Verify 2 still exists (it's replaced).
  ResourceId old_resource_2_id = host_impl->ResourceIdForUIResource(2);
  auto update6 = CreateDefaultUpdate();
  update6->ui_resource_requests.push_back(CreateUIResourceRequest(
      2, mojom::TransferableUIResourceRequest::Type::kCreate));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update6)).has_value());
  EXPECT_NE(host_impl->ResourceIdForUIResource(2), kInvalidResourceId);
  // The resource ID might change upon re-creation.
  EXPECT_NE(host_impl->ResourceIdForUIResource(2), old_resource_2_id);

  // Test Case 7: Remove UIResource 2. Verify it's gone.
  auto update7 = CreateDefaultUpdate();
  update7->ui_resource_requests.push_back(CreateUIResourceRequest(
      2, mojom::TransferableUIResourceRequest::Type::kDelete));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update7)).has_value());
  EXPECT_EQ(host_impl->ResourceIdForUIResource(2), kInvalidResourceId);

  // Test Case 8: Operations within the same update.
  // Create UID 10, then Delete UID 10. Should result in no resource 10.
  // Delete UID 11 (non-existent), then Create UID 11. Should result in
  // resource 11. Create UID 12, then Create UID 12 again. Should result in
  // resource 12.
  auto update8 = CreateDefaultUpdate();
  update8->ui_resource_requests.push_back(CreateUIResourceRequest(
      10, mojom::TransferableUIResourceRequest::Type::kCreate));
  update8->ui_resource_requests.push_back(CreateUIResourceRequest(
      10, mojom::TransferableUIResourceRequest::Type::kDelete));
  update8->ui_resource_requests.push_back(CreateUIResourceRequest(
      11, mojom::TransferableUIResourceRequest::Type::kDelete));
  update8->ui_resource_requests.push_back(CreateUIResourceRequest(
      11, mojom::TransferableUIResourceRequest::Type::kCreate));
  update8->ui_resource_requests.push_back(CreateUIResourceRequest(
      12, mojom::TransferableUIResourceRequest::Type::kCreate));
  update8->ui_resource_requests.push_back(CreateUIResourceRequest(
      12, mojom::TransferableUIResourceRequest::Type::kCreate));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update8)).has_value());
  EXPECT_EQ(host_impl->ResourceIdForUIResource(10), kInvalidResourceId);
  EXPECT_NE(host_impl->ResourceIdForUIResource(11), kInvalidResourceId);
  EXPECT_NE(host_impl->ResourceIdForUIResource(12), kInvalidResourceId);

  // Cleanup remaining resources
  auto update_cleanup = CreateDefaultUpdate();
  update_cleanup->ui_resource_requests.push_back(CreateUIResourceRequest(
      11, mojom::TransferableUIResourceRequest::Type::kDelete));
  update_cleanup->ui_resource_requests.push_back(CreateUIResourceRequest(
      12, mojom::TransferableUIResourceRequest::Type::kDelete));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update_cleanup))
          .has_value());
  EXPECT_EQ(host_impl->ResourceIdForUIResource(11), kInvalidResourceId);
  EXPECT_EQ(host_impl->ResourceIdForUIResource(12), kInvalidResourceId);
}

class LayerContextImplLayerLifecycleTest : public LayerContextImplTest {
 public:
  static std::string GetLayerImplName(cc::mojom::LayerType type) {
    switch (type) {
      case cc::mojom::LayerType::kLayer:
        return "LayerImpl";
      case cc::mojom::LayerType::kMirror:
        return "MirrorLayerImpl";
      case cc::mojom::LayerType::kNinePatch:
        return "NinePatchLayerImpl";
      case cc::mojom::LayerType::kNinePatchThumbScrollbar:
        return "NinePatchThumbScrollbarLayerImpl";
      case cc::mojom::LayerType::kPaintedScrollbar:
        return "PaintedScrollbarLayerImpl";
      case cc::mojom::LayerType::kSolidColor:
        return "SolidColorLayerImpl";
      case cc::mojom::LayerType::kSolidColorScrollbar:
        return "SolidColorScrollbarLayerImpl";
      case cc::mojom::LayerType::kSurface:
        return "SurfaceLayerImpl";
      case cc::mojom::LayerType::kTexture:
        return "TextureLayerImpl";
      case cc::mojom::LayerType::kUIResource:
        return "UIResourceLayerImpl";
      case cc::mojom::LayerType::kTileDisplay:
        return "TileDisplayLayerImpl";
      case cc::mojom::LayerType::kViewTransitionContent:
        return "ViewTransitionContentLayerImpl";
      default:
        return "UnknownLayerType";
    }
  }

 protected:
  void VerifyLayerExists(int layer_id, bool should_exist) {
    if (should_exist) {
      EXPECT_NE(nullptr, GetLayerFromActiveTree(layer_id))
          << "Layer " << layer_id << " should exist.";
    } else {
      EXPECT_EQ(nullptr, GetLayerFromActiveTree(layer_id))
          << "Layer " << layer_id << " should not exist.";
    }
  }

  void VerifyLayerBounds(int layer_id, const gfx::Size& expected_bounds) {
    cc::LayerImpl* layer = GetLayerFromActiveTree(layer_id);
    ASSERT_NE(nullptr, layer) << "Layer " << layer_id << " not found.";
    EXPECT_EQ(expected_bounds, layer->bounds());
  }

  void VerifyLayerOrder(const std::vector<int>& expected_order) {
    cc::LayerTreeImpl* active_tree =
        layer_context_impl_->host_impl()->active_tree();
    ASSERT_EQ(expected_order.size(), active_tree->NumLayers());
    size_t i = 0;
    for (cc::LayerImpl* layer : *active_tree) {
      ASSERT_LT(i, expected_order.size());
      EXPECT_EQ(expected_order[i], layer->id()) << "Mismatch at index " << i;
      i++;
    }
  }
};

class LayerContextImplUpdateDisplayTreeUIResourceLayerTest
    : public LayerContextImplLayerLifecycleTest {
 protected:
  cc::UIResourceLayerImpl* GetUIResourceLayerFromActiveTree(int layer_id) {
    cc::LayerImpl* layer = GetLayerFromActiveTree(layer_id);
    if (layer && layer->GetLayerType() == cc::mojom::LayerType::kUIResource) {
      return static_cast<cc::UIResourceLayerImpl*>(layer);
    }
    return nullptr;
  }
};

TEST_F(LayerContextImplUpdateDisplayTreeUIResourceLayerTest,
       CreateAndUpdateUIResourceLayer) {
  const cc::UIResourceId kUpdatedUIResourceId = 321;
  const gfx::Size kUpdatedUIResourceImageBounds(50, 100);
  const gfx::PointF kUpdatedUIResourceUVTopLeft = gfx::PointF(0.1f, 0.2f);
  const gfx::PointF kUpdatedUIResourceUVBottomRight = gfx::PointF(0.9f, 0.8f);

  // 1. Create the layer with default properties.
  auto update = CreateDefaultUpdate();
  int layer_id =
      AddDefaultLayerToUpdate(update.get(), cc::mojom::LayerType::kUIResource);

  auto ui_resource_extra = mojom::UIResourceLayerExtra::New();
  ui_resource_extra->ui_resource_id = kDefaultUIResourceId;
  ui_resource_extra->image_bounds = kDefaultUIResourceImageBounds;
  ui_resource_extra->uv_top_left = kDefaultUIResourceUVTopLeft;
  ui_resource_extra->uv_bottom_right = kDefaultUIResourceUVBottomRight;
  update->layers.back()->layer_extra =
      mojom::LayerExtra::NewUiResourceLayerExtra(std::move(ui_resource_extra));

  auto result1 = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_TRUE(result1.has_value());

  cc::UIResourceLayerImpl* layer_impl =
      GetUIResourceLayerFromActiveTree(layer_id);
  ASSERT_TRUE(layer_impl);

  // Verify initial default properties.
  EXPECT_EQ(layer_impl->ui_resource_id(), kDefaultUIResourceId);
  EXPECT_EQ(layer_impl->image_bounds(), kDefaultUIResourceImageBounds);
  EXPECT_EQ(layer_impl->uv_top_left(), kDefaultUIResourceUVTopLeft);
  EXPECT_EQ(layer_impl->uv_bottom_right(), kDefaultUIResourceUVBottomRight);

  // 2. Update some properties of the layer.
  auto update2 = CreateDefaultUpdate();
  auto ui_resource_extra2 = mojom::UIResourceLayerExtra::New();
  ui_resource_extra2->ui_resource_id = kUpdatedUIResourceId;
  ui_resource_extra2->image_bounds = kUpdatedUIResourceImageBounds;
  ui_resource_extra2->uv_top_left = kUpdatedUIResourceUVTopLeft;
  ui_resource_extra2->uv_bottom_right = kUpdatedUIResourceUVBottomRight;

  auto layer_update2 =
      CreateManualLayer(layer_id, cc::mojom::LayerType::kUIResource);
  layer_update2->layer_extra =
      mojom::LayerExtra::NewUiResourceLayerExtra(std::move(ui_resource_extra2));
  update2->layers.push_back(std::move(layer_update2));

  auto result2 = layer_context_impl_->DoUpdateDisplayTree(std::move(update2));
  ASSERT_TRUE(result2.has_value());

  EXPECT_EQ(layer_impl->ui_resource_id(), kUpdatedUIResourceId);
  EXPECT_EQ(layer_impl->image_bounds(), kUpdatedUIResourceImageBounds);
  EXPECT_EQ(layer_impl->uv_top_left(), kUpdatedUIResourceUVTopLeft);
  EXPECT_EQ(layer_impl->uv_bottom_right(), kUpdatedUIResourceUVBottomRight);
}

TEST_F(LayerContextImplUpdateDisplayTreeUIResourceLayerTest,
       UpdateUIResourceLayerWithInvalidIdFails) {
  constexpr int kLayerId = 2;
  const cc::UIResourceId kValidUIResourceId = kDefaultUIResourceId;
  const cc::UIResourceId kInvalidUIResourceId = 0;

  // Initial update: Create UIResourceLayer with a valid resource ID.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), cc::mojom::LayerType::kUIResource,
                          kLayerId);
  auto ui_resource_extra1 = mojom::UIResourceLayerExtra::New();
  ui_resource_extra1->ui_resource_id = kValidUIResourceId;
  ui_resource_extra1->image_bounds = kDefaultUIResourceImageBounds;
  update1->layers.back()->layer_extra =
      mojom::LayerExtra::NewUiResourceLayerExtra(std::move(ui_resource_extra1));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());

  cc::UIResourceLayerImpl* ui_resource_layer_impl =
      static_cast<cc::UIResourceLayerImpl*>(GetLayerFromActiveTree(kLayerId));
  ASSERT_NE(nullptr, ui_resource_layer_impl);
  EXPECT_EQ(ui_resource_layer_impl->ui_resource_id(), kValidUIResourceId);

  // Second update: Attempt to update with an invalid resource ID.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 =
      CreateManualLayer(kLayerId, cc::mojom::LayerType::kUIResource);
  auto ui_resource_extra2 = mojom::UIResourceLayerExtra::New();
  ui_resource_extra2->ui_resource_id = kInvalidUIResourceId;  // Invalid ID
  layer_props2->layer_extra =
      mojom::LayerExtra::NewUiResourceLayerExtra(std::move(ui_resource_extra2));
  update2->layers.push_back(std::move(layer_props2));

  auto result2 = layer_context_impl_->DoUpdateDisplayTree(std::move(update2));
  ASSERT_FALSE(result2.has_value());
  EXPECT_EQ(result2.error(), "Invalid ui_resource_id for UIResourceLayerImpl");
  EXPECT_EQ(ui_resource_layer_impl->ui_resource_id(), kValidUIResourceId);
}

class LayerContextImplUpdateDisplayTreeNinePatchLayerTest
    : public LayerContextImplLayerLifecycleTest {
 protected:
  cc::NinePatchLayerImpl* GetNinePatchLayerFromActiveTree(int layer_id) {
    cc::LayerImpl* layer = GetLayerFromActiveTree(layer_id);
    if (layer && layer->GetLayerType() == cc::mojom::LayerType::kNinePatch) {
      return static_cast<cc::NinePatchLayerImpl*>(layer);
    }
    return nullptr;
  }
};

TEST_F(LayerContextImplUpdateDisplayTreeNinePatchLayerTest,
       CreateAndUpdateNinePatchLayer) {
  auto update = CreateDefaultUpdate();
  const gfx::Rect kUpdatedNinePatchAperture(11, 12, 13, 14);
  const gfx::Rect kUpdatedNinePatchBorder(15, 16, 17, 18);
  const gfx::Rect kUpdatedNinePatchLayerOcclusion(19, 20, 21, 22);
  const bool kUpdatedNinePatchFillCenter = true;
  const cc::UIResourceId kUpdatedNinePatchUIResourceId = 456;
  const gfx::Size kUpdatedNinePatchImageBounds(300, 400);
  const gfx::PointF kUpdatedNinePatchUVTopLeft(0.1f, 0.2f);
  const gfx::PointF kUpdatedNinePatchUVBottomRight(0.8f, 0.9f);
  int layer_id =
      AddDefaultLayerToUpdate(update.get(), cc::mojom::LayerType::kNinePatch);

  auto nine_patch_extra = mojom::NinePatchLayerExtra::New();
  nine_patch_extra->image_aperture = kDefaultNinePatchAperture;
  nine_patch_extra->border = kDefaultNinePatchBorder;
  nine_patch_extra->layer_occlusion = kDefaultNinePatchLayerOcclusion;
  nine_patch_extra->fill_center = kDefaultNinePatchFillCenter;
  nine_patch_extra->ui_resource_id = kDefaultNinePatchUIResourceId;
  nine_patch_extra->image_bounds = kDefaultNinePatchImageBounds;
  nine_patch_extra->uv_top_left = kDefaultNinePatchUVTopLeft;
  nine_patch_extra->uv_bottom_right = kDefaultNinePatchUVBottomRight;
  update->layers.back()->layer_extra =
      mojom::LayerExtra::NewNinePatchLayerExtra(std::move(nine_patch_extra));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_TRUE(result.has_value());

  cc::NinePatchLayerImpl* nine_patch_layer =
      GetNinePatchLayerFromActiveTree(layer_id);
  ASSERT_TRUE(nine_patch_layer);

  EXPECT_EQ(nine_patch_layer->quad_generator().image_aperture(),
            kDefaultNinePatchAperture);
  EXPECT_EQ(nine_patch_layer->quad_generator().border(),
            kDefaultNinePatchBorder);
  EXPECT_EQ(nine_patch_layer->quad_generator().output_occlusion(),
            kDefaultNinePatchLayerOcclusion);
  EXPECT_EQ(nine_patch_layer->quad_generator().fill_center(),
            kDefaultNinePatchFillCenter);
  EXPECT_EQ(nine_patch_layer->ui_resource_id(), kDefaultNinePatchUIResourceId);
  EXPECT_EQ(nine_patch_layer->image_bounds(), kDefaultNinePatchImageBounds);
  EXPECT_EQ(nine_patch_layer->uv_top_left(), kDefaultNinePatchUVTopLeft);
  EXPECT_EQ(nine_patch_layer->uv_bottom_right(),
            kDefaultNinePatchUVBottomRight);

  // Update all NinePatchLayerExtra properties.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 =
      CreateManualLayer(layer_id, cc::mojom::LayerType::kNinePatch);
  auto nine_patch_extra2 = mojom::NinePatchLayerExtra::New();
  nine_patch_extra2->image_aperture = kUpdatedNinePatchAperture;
  nine_patch_extra2->border = kUpdatedNinePatchBorder;
  nine_patch_extra2->layer_occlusion = kUpdatedNinePatchLayerOcclusion;
  nine_patch_extra2->fill_center = kUpdatedNinePatchFillCenter;
  nine_patch_extra2->ui_resource_id = kUpdatedNinePatchUIResourceId;
  nine_patch_extra2->image_bounds = kUpdatedNinePatchImageBounds;
  nine_patch_extra2->uv_top_left = kUpdatedNinePatchUVTopLeft;
  nine_patch_extra2->uv_bottom_right = kUpdatedNinePatchUVBottomRight;
  layer_props2->layer_extra =
      mojom::LayerExtra::NewNinePatchLayerExtra(std::move(nine_patch_extra2));
  update2->layers.push_back(std::move(layer_props2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());

  EXPECT_EQ(nine_patch_layer->quad_generator().image_aperture(),
            kUpdatedNinePatchAperture);
  EXPECT_EQ(nine_patch_layer->quad_generator().border(),
            kUpdatedNinePatchBorder);
  EXPECT_EQ(nine_patch_layer->quad_generator().output_occlusion(),
            kUpdatedNinePatchLayerOcclusion);
  EXPECT_EQ(nine_patch_layer->quad_generator().fill_center(),
            kUpdatedNinePatchFillCenter);
  EXPECT_EQ(nine_patch_layer->ui_resource_id(), kUpdatedNinePatchUIResourceId);
  EXPECT_EQ(nine_patch_layer->image_bounds(), kUpdatedNinePatchImageBounds);
  EXPECT_EQ(nine_patch_layer->uv_top_left(), kUpdatedNinePatchUVTopLeft);
  EXPECT_EQ(nine_patch_layer->uv_bottom_right(),
            kUpdatedNinePatchUVBottomRight);
}

TEST_F(LayerContextImplUpdateDisplayTreeNinePatchLayerTest,
       UpdateNinePatchLayerWithInvalidUIResourceIdFails) {
  constexpr int kLayerId = 2;
  const cc::UIResourceId kValidUIResourceId = kDefaultNinePatchUIResourceId;
  const cc::UIResourceId kInvalidUIResourceId = 0;

  // Initial update: Create NinePatchLayer with a valid resource ID.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), cc::mojom::LayerType::kNinePatch,
                          kLayerId);
  auto nine_patch_extra1 = mojom::NinePatchLayerExtra::New();
  nine_patch_extra1->ui_resource_id = kValidUIResourceId;
  // Set other required fields for a valid NinePatchLayer
  nine_patch_extra1->image_aperture = kDefaultNinePatchAperture;
  nine_patch_extra1->border = kDefaultNinePatchBorder;
  update1->layers.back()->layer_extra =
      mojom::LayerExtra::NewNinePatchLayerExtra(std::move(nine_patch_extra1));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());

  cc::NinePatchLayerImpl* nine_patch_layer_impl =
      GetNinePatchLayerFromActiveTree(kLayerId);
  ASSERT_NE(nullptr, nine_patch_layer_impl);
  EXPECT_EQ(nine_patch_layer_impl->ui_resource_id(), kValidUIResourceId);

  // Second update: Attempt to update with an invalid resource ID.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 =
      CreateManualLayer(kLayerId, cc::mojom::LayerType::kNinePatch);
  auto nine_patch_extra2 = mojom::NinePatchLayerExtra::New();
  nine_patch_extra2->ui_resource_id = kInvalidUIResourceId;  // Invalid ID
  layer_props2->layer_extra =
      mojom::LayerExtra::NewNinePatchLayerExtra(std::move(nine_patch_extra2));
  update2->layers.push_back(std::move(layer_props2));

  auto result2 = layer_context_impl_->DoUpdateDisplayTree(std::move(update2));
  ASSERT_FALSE(result2.has_value());
  EXPECT_EQ(result2.error(), "Invalid ui_resource_id for NinePatchLayerImpl");
  EXPECT_EQ(nine_patch_layer_impl->ui_resource_id(), kValidUIResourceId);
}

TEST_F(LayerContextImplLayerLifecycleTest, LayerLifecycleAndEdgeCases) {
  constexpr int kLayerId1 = 2;  // Start after default root layer (ID 1).
  constexpr int kLayerId2 = 3;
  constexpr int kLayerId3 = 4;
  constexpr int kNonExistentLayerId = 99;

  // Test Case 1: Basic Layer Lifecycle (Create, Update Bounds, Remove)
  // Update 1: Create Layer ID kLayerId1.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), cc::mojom::LayerType::kLayer,
                          kLayerId1);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  VerifyLayerExists(kLayerId1, true);
  VerifyLayerBounds(kLayerId1, kDefaultLayerBounds);  // Default bounds

  // Update 2: Update bounds of Layer ID kLayerId1.
  auto update2 = CreateDefaultUpdate();
  const gfx::Size kUpdatedBounds1(20, 20);
  update2->layers.push_back(CreateManualLayer(
      kLayerId1, cc::mojom::LayerType::kLayer, kUpdatedBounds1));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  VerifyLayerExists(kLayerId1, true);
  VerifyLayerBounds(kLayerId1, kUpdatedBounds1);

  // Update 3: Remove Layer ID kLayerId1.
  auto update3 = CreateDefaultUpdate();
  RemoveLayerInUpdate(update3.get(), kLayerId1);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  VerifyLayerExists(kLayerId1, false);

  // Test Case 2: Multiple Layers and Interleaved Operations
  // Update 4: Re-Create Layer ID kLayerId1.
  auto update4 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update4.get(), cc::mojom::LayerType::kLayer,
                          kLayerId1);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update4)).has_value());
  VerifyLayerExists(kLayerId1, true);
  VerifyLayerOrder({1, kLayerId1});

  // Update 5: Create Layer ID kLayerId2.
  auto update5 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update5.get(), cc::mojom::LayerType::kLayer,
                          kLayerId2);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update5)).has_value());
  VerifyLayerExists(kLayerId1, true);
  VerifyLayerExists(kLayerId2, true);
  VerifyLayerOrder({1, kLayerId1, kLayerId2});

  // Update 6: Update Layer ID kLayerId1.
  auto update6 = CreateDefaultUpdate();
  const gfx::Size kUpdatedBounds2(30, 30);
  update6->layers.push_back(CreateManualLayer(
      kLayerId1, cc::mojom::LayerType::kLayer, kUpdatedBounds2));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update6)).has_value());
  VerifyLayerBounds(kLayerId1, kUpdatedBounds2);
  VerifyLayerBounds(kLayerId2, kDefaultLayerBounds);  // Unaffected

  // Update 7: Remove Layer ID kLayerId1.
  auto update7 = CreateDefaultUpdate();
  RemoveLayerInUpdate(update7.get(), kLayerId1);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update7)).has_value());
  VerifyLayerExists(kLayerId1, false);
  VerifyLayerExists(kLayerId2, true);
  VerifyLayerOrder({1, kLayerId2});

  // Update 8: Remove Layer ID kLayerId2.
  auto update8 = CreateDefaultUpdate();
  RemoveLayerInUpdate(update8.get(), kLayerId2);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update8)).has_value());
  VerifyLayerExists(kLayerId2, false);
  VerifyLayerOrder({1});

  // Test Case 3: Updating a Never Existing Layer should fail
  // Update 9: Create kLayerId1 again.
  auto update9 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update9.get(), cc::mojom::LayerType::kLayer,
                          kLayerId1);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update9)).has_value());
  VerifyLayerExists(kLayerId1, true);

  // Update 10: Attempt to update kNonExistentLayerId.
  auto update10 = CreateDefaultUpdate();
  const gfx::Size kUpdatedBounds3(5, 5);
  update10->layers.push_back(
      CreateManualLayer(kNonExistentLayerId, cc::mojom::LayerType::kLayer,
                        kUpdatedBounds3));  // Update non-existent
  auto result10 = layer_context_impl_->DoUpdateDisplayTree(std::move(update10));
  ASSERT_FALSE(result10.has_value());
  EXPECT_EQ(result10.error(), "Invalid layer ID");
  VerifyLayerExists(kLayerId1, true);  // Unaffected
  VerifyLayerBounds(
      kLayerId1,
      kDefaultLayerBounds);  // Should be reset to default or last valid
  VerifyLayerExists(kNonExistentLayerId, false);
  VerifyLayerOrder({1, kLayerId1});

  // Test Case 4: Updating on Previously Removed Layer shoulf fail
  // Update 11: Remove kLayerId1.
  auto update11 = CreateDefaultUpdate();
  RemoveLayerInUpdate(update11.get(), kLayerId1);
  EXPECT_TRUE(layer_context_impl_->DoUpdateDisplayTree(std::move(update11))
                  .has_value());
  VerifyLayerExists(kLayerId1, false);

  // Update 12: Attempt to update kLayerId1 (removed).
  auto update12 = CreateDefaultUpdate();
  const gfx::Size kUpdatedBounds4(40, 40);
  update12->layers.push_back(CreateManualLayer(
      kLayerId1, cc::mojom::LayerType::kLayer, kUpdatedBounds4));
  auto result12 = layer_context_impl_->DoUpdateDisplayTree(std::move(update12));
  ASSERT_FALSE(result12.has_value());
  EXPECT_EQ(result12.error(), "Invalid layer ID");

  VerifyLayerExists(kLayerId1, false);  // Should not be re-created

  // Test Case 5: Duplicate or non existent layer IDs in the Layer Order should
  // fail. Update 13: Create kLayerId1 again.
  auto update13 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update13.get(), cc::mojom::LayerType::kLayer,
                          kLayerId1);
  EXPECT_TRUE(layer_context_impl_->DoUpdateDisplayTree(std::move(update13))
                  .has_value());
  VerifyLayerExists(kLayerId1, true);
  VerifyLayerBounds(kLayerId1, kDefaultLayerBounds);
  VerifyLayerOrder({1, kLayerId1});

  // Update 14: Try to add another instance of kLayerId1 with different bounds.
  auto update14 = CreateDefaultUpdate();
  const gfx::Size kUpdatedBounds5(50, 50);
  update14->layers.push_back(CreateManualLayer(
      kLayerId1, cc::mojom::LayerType::kLayer, kUpdatedBounds5));
  update14->layer_order = layer_order_;
  update14->layer_order->push_back(kLayerId1);

  auto result14 = layer_context_impl_->DoUpdateDisplayTree(std::move(update14));
  ASSERT_FALSE(result14.has_value());
  EXPECT_EQ(result14.error(), "Invalid or duplicate layer ID");
  VerifyLayerExists(kLayerId1, true);
  VerifyLayerBounds(kLayerId1, kUpdatedBounds5);  // Layer should be updated
  VerifyLayerOrder({1, kLayerId1});  // Layer Order should not update

  // Update 15: Try to add a Non Existent layer to Layer Order
  auto update15 = CreateDefaultUpdate();
  update15->layer_order = layer_order_;
  update15->layer_order->push_back(kNonExistentLayerId);

  auto result15 = layer_context_impl_->DoUpdateDisplayTree(std::move(update15));
  ASSERT_FALSE(result15.has_value());
  EXPECT_EQ(result15.error(), "Invalid or duplicate layer ID");

  // Test Case 7: Invalid Property Tree Indices on Creation
  // Update 16: Try to send a layer update with a1valid transform node index
  auto update16 = CreateDefaultUpdate();
  update16->layers.push_back(
      CreateManualLayer(kLayerId2, cc::mojom::LayerType::kLayer,
                        kDefaultLayerBounds, /*transform_idx=*/999));
  update16->layer_order = layer_order_;
  update16->layer_order->push_back(kLayerId2);
  EXPECT_FALSE(layer_context_impl_->DoUpdateDisplayTree(std::move(update16))
                   .has_value());
  VerifyLayerExists(kLayerId2, false);  // Should not have been created

  // Test Case 8: Layer Order Manipulation
  // Update 17: Re-Create 1, kLayerId1, kLayerId2. Order [1, kLayerId1,
  // kLayerId2]
  auto update17 = CreateDefaultUpdate();
  layer_order_.clear();
  AddDefaultLayerToUpdate(update17.get(), cc::mojom::LayerType::kLayer, 1);
  AddDefaultLayerToUpdate(update17.get(), cc::mojom::LayerType::kLayer,
                          kLayerId1);
  AddDefaultLayerToUpdate(update17.get(), cc::mojom::LayerType::kLayer,
                          kLayerId2);
  EXPECT_TRUE(layer_context_impl_->DoUpdateDisplayTree(std::move(update17))
                  .has_value());
  VerifyLayerOrder({1, kLayerId1, kLayerId2});

  // Update 18: Change order to [1, kLayerId2, kLayerId1]
  auto update18 = CreateDefaultUpdate();
  layer_order_.clear();
  layer_order_.push_back(1);
  layer_order_.push_back(kLayerId2);
  layer_order_.push_back(kLayerId1);
  update18->layer_order = layer_order_;
  EXPECT_TRUE(layer_context_impl_->DoUpdateDisplayTree(std::move(update18))
                  .has_value());
  VerifyLayerOrder({1, kLayerId2, kLayerId1});

  // Update 19: Create kLayerId3. Order [1, kLayerId2, kLayerId3, kLayerId1]
  auto update19 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update19.get(), cc::mojom::LayerType::kLayer,
                          kLayerId3);  // kLayerId3
  layer_order_.clear();
  layer_order_.push_back(1);
  layer_order_.push_back(kLayerId2);
  layer_order_.push_back(kLayerId3);
  layer_order_.push_back(kLayerId1);
  update19->layer_order = layer_order_;
  EXPECT_TRUE(layer_context_impl_->DoUpdateDisplayTree(std::move(update19))
                  .has_value());
  VerifyLayerOrder({1, kLayerId2, kLayerId3, kLayerId1});

  // Update 20: Remove kLayerId2. Order [1, kLayerId3, kLayerId1]
  auto update20 = CreateDefaultUpdate();
  layer_order_.clear();
  layer_order_.push_back(1);
  layer_order_.push_back(kLayerId3);
  layer_order_.push_back(kLayerId1);
  update20->layer_order = layer_order_;
  EXPECT_TRUE(layer_context_impl_->DoUpdateDisplayTree(std::move(update20))
                  .has_value());
  VerifyLayerExists(kLayerId2, false);
  VerifyLayerOrder({1, kLayerId3, kLayerId1});
}

TEST_F(LayerContextImplLayerLifecycleTest, CreateLayersOfAllTypes) {
  auto update = CreateDefaultUpdate();

  // Test a subset of layer types that have distinct LayerImpl classes or
  // specific handling in CreateLayer.
  const std::vector<cc::mojom::LayerType> types_to_test = {
      cc::mojom::LayerType::kLayer,
      cc::mojom::LayerType::kMirror,
      cc::mojom::LayerType::kNinePatchThumbScrollbar,
      cc::mojom::LayerType::kPaintedScrollbar,
      cc::mojom::LayerType::kTileDisplay,
      cc::mojom::LayerType::kSolidColor,
      cc::mojom::LayerType::kSolidColorScrollbar,
      cc::mojom::LayerType::kSurface,
      cc::mojom::LayerType::kTexture,
      cc::mojom::LayerType::kUIResource,
      cc::mojom::LayerType::kViewTransitionContent,
      // Add other relevant types here.
  };

  std::vector<int> layer_ids;
  for (cc::mojom::LayerType type : types_to_test) {
    int layer_id = next_layer_id_++;
    layer_ids.push_back(layer_id);
    auto layer = CreateManualLayer(layer_id, type);
    switch (type) {
      case cc::mojom::LayerType::kMirror: {
        auto extra = mojom::MirrorLayerExtra::New();
        // Mirroring the root layer (ID 1) by default for simplicity.
        extra->mirrored_layer_id = 1;
        layer->layer_extra =
            mojom::LayerExtra::NewMirrorLayerExtra(std::move(extra));
        break;
      }
      case cc::mojom::LayerType::kNinePatchThumbScrollbar: {
        auto extra = mojom::NinePatchThumbScrollbarLayerExtra::New();
        extra->scrollbar_base_extra = mojom::ScrollbarLayerBaseExtra::New();
        layer->layer_extra =
            mojom::LayerExtra::NewNinePatchThumbScrollbarLayerExtra(
                std::move(extra));
        break;
      }
      case cc::mojom::LayerType::kPaintedScrollbar: {
        auto extra = mojom::PaintedScrollbarLayerExtra::New();
        extra->scrollbar_base_extra = mojom::ScrollbarLayerBaseExtra::New();
        layer->layer_extra =
            mojom::LayerExtra::NewPaintedScrollbarLayerExtra(std::move(extra));
        break;
      }
      case cc::mojom::LayerType::kSolidColorScrollbar: {
        auto extra = mojom::SolidColorScrollbarLayerExtra::New();
        extra->scrollbar_base_extra = mojom::ScrollbarLayerBaseExtra::New();
        layer->layer_extra =
            mojom::LayerExtra::NewSolidColorScrollbarLayerExtra(
                std::move(extra));
        break;
      }
      case cc::mojom::LayerType::kSurface: {
        auto extra = mojom::SurfaceLayerExtra::New();
        extra->surface_range = kDefaultSurfaceRange;
        extra->deadline_in_frames = 0u;
        layer->layer_extra =
            mojom::LayerExtra::NewSurfaceLayerExtra(std::move(extra));
        break;
      }
      case cc::mojom::LayerType::kTexture: {
        auto extra = mojom::TextureLayerExtra::New();
        // TextureLayer can have an optional TransferableResource.
        // For this basic creation test, leaving it null is fine.
        layer->layer_extra =
            mojom::LayerExtra::NewTextureLayerExtra(std::move(extra));
        break;
      }
      case cc::mojom::LayerType::kViewTransitionContent: {
        auto extra = mojom::ViewTransitionContentLayerExtra::New();
        extra->resource_id = ViewTransitionElementResourceId(
            blink::ViewTransitionToken(), 1, false);
        layer->layer_extra =
            mojom::LayerExtra::NewViewTransitionContentLayerExtra(
                std::move(extra));
        break;
      }
      default:
        // No layer_extra needed for other types in this test.
        break;
    }
    update->layers.push_back(std::move(layer));
    layer_order_.push_back(layer_id);
  }
  update->layer_order = layer_order_;

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update)).has_value());

  for (size_t i = 0; i < types_to_test.size(); ++i) {
    VerifyLayerExists(layer_ids[i], true);
    cc::LayerImpl* layer = GetLayerFromActiveTree(layer_ids[i]);
    ASSERT_NE(nullptr, layer);
    EXPECT_EQ(layer->GetLayerType(), types_to_test[i]);
  }
}

TEST_F(LayerContextImplLayerLifecycleTest, UpdateMultipleLayerProperties) {
  const gfx::Size kUpdatedBounds(50, 50);

  auto update = CreateDefaultUpdate();
  int layer_id1 = AddDefaultLayerToUpdate(update.get());
  int layer_id2 = AddDefaultLayerToUpdate(update.get());
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update)).has_value());

  auto update_props = CreateDefaultUpdate();
  auto layer1_props = CreateManualLayer(layer_id1);
  layer1_props->bounds = kUpdatedBounds;
  layer1_props->contents_opaque = true;
  layer1_props->contents_opaque_for_text = true;
  // If contents_opaque is true, safe_opaque_background_color must be opaque.
  layer1_props->safe_opaque_background_color = SkColors::kRed;
  layer1_props->background_color = SkColors::kRed;
  layer1_props->transform_tree_index = cc::kRootPropertyNodeId;
  update_props->layers.push_back(std::move(layer1_props));

  auto layer2_props = CreateManualLayer(layer_id2);
  layer2_props->is_drawable = false;
  layer2_props->clip_tree_index = cc::kSecondaryRootPropertyNodeId;
  layer2_props->effect_tree_index = cc::kRootPropertyNodeId;
  update_props->layers.push_back(std::move(layer2_props));

  EXPECT_TRUE(layer_context_impl_->DoUpdateDisplayTree(std::move(update_props))
                  .has_value());

  cc::LayerImpl* layer1_impl = GetLayerFromActiveTree(layer_id1);
  ASSERT_NE(nullptr, layer1_impl);
  EXPECT_EQ(layer1_impl->bounds(), kUpdatedBounds);
  EXPECT_TRUE(layer1_impl->contents_opaque());
  EXPECT_TRUE(layer1_impl->contents_opaque_for_text());
  EXPECT_EQ(layer1_impl->background_color(), SkColors::kRed);
  EXPECT_EQ(layer1_impl->transform_tree_index(), cc::kRootPropertyNodeId);

  cc::LayerImpl* layer2_impl = GetLayerFromActiveTree(layer_id2);
  ASSERT_NE(nullptr, layer2_impl);
  EXPECT_FALSE(layer2_impl->draws_content());
  EXPECT_EQ(layer2_impl->clip_tree_index(), cc::kSecondaryRootPropertyNodeId);
  EXPECT_EQ(layer2_impl->effect_tree_index(), cc::kRootPropertyNodeId);
}

TEST_F(LayerContextImplLayerLifecycleTest, ReorderLayers) {
  auto update = CreateDefaultUpdate();
  int layer_id1 = AddDefaultLayerToUpdate(update.get());
  int layer_id2 = AddDefaultLayerToUpdate(update.get());
  int layer_id3 = AddDefaultLayerToUpdate(update.get());
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update)).has_value());
  VerifyLayerOrder({1, layer_id1, layer_id2, layer_id3});

  // Move layer_id1 to the end.
  auto update_reorder1 = CreateDefaultUpdate();
  layer_order_ = {1, layer_id2, layer_id3, layer_id1};
  update_reorder1->layer_order = layer_order_;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update_reorder1))
          .has_value());
  VerifyLayerOrder({1, layer_id2, layer_id3, layer_id1});

  // Move layer_id3 to the beginning (after root).
  auto update_reorder2 = CreateDefaultUpdate();
  layer_order_ = {1, layer_id3, layer_id2, layer_id1};
  update_reorder2->layer_order = layer_order_;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update_reorder2))
          .has_value());
  VerifyLayerOrder({1, layer_id3, layer_id2, layer_id1});
}

TEST_F(LayerContextImplLayerLifecycleTest, RemoveLayers) {
  auto update = CreateDefaultUpdate();
  int layer_id1 = AddDefaultLayerToUpdate(update.get());
  int layer_id2 = AddDefaultLayerToUpdate(update.get());
  int layer_id3 = AddDefaultLayerToUpdate(update.get());

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update)).has_value());
  VerifyLayerOrder({1, layer_id1, layer_id2, layer_id3});

  // Remove from the middle.
  auto update_remove1 = CreateDefaultUpdate();
  RemoveLayerInUpdate(update_remove1.get(), layer_id2);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update_remove1))
          .has_value());
  VerifyLayerExists(layer_id2, false);
  VerifyLayerOrder({1, layer_id1, layer_id3});

  // Remove from the beginning (after root).
  auto update_remove2 = CreateDefaultUpdate();
  RemoveLayerInUpdate(update_remove2.get(), layer_id1);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update_remove2))
          .has_value());
  VerifyLayerExists(layer_id1, false);
  VerifyLayerOrder({1, layer_id3});

  // Remove from the end.
  auto update_remove3 = CreateDefaultUpdate();
  RemoveLayerInUpdate(update_remove3.get(), layer_id3);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update_remove3))
          .has_value());
  VerifyLayerExists(layer_id3, false);
  VerifyLayerOrder({1});
}

TEST_F(LayerContextImplLayerLifecycleTest, LayerPropertyChangedFlags) {
  auto update = CreateDefaultUpdate();
  int layer_id = AddDefaultLayerToUpdate(update.get());
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update)).has_value());

  // Test layer_property_changed_not_from_property_trees
  auto update_flag1 = CreateDefaultUpdate();
  auto layer_props1 = CreateManualLayer(layer_id);
  layer_props1->layer_property_changed_not_from_property_trees = true;
  update_flag1->layers.push_back(std::move(layer_props1));
  EXPECT_TRUE(layer_context_impl_->DoUpdateDisplayTree(std::move(update_flag1))
                  .has_value());
  cc::LayerImpl* layer_impl_flag1 = GetLayerFromActiveTree(layer_id);
  ASSERT_NE(nullptr, layer_impl_flag1);
  EXPECT_TRUE(layer_impl_flag1->LayerPropertyChangedNotFromPropertyTrees());

  // Test layer_property_changed_from_property_trees
  auto update_flag2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(layer_id);
  layer_props2->layer_property_changed_from_property_trees = true;
  update_flag2->layers.push_back(std::move(layer_props2));
  EXPECT_TRUE(layer_context_impl_->DoUpdateDisplayTree(std::move(update_flag2))
                  .has_value());
  // This flag is reset after processing, so we can't directly verify it here
  // without more complex state tracking or inspecting internal LayerImpl
  // states that are affected by it. For now, we ensure the update passes.
  // A more thorough test would involve checking if draw properties were
  // actually updated.
  ASSERT_NE(nullptr, GetLayerFromActiveTree(layer_id));
}

TEST_F(LayerContextImplLayerLifecycleTest, RareProperties) {
  auto update = CreateDefaultUpdate();
  int layer_id = AddDefaultLayerToUpdate(update.get());
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update)).has_value());

  const auto kFirstId = RegionCaptureCropId::CreateRandom();
  const auto kSecondId = RegionCaptureCropId::CreateRandom();
  const RegionCaptureBounds kLayerBounds{
      {{kFirstId, gfx::Rect{0, 0, 250, 250}}, {kSecondId, gfx::Rect{}}}};

  auto update_rare = CreateDefaultUpdate();
  auto layer_props = CreateManualLayer(layer_id);
  layer_props->rare_properties = mojom::RareProperties::New();
  layer_props->rare_properties->filter_quality =
      cc::PaintFlags::FilterQuality::kMedium;
  layer_props->rare_properties->dynamic_range_limit =
      cc::PaintFlags::DynamicRangeLimitMixture(1.f, 0.5f);
  layer_props->rare_properties->capture_bounds = kLayerBounds;
  update_rare->layers.push_back(std::move(layer_props));

  EXPECT_TRUE(layer_context_impl_->DoUpdateDisplayTree(std::move(update_rare))
                  .has_value());

  cc::LayerImpl* layer_impl_rare = GetLayerFromActiveTree(layer_id);
  ASSERT_NE(nullptr, layer_impl_rare);
  EXPECT_EQ(layer_impl_rare->GetFilterQuality(),
            cc::PaintFlags::FilterQuality::kMedium);
  EXPECT_EQ(layer_impl_rare->GetDynamicRangeLimit(),
            cc::PaintFlags::DynamicRangeLimitMixture(1.f, 0.5f));
  ASSERT_TRUE(layer_impl_rare->capture_bounds());
  EXPECT_EQ(*layer_impl_rare->capture_bounds(), kLayerBounds);
}

TEST_F(LayerContextImplLayerLifecycleTest, ContentsOpaqueFlags) {
  ResetTestState();
  auto update = CreateDefaultUpdate();
  int layer_id = AddDefaultLayerToUpdate(update.get());
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update)).has_value());

  // Valid: contents_opaque = true, contents_opaque_for_text = true
  auto update_valid1 = CreateDefaultUpdate();
  auto layer_props_valid1 = CreateManualLayer(layer_id);
  layer_props_valid1->contents_opaque = true;
  layer_props_valid1->contents_opaque_for_text = true;
  // If contents_opaque is true, safe_opaque_background_color must be opaque.
  layer_props_valid1->safe_opaque_background_color = SkColors::kRed;
  update_valid1->layers.push_back(std::move(layer_props_valid1));
  EXPECT_TRUE(layer_context_impl_->DoUpdateDisplayTree(std::move(update_valid1))
                  .has_value());
  cc::LayerImpl* layer_impl_valid1 = GetLayerFromActiveTree(layer_id);
  ASSERT_NE(nullptr, layer_impl_valid1);
  EXPECT_TRUE(layer_impl_valid1->contents_opaque());
  EXPECT_TRUE(layer_impl_valid1->contents_opaque_for_text());

  // Invalid: contents_opaque = true, contents_opaque_for_text = false
  auto update_invalid = CreateDefaultUpdate();
  auto layer_props_invalid = CreateManualLayer(layer_id);
  layer_props_invalid->contents_opaque = true;
  layer_props_invalid->contents_opaque_for_text = false;
  // This would also be invalid if contents_opaque_for_text was not opaque.
  layer_props_invalid->safe_opaque_background_color = SkColors::kGreen;
  update_invalid->layers.push_back(std::move(layer_props_invalid));
  auto result_invalid =
      layer_context_impl_->DoUpdateDisplayTree(std::move(update_invalid));
  ASSERT_FALSE(result_invalid.has_value());
  EXPECT_EQ(result_invalid.error(),
            "Invalid contents_opaque_for_text: cannot be false if "
            "contents_opaque is true.");
  // Verify properties remain from the last valid update
  cc::LayerImpl* layer_impl_invalid = GetLayerFromActiveTree(layer_id);
  ASSERT_NE(nullptr, layer_impl_invalid);
  EXPECT_TRUE(layer_impl_invalid->contents_opaque());
  EXPECT_TRUE(layer_impl_invalid->contents_opaque_for_text());

  // Valid: contents_opaque = false, contents_opaque_for_text = true
  auto update_valid2 = CreateDefaultUpdate();
  auto layer_props_valid2 = CreateManualLayer(layer_id);
  layer_props_valid2->contents_opaque = false;
  layer_props_valid2->contents_opaque_for_text = true;
  // If contents_opaque is false, safe_opaque_background_color must be
  // transparent.
  layer_props_valid2->safe_opaque_background_color = SkColors::kTransparent;
  update_valid2->layers.push_back(std::move(layer_props_valid2));
  EXPECT_TRUE(layer_context_impl_->DoUpdateDisplayTree(std::move(update_valid2))
                  .has_value());
  cc::LayerImpl* layer_impl_valid2 = GetLayerFromActiveTree(layer_id);
  ASSERT_NE(nullptr, layer_impl_valid2);
  EXPECT_FALSE(layer_impl_valid2->contents_opaque());
  EXPECT_TRUE(layer_impl_valid2->contents_opaque_for_text());

  // Valid: contents_opaque = false, contents_opaque_for_text = false
  auto update_valid3 = CreateDefaultUpdate();
  auto layer_props_valid3 = CreateManualLayer(layer_id);
  layer_props_valid3->contents_opaque = false;
  layer_props_valid3->contents_opaque_for_text = false;
  // If contents_opaque is false, safe_opaque_background_color must be
  // transparent.
  layer_props_valid3->safe_opaque_background_color = SkColors::kTransparent;
  update_valid3->layers.push_back(std::move(layer_props_valid3));
  EXPECT_TRUE(layer_context_impl_->DoUpdateDisplayTree(std::move(update_valid3))
                  .has_value());
  cc::LayerImpl* layer_impl_valid3 = GetLayerFromActiveTree(layer_id);
  ASSERT_NE(nullptr, layer_impl_valid3);
  EXPECT_FALSE(layer_impl_valid3->contents_opaque());
  EXPECT_FALSE(layer_impl_valid3->contents_opaque_for_text());
}

const cc::mojom::LayerType kLayerTypesWithSpecificExtras[] = {
    cc::mojom::LayerType::kMirror,
    cc::mojom::LayerType::kNinePatch,
    cc::mojom::LayerType::kNinePatchThumbScrollbar,
    cc::mojom::LayerType::kPaintedScrollbar,
    cc::mojom::LayerType::kSolidColorScrollbar,
    cc::mojom::LayerType::kSurface,
    cc::mojom::LayerType::kTexture,
    cc::mojom::LayerType::kTileDisplay,
    cc::mojom::LayerType::kUIResource,
    cc::mojom::LayerType::kViewTransitionContent,
};

TEST_F(LayerContextImplLayerLifecycleTest, MissingLayerExtra) {
  for (cc::mojom::LayerType type : kLayerTypesWithSpecificExtras) {
    SCOPED_TRACE(testing::Message()
                 << "Testing LayerType: " << GetLayerImplName(type));
    ResetTestState();
    // Create a valid root layer first.
    auto initial_update = CreateDefaultUpdate();
    EXPECT_TRUE(
        layer_context_impl_->DoUpdateDisplayTree(std::move(initial_update))
            .has_value());

    auto update_missing_extra = CreateDefaultUpdate();
    int layer_id = next_layer_id_++;
    // Create the layer manually without setting layer_extra.
    auto layer = CreateManualLayer(layer_id, type);
    // Ensure layer_extra is indeed null.
    layer->layer_extra = nullptr;

    update_missing_extra->layers.push_back(std::move(layer));
    update_missing_extra->layer_order = layer_order_;
    update_missing_extra->layer_order->push_back(layer_id);

    auto result = layer_context_impl_->DoUpdateDisplayTree(
        std::move(update_missing_extra));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(),
              "Invalid layer_extra type for " + GetLayerImplName(type));
  }
}

class LayerContextImplLayerExtraTypeValidationTest
    : public LayerContextImplLayerLifecycleTest,
      public testing::WithParamInterface<cc::mojom::LayerType> {};

TEST_P(LayerContextImplLayerExtraTypeValidationTest, MismatchedLayerExtra) {
  constexpr int kLayerId = 2;
  cc::mojom::LayerType layer_type_under_test = GetParam();

  for (cc::mojom::LayerType mismatching_extra_provider_type :
       kLayerTypesWithSpecificExtras) {
    if (layer_type_under_test == mismatching_extra_provider_type) {
      continue;
    }

    SCOPED_TRACE(testing::Message()
                 << "LayerTypeUnderTest: "
                 << GetLayerImplName(layer_type_under_test)
                 << ", MismatchingExtraProviderType: "
                 << GetLayerImplName(mismatching_extra_provider_type));

    // Test Creation with mismatched extra
    ResetTestState();
    auto update_create = CreateDefaultUpdate();
    auto layer_props_create =
        CreateManualLayer(kLayerId, layer_type_under_test);
    layer_props_create->layer_extra =
        CreateDefaultLayerExtra(mismatching_extra_provider_type);
    update_create->layers.push_back(std::move(layer_props_create));
    update_create->layer_order = {1, kLayerId};  // Root layer (1) + test layer

    auto result_create =
        layer_context_impl_->DoUpdateDisplayTree(std::move(update_create));
    ASSERT_FALSE(result_create.has_value());
    EXPECT_THAT(result_create.error(),
                testing::StartsWith("Invalid layer_extra type for " +
                                    GetLayerImplName(layer_type_under_test)));

    // Test Update with mismatched extra
    ResetTestState();
    auto initial_update = CreateDefaultUpdate();
    AddDefaultLayerToUpdate(initial_update.get(), layer_type_under_test,
                            kLayerId);
    EXPECT_TRUE(
        layer_context_impl_->DoUpdateDisplayTree(std::move(initial_update))
            .has_value());

    auto update_props = CreateDefaultUpdate();
    auto layer_props_update =
        CreateManualLayer(kLayerId, layer_type_under_test);
    layer_props_update->layer_extra =
        CreateDefaultLayerExtra(mismatching_extra_provider_type);
    update_props->layers.push_back(std::move(layer_props_update));

    auto result_update =
        layer_context_impl_->DoUpdateDisplayTree(std::move(update_props));
    ASSERT_FALSE(result_update.has_value());
    EXPECT_THAT(result_update.error(),
                testing::StartsWith("Invalid layer_extra type for " +
                                    GetLayerImplName(layer_type_under_test)));
  }
}

INSTANTIATE_TEST_SUITE_P(
    AllLayerTypesRequiringExtra,
    LayerContextImplLayerExtraTypeValidationTest,
    testing::ValuesIn(kLayerTypesWithSpecificExtras),
    [](const testing::TestParamInfo<
        LayerContextImplLayerExtraTypeValidationTest::ParamType>& info) {
      return LayerContextImplLayerLifecycleTest::GetLayerImplName(info.param);
    });

TEST_F(LayerContextImplLayerLifecycleTest,
       UpdateExistingLayerWithInvalidPropertyTreeIndicesFails) {
  constexpr int kLayerId = 2;
  constexpr int kValidIndex = 1;     // Assumes root (0) and secondary_root (1)
  constexpr int kInvalidIndex = 99;  // An index that will be out of bounds.

  // Setup: Create a layer with valid indices and small property trees.
  auto setup_update = CreateDefaultUpdate();

  AddDefaultLayerToUpdate(setup_update.get(), cc::mojom::LayerType::kLayer,
                          kLayerId);
  // Set initial valid indices for the layer.
  setup_update->layers.back()->transform_tree_index = kValidIndex;
  setup_update->layers.back()->clip_tree_index = kValidIndex;
  setup_update->layers.back()->effect_tree_index = kValidIndex;
  setup_update->layers.back()->scroll_tree_index = kValidIndex;

  EXPECT_TRUE(layer_context_impl_->DoUpdateDisplayTree(std::move(setup_update))
                  .has_value());
  VerifyLayerExists(kLayerId, true);

  // Test Case 1: Update with invalid transform_tree_index.
  auto update_invalid_transform = CreateDefaultUpdate();
  update_invalid_transform->layers.push_back(CreateManualLayer(
      kLayerId, cc::mojom::LayerType::kLayer, kDefaultLayerBounds,
      kInvalidIndex, kValidIndex, kValidIndex, kValidIndex));
  auto result_transform = layer_context_impl_->DoUpdateDisplayTree(
      std::move(update_invalid_transform));
  ASSERT_FALSE(result_transform.has_value());
  EXPECT_THAT(result_transform.error(),
              testing::StartsWith("Invalid transform tree ID"));

  // Test Case 2: Update with invalid clip_tree_index.
  auto update_invalid_clip = CreateDefaultUpdate();
  update_invalid_clip->layers.push_back(CreateManualLayer(
      kLayerId, cc::mojom::LayerType::kLayer, kDefaultLayerBounds, kValidIndex,
      kInvalidIndex, kValidIndex, kValidIndex));
  auto result_clip =
      layer_context_impl_->DoUpdateDisplayTree(std::move(update_invalid_clip));
  ASSERT_FALSE(result_clip.has_value());
  EXPECT_THAT(result_clip.error(), testing::StartsWith("Invalid clip tree ID"));

  // Test Case 3: Update with invalid effect_tree_index.
  auto update_invalid_effect = CreateDefaultUpdate();
  update_invalid_effect->layers.push_back(CreateManualLayer(
      kLayerId, cc::mojom::LayerType::kLayer, kDefaultLayerBounds, kValidIndex,
      kValidIndex, kInvalidIndex, kValidIndex));
  auto result_effect = layer_context_impl_->DoUpdateDisplayTree(
      std::move(update_invalid_effect));
  ASSERT_FALSE(result_effect.has_value());
  EXPECT_THAT(result_effect.error(),
              testing::StartsWith("Invalid effect tree ID"));

  // Test Case 4: Update with invalid scroll_tree_index.
  auto update_invalid_scroll = CreateDefaultUpdate();
  update_invalid_scroll->layers.push_back(CreateManualLayer(
      kLayerId, cc::mojom::LayerType::kLayer, kDefaultLayerBounds, kValidIndex,
      kValidIndex, kValidIndex, kInvalidIndex));
  auto result_scroll = layer_context_impl_->DoUpdateDisplayTree(
      std::move(update_invalid_scroll));
  ASSERT_FALSE(result_scroll.has_value());
  EXPECT_THAT(result_scroll.error(),
              testing::StartsWith("Invalid scroll tree ID"));

  // Verify layer properties remain from the last successful update.
  cc::LayerImpl* layer_impl_after_invalid = GetLayerFromActiveTree(kLayerId);
  ASSERT_NE(nullptr, layer_impl_after_invalid);
  EXPECT_EQ(layer_impl_after_invalid->transform_tree_index(), kValidIndex);
  EXPECT_EQ(layer_impl_after_invalid->clip_tree_index(), kValidIndex);
  EXPECT_EQ(layer_impl_after_invalid->effect_tree_index(), kValidIndex);
  EXPECT_EQ(layer_impl_after_invalid->scroll_tree_index(), kValidIndex);
}

// Test fixture for base LayerImpl property updates (those directly on
// mojom::Layer).
class LayerContextImplUpdateDisplayTreeBaseLayerPropertiesTest
    : public LayerContextImplLayerLifecycleTest {};

TEST_F(LayerContextImplUpdateDisplayTreeBaseLayerPropertiesTest,
       UpdateLayerWithMismatchedTypeFails) {
  constexpr int kLayerId = 2;
  constexpr cc::mojom::LayerType kInitialType =
      cc::mojom::LayerType::kSolidColor;
  constexpr cc::mojom::LayerType kUpdatedType = cc::mojom::LayerType::kTexture;

  // Initial update: Create a layer of kInitialType.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), kInitialType, kLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());

  cc::LayerImpl* layer_impl = GetLayerFromActiveTree(kLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_EQ(layer_impl->GetLayerType(), kInitialType);

  // Second update: Attempt to update the layer with kUpdatedType.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(kLayerId, kUpdatedType);
  layer_props2->layer_extra = CreateDefaultLayerExtra(kUpdatedType);
  update2->layers.push_back(std::move(layer_props2));

  auto result2 = layer_context_impl_->DoUpdateDisplayTree(std::move(update2));
  ASSERT_FALSE(result2.has_value());
  EXPECT_EQ(result2.error(), "Incorrect layer type used in Layer update.");

  // Verify the layer type in the tree remains kInitialType.
  EXPECT_EQ(layer_impl->GetLayerType(), kInitialType);
}

TEST_F(LayerContextImplUpdateDisplayTreeBaseLayerPropertiesTest,
       UpdateSafeOpaqueBackgroundColor) {
  constexpr int kLayerId = 2;
  const SkColor4f kDefaultColor = SkColors::kTransparent;  // Default from mojom
  const SkColor4f kColor1 = SkColors::kRed;
  const SkColor4f kColor2 = SkColors::kGreen;

  // Initial update: Create with default color.
  // Default contents_opaque is false.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), cc::mojom::LayerType::kLayer,
                          kLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::LayerImpl* layer_impl = GetLayerFromActiveTree(kLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_FALSE(layer_impl->contents_opaque());
  EXPECT_EQ(layer_impl->safe_opaque_background_color(), kDefaultColor);

  // Second update: Update color (still transparent, contents_opaque=false).
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(kLayerId);
  layer_props2->contents_opaque = false;
  layer_props2->contents_opaque_for_text = false;
  layer_props2->safe_opaque_background_color = SkColors::kTransparent;
  update2->layers.push_back(std::move(layer_props2));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(layer_impl->safe_opaque_background_color(), SkColors::kTransparent);

  // Third update: Set contents_opaque = true, safe_opaque_background_color =
  // opaque red.
  auto update3 = CreateDefaultUpdate();
  auto layer_props3 = CreateManualLayer(kLayerId);
  layer_props3->contents_opaque = true;
  layer_props3->contents_opaque_for_text = true;
  layer_props3->safe_opaque_background_color = kColor2;
  update3->layers.push_back(std::move(layer_props3));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  EXPECT_TRUE(layer_impl->contents_opaque());
  EXPECT_EQ(layer_impl->safe_opaque_background_color(), kColor2);

  // Fourth update: Update color again (opaque green, contents_opaque=true).
  auto update4 = CreateDefaultUpdate();
  auto layer_props4 = CreateManualLayer(kLayerId);
  layer_props4->contents_opaque = true;
  layer_props4->contents_opaque_for_text = true;
  layer_props4->safe_opaque_background_color = kColor2;
  update4->layers.push_back(std::move(layer_props4));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update4)).has_value());
  EXPECT_EQ(layer_impl->safe_opaque_background_color(), kColor2);

  // Fifth update: Attempt invalid update: contents_opaque = true, but
  // safe_opaque_background_color is transparent.
  auto update5 = CreateDefaultUpdate();
  auto layer_props5 = CreateManualLayer(kLayerId);
  layer_props5->contents_opaque = true;
  layer_props5->contents_opaque_for_text = true;
  layer_props5->safe_opaque_background_color = SkColors::kTransparent;
  update5->layers.push_back(std::move(layer_props5));
  auto result5 = layer_context_impl_->DoUpdateDisplayTree(std::move(update5));
  ASSERT_FALSE(result5.has_value());
  EXPECT_THAT(result5.error(),
              testing::StartsWith("Invalid safe_opaque_background_color"));
  // Color should remain kColor2 from the last successful update.
  EXPECT_EQ(layer_impl->safe_opaque_background_color(), kColor2);
  EXPECT_TRUE(layer_impl->contents_opaque());

  // Sixth update: Attempt invalid update: contents_opaque = false, but
  // safe_opaque_background_color is opaque (and not transparent).
  auto update6 = CreateDefaultUpdate();
  auto layer_props6 = CreateManualLayer(kLayerId);
  layer_props6->contents_opaque = false;
  layer_props6->contents_opaque_for_text = false;
  layer_props6->safe_opaque_background_color = kColor1;  // Opaque red
  update6->layers.push_back(std::move(layer_props6));
  auto result6 = layer_context_impl_->DoUpdateDisplayTree(std::move(update6));
  ASSERT_FALSE(result6.has_value());
  EXPECT_THAT(result6.error(),
              testing::StartsWith("Invalid safe_opaque_background_color"));
  // Color should remain kColor2 from the last successful update.
  EXPECT_EQ(layer_impl->safe_opaque_background_color(), kColor2);
  EXPECT_TRUE(layer_impl->contents_opaque());

  // Seventh update: Valid update: contents_opaque = false,
  // safe_opaque_background_color is transparent.
  auto update7 = CreateDefaultUpdate();
  auto layer_props7 = CreateManualLayer(kLayerId);
  layer_props7->contents_opaque = false;
  layer_props7->contents_opaque_for_text = false;
  layer_props7->safe_opaque_background_color = SkColors::kTransparent;
  update7->layers.push_back(std::move(layer_props7));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update7)).has_value());
  EXPECT_FALSE(layer_impl->contents_opaque());
  EXPECT_EQ(layer_impl->safe_opaque_background_color(), SkColors::kTransparent);
}

TEST_F(LayerContextImplUpdateDisplayTreeBaseLayerPropertiesTest, UpdateRect) {
  constexpr int kLayerId = 2;
  const gfx::Rect kInitialRect;  // Default from mojom
  const gfx::Rect kRect1(10, 10, 5, 5);
  const gfx::Rect kRect2(0, 0, 12, 12);
  const gfx::Rect kExpectedUnion = gfx::UnionRects(kRect1, kRect2);

  // Initial update: Create with default (empty) update_rect.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), cc::mojom::LayerType::kLayer,
                          kLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::LayerImpl* layer_impl = GetLayerFromActiveTree(kLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_EQ(layer_impl->update_rect(), kInitialRect);

  // Second update: Set update_rect to kRect1.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(kLayerId);
  layer_props2->update_rect = kRect1;
  update2->layers.push_back(std::move(layer_props2));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  // LayerImpl::UnionUpdateRect is called, so it unions with its current value.
  // Since it was empty, it becomes kRect1.
  EXPECT_EQ(layer_impl->update_rect(), kRect1);

  // Third update: Set update_rect to kRect2. It should be unioned.
  auto update3 = CreateDefaultUpdate();
  auto layer_props3 = CreateManualLayer(kLayerId);
  layer_props3->update_rect = kRect2;
  update3->layers.push_back(std::move(layer_props3));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  EXPECT_EQ(layer_impl->update_rect(), kExpectedUnion);
}

TEST_F(LayerContextImplUpdateDisplayTreeBaseLayerPropertiesTest,
       UpdateHitTestOpaqueness) {
  constexpr int kLayerId = 2;
  const cc::HitTestOpaqueness kDefaultValue =
      cc::HitTestOpaqueness::kTransparent;  // Default from mojom
  const cc::HitTestOpaqueness kValue1 = cc::HitTestOpaqueness::kOpaque;
  const cc::HitTestOpaqueness kValue2 = cc::HitTestOpaqueness::kMixed;

  // Initial update: Create with default value.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), cc::mojom::LayerType::kLayer,
                          kLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::LayerImpl* layer_impl = GetLayerFromActiveTree(kLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_EQ(layer_impl->hit_test_opaqueness(), kDefaultValue);

  // Second update: Update value.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(kLayerId);
  layer_props2->hit_test_opaqueness = kValue1;
  update2->layers.push_back(std::move(layer_props2));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(layer_impl->hit_test_opaqueness(), kValue1);

  // Third update: Update value again.
  auto update3 = CreateDefaultUpdate();
  auto layer_props3 = CreateManualLayer(kLayerId);
  layer_props3->hit_test_opaqueness = kValue2;
  update3->layers.push_back(std::move(layer_props3));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  EXPECT_EQ(layer_impl->hit_test_opaqueness(), kValue2);
}

TEST_F(LayerContextImplUpdateDisplayTreeBaseLayerPropertiesTest,
       UpdateElementId) {
  constexpr int kLayerId = 2;
  const cc::ElementId kDefaultValue;  // Default from mojom
  const cc::ElementId kValue1(123);
  const cc::ElementId kValue2(456);

  // Initial update: Create with default value.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), cc::mojom::LayerType::kLayer,
                          kLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::LayerImpl* layer_impl = GetLayerFromActiveTree(kLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_EQ(layer_impl->element_id(), kDefaultValue);

  // Second update: Update value.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(kLayerId);
  layer_props2->element_id = kValue1;
  update2->layers.push_back(std::move(layer_props2));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(layer_impl->element_id(), kValue1);

  // Third update: Update value again.
  auto update3 = CreateDefaultUpdate();
  auto layer_props3 = CreateManualLayer(kLayerId);
  layer_props3->element_id = kValue2;
  update3->layers.push_back(std::move(layer_props3));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  EXPECT_EQ(layer_impl->element_id(), kValue2);
}

TEST_F(LayerContextImplUpdateDisplayTreeBaseLayerPropertiesTest,
       UpdateOffsetToTransformParent) {
  constexpr int kLayerId = 2;
  const gfx::Vector2dF kDefaultValue;  // Default from mojom
  const gfx::Vector2dF kValue1(10.f, 20.f);
  const gfx::Vector2dF kValue2(-5.f, 15.f);

  // Initial update: Create with default value.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), cc::mojom::LayerType::kLayer,
                          kLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::LayerImpl* layer_impl = GetLayerFromActiveTree(kLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_EQ(layer_impl->offset_to_transform_parent(), kDefaultValue);

  // Second update: Update value.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(kLayerId);
  layer_props2->offset_to_transform_parent = kValue1;
  update2->layers.push_back(std::move(layer_props2));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(layer_impl->offset_to_transform_parent(), kValue1);

  // Third update: Update value again.
  auto update3 = CreateDefaultUpdate();
  auto layer_props3 = CreateManualLayer(kLayerId);
  layer_props3->offset_to_transform_parent = kValue2;
  update3->layers.push_back(std::move(layer_props3));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  EXPECT_EQ(layer_impl->offset_to_transform_parent(), kValue2);
}

TEST_F(LayerContextImplUpdateDisplayTreeBaseLayerPropertiesTest,
       UpdateShouldCheckBackfaceVisibility) {
  constexpr int kLayerId = 2;
  constexpr bool kDefaultValue = false;  // Default from mojom
  constexpr bool kValue1 = true;

  // Initial update: Create with default value.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), cc::mojom::LayerType::kLayer,
                          kLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::LayerImpl* layer_impl = GetLayerFromActiveTree(kLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_EQ(layer_impl->should_check_backface_visibility(), kDefaultValue);

  // Second update: Update value to true.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(kLayerId);
  layer_props2->should_check_backface_visibility = kValue1;
  update2->layers.push_back(std::move(layer_props2));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(layer_impl->should_check_backface_visibility(), kValue1);

  // Third update: Update value back to false.
  auto update3 = CreateDefaultUpdate();
  auto layer_props3 = CreateManualLayer(kLayerId);
  layer_props3->should_check_backface_visibility = kDefaultValue;
  update3->layers.push_back(std::move(layer_props3));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  EXPECT_EQ(layer_impl->should_check_backface_visibility(), kDefaultValue);
}

// Test fixture for TileDisplayLayerImpl specific property updates.
class LayerContextImplUpdateDisplayTreeTileDisplayLayerPropertiesTest
    : public LayerContextImplLayerLifecycleTest {};

TEST_F(LayerContextImplUpdateDisplayTreeTileDisplayLayerPropertiesTest,
       UpdateTileDisplayLayerProperties) {
  constexpr int kLayerId = 2;
  const SkColor4f kSolidColor = SkColors::kMagenta;

  // Initial update: Create TileDisplayLayer with default properties.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), cc::mojom::LayerType::kTileDisplay,
                          kLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());

  cc::LayerImpl* layer_impl_base = GetLayerFromActiveTree(kLayerId);
  ASSERT_NE(nullptr, layer_impl_base);
  ASSERT_EQ(layer_impl_base->GetLayerType(),
            cc::mojom::LayerType::kTileDisplay);
  auto* tile_display_layer_impl =
      static_cast<cc::TileDisplayLayerImpl*>(layer_impl_base);

  EXPECT_FALSE(tile_display_layer_impl->solid_color_for_testing().has_value());
  EXPECT_FALSE(tile_display_layer_impl->is_backdrop_filter_mask());

  // Second update: Set solid_color and is_backdrop_filter_mask.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 =
      CreateManualLayer(kLayerId, cc::mojom::LayerType::kTileDisplay);
  auto tile_extra2 = mojom::TileDisplayLayerExtra::New();
  tile_extra2->solid_color = kSolidColor;
  tile_extra2->is_backdrop_filter_mask = true;
  layer_props2->layer_extra =
      mojom::LayerExtra::NewTileDisplayLayerExtra(std::move(tile_extra2));
  update2->layers.push_back(std::move(layer_props2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());

  EXPECT_TRUE(tile_display_layer_impl->solid_color_for_testing().has_value());
  EXPECT_EQ(tile_display_layer_impl->solid_color_for_testing().value(),
            kSolidColor);
  EXPECT_TRUE(tile_display_layer_impl->is_backdrop_filter_mask());

  // Third update: Clear solid_color and set is_backdrop_filter_mask to false.
  auto update3 = CreateDefaultUpdate();
  auto layer_props3 =
      CreateManualLayer(kLayerId, cc::mojom::LayerType::kTileDisplay);
  auto tile_extra3 = mojom::TileDisplayLayerExtra::New();
  tile_extra3->solid_color = std::nullopt;
  tile_extra3->is_backdrop_filter_mask = false;
  layer_props3->layer_extra =
      mojom::LayerExtra::NewTileDisplayLayerExtra(std::move(tile_extra3));
  update3->layers.push_back(std::move(layer_props3));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());

  EXPECT_FALSE(tile_display_layer_impl->solid_color_for_testing().has_value());
  EXPECT_FALSE(tile_display_layer_impl->is_backdrop_filter_mask());
}

TEST_F(LayerContextImplUpdateDisplayTreeTileDisplayLayerPropertiesTest,
       UpdateIsDirectlyCompositedImage) {
  constexpr int kLayerId = 2;

  // Initial update: Create TileDisplayLayer with default properties.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), cc::mojom::LayerType::kTileDisplay,
                          kLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());

  cc::LayerImpl* layer_impl_base = GetLayerFromActiveTree(kLayerId);
  ASSERT_NE(nullptr, layer_impl_base);
  ASSERT_EQ(layer_impl_base->GetLayerType(),
            cc::mojom::LayerType::kTileDisplay);
  auto* tile_display_layer_impl =
      static_cast<cc::TileDisplayLayerImpl*>(layer_impl_base);

  EXPECT_FALSE(tile_display_layer_impl->IsDirectlyCompositedImage());

  // Second update: Set is_directly_composited_image to true.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 =
      CreateManualLayer(kLayerId, cc::mojom::LayerType::kTileDisplay);
  auto tile_extra2 = mojom::TileDisplayLayerExtra::New();
  tile_extra2->is_directly_composited_image = true;
  layer_props2->layer_extra =
      mojom::LayerExtra::NewTileDisplayLayerExtra(std::move(tile_extra2));
  update2->layers.push_back(std::move(layer_props2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());

  EXPECT_TRUE(tile_display_layer_impl->IsDirectlyCompositedImage());

  // Third update: Set is_directly_composited_image to false.
  auto update3 = CreateDefaultUpdate();
  auto layer_props3 =
      CreateManualLayer(kLayerId, cc::mojom::LayerType::kTileDisplay);
  auto tile_extra3 = mojom::TileDisplayLayerExtra::New();
  tile_extra3->is_directly_composited_image = false;
  layer_props3->layer_extra =
      mojom::LayerExtra::NewTileDisplayLayerExtra(std::move(tile_extra3));
  update3->layers.push_back(std::move(layer_props3));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());

  EXPECT_FALSE(tile_display_layer_impl->IsDirectlyCompositedImage());
}

TEST_F(LayerContextImplUpdateDisplayTreeTileDisplayLayerPropertiesTest,
       UpdateNearestNeighbor) {
  constexpr int kLayerId = 2;

  // Initial update: Create TileDisplayLayer with default properties.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), cc::mojom::LayerType::kTileDisplay,
                          kLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());

  cc::LayerImpl* layer_impl_base = GetLayerFromActiveTree(kLayerId);
  ASSERT_NE(nullptr, layer_impl_base);
  ASSERT_EQ(layer_impl_base->GetLayerType(),
            cc::mojom::LayerType::kTileDisplay);
  auto* tile_display_layer_impl =
      static_cast<cc::TileDisplayLayerImpl*>(layer_impl_base);

  EXPECT_FALSE(tile_display_layer_impl->nearest_neighbor());

  // Second update: Set nearest_neighbor to true.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 =
      CreateManualLayer(kLayerId, cc::mojom::LayerType::kTileDisplay);
  auto tile_extra2 = mojom::TileDisplayLayerExtra::New();
  tile_extra2->nearest_neighbor = true;
  layer_props2->layer_extra =
      mojom::LayerExtra::NewTileDisplayLayerExtra(std::move(tile_extra2));
  update2->layers.push_back(std::move(layer_props2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());

  EXPECT_TRUE(tile_display_layer_impl->nearest_neighbor());

  // Third update: Set nearest_neighbor to false.
  auto update3 = CreateDefaultUpdate();
  auto layer_props3 =
      CreateManualLayer(kLayerId, cc::mojom::LayerType::kTileDisplay);
  auto tile_extra3 = mojom::TileDisplayLayerExtra::New();
  tile_extra3->nearest_neighbor = false;
  layer_props3->layer_extra =
      mojom::LayerExtra::NewTileDisplayLayerExtra(std::move(tile_extra3));
  update3->layers.push_back(std::move(layer_props3));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());

  EXPECT_FALSE(tile_display_layer_impl->nearest_neighbor());
}

TEST_F(LayerContextImplUpdateDisplayTreeTilingTest, TilingAndTileLifecycle) {
  constexpr int kLayerId = 2;
  constexpr float kScaleKey1 = 1.0f;
  constexpr float kScaleKey2 = 2.0f;
  const gfx::Size kTileSize1(64, 64);
  const gfx::Size kTileSize2(128, 128);
  const gfx::Rect kTilingRect1(0, 0, 200, 200);
  const gfx::Rect kTilingRect2(0, 0, 400, 400);
  const cc::TileIndex kTileIndex1(0, 0);
  const cc::TileIndex kTileIndex2(1, 1);
  const ResourceId kResourceId1(23);
  const ResourceId kResourceId2(45);

  // Initial update: Create TileDisplayLayer.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), cc::mojom::LayerType::kTileDisplay,
                          kLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());

  cc::LayerTreeImpl* active_tree =
      layer_context_impl_->host_impl()->active_tree();
  ASSERT_TRUE(active_tree);
  cc::LayerImpl* layer_impl_base = active_tree->LayerById(kLayerId);
  ASSERT_NE(nullptr, layer_impl_base);
  ASSERT_EQ(layer_impl_base->GetLayerType(),
            cc::mojom::LayerType::kTileDisplay);
  auto* tile_display_layer_impl =
      static_cast<cc::TileDisplayLayerImpl*>(layer_impl_base);

  // Test Case 1: Create a new Tiling with a SolidColor Tile.
  auto update_create_tiling = CreateDefaultUpdate();
  auto tiling1 = mojom::Tiling::New();
  tiling1->layer_id = kLayerId;
  tiling1->scale_key = kScaleKey1;
  tiling1->raster_scale = gfx::Vector2dF(kScaleKey1, kScaleKey1);
  tiling1->tile_size = kTileSize1;
  tiling1->tiling_rect = kTilingRect1;
  auto tile1_solid = mojom::Tile::New();
  tile1_solid->column_index = kTileIndex1.i;
  tile1_solid->row_index = kTileIndex1.j;
  tile1_solid->contents =
      mojom::TileContents::NewSolidColor(SkColors::kMagenta);
  tiling1->tiles.push_back(std::move(tile1_solid));
  update_create_tiling->tilings.push_back(std::move(tiling1));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update_create_tiling))
          .has_value());
  const cc::TileDisplayLayerTiling* tiling_impl1 =
      tile_display_layer_impl->GetTilingForTesting(kScaleKey1);
  ASSERT_NE(nullptr, tiling_impl1);
  EXPECT_EQ(tiling_impl1->tile_size(), kTileSize1);
  EXPECT_EQ(tiling_impl1->tiling_rect(), kTilingRect1);
  ASSERT_NE(nullptr, tiling_impl1->TileAt(kTileIndex1));
  EXPECT_TRUE(tiling_impl1->TileAt(kTileIndex1)->solid_color().has_value());
  EXPECT_EQ(tiling_impl1->TileAt(kTileIndex1)->solid_color().value(),
            SkColors::kMagenta);

  // Test Case 2: Update existing Tiling (tile_size) and add a Resource Tile.
  auto update_update_tiling = CreateDefaultUpdate();
  auto tiling1_updated = mojom::Tiling::New();
  tiling1_updated->layer_id = kLayerId;
  tiling1_updated->scale_key = kScaleKey1;  // Same key to update
  tiling1_updated->raster_scale = gfx::Vector2dF(kScaleKey1, kScaleKey1);
  tiling1_updated->tile_size = kTileSize2;  // New tile size
  tiling1_updated->tiling_rect = kTilingRect1;
  // Add a new resource tile
  auto tile2_resource = mojom::Tile::New();
  tile2_resource->column_index = kTileIndex2.i;
  tile2_resource->row_index = kTileIndex2.j;
  auto resource_contents = mojom::TileResource::New();
  resource_contents->resource = MakeFakeResource(kTileSize2);
  resource_contents->resource.id = kResourceId1;
  tile2_resource->contents =
      mojom::TileContents::NewResource(std::move(resource_contents));
  tiling1_updated->tiles.push_back(std::move(tile2_resource));
  update_update_tiling->tilings.push_back(std::move(tiling1_updated));

  auto result =
      layer_context_impl_->DoUpdateDisplayTree(std::move(update_update_tiling));
  EXPECT_TRUE(result.has_value()) << result.error();
  ASSERT_EQ(tiling_impl1,
            tile_display_layer_impl->GetTilingForTesting(
                kScaleKey1));  // Should still be the same tiling object
  EXPECT_EQ(tiling_impl1->tile_size(), kTileSize2);  // Updated
  // Previous tiles should still exist after a tile_size change
  EXPECT_NE(nullptr, tiling_impl1->TileAt(kTileIndex1));
  ASSERT_NE(nullptr, tiling_impl1->TileAt(kTileIndex2));
  EXPECT_TRUE(tiling_impl1->TileAt(kTileIndex2)->resource().has_value());

  // Test Case 3: Add a second Tiling with a MissingReason Tile.
  auto update_add_tiling2 = CreateDefaultUpdate();
  auto tiling2 = mojom::Tiling::New();
  tiling2->layer_id = kLayerId;
  tiling2->scale_key = kScaleKey2;
  tiling2->raster_scale = gfx::Vector2dF(kScaleKey2, kScaleKey2);
  tiling2->tile_size = kTileSize1;
  tiling2->tiling_rect = kTilingRect2;
  auto tile3_missing = mojom::Tile::New();
  tile3_missing->column_index = kTileIndex1.i;
  tile3_missing->row_index = kTileIndex1.j;
  tile3_missing->contents = mojom::TileContents::NewMissingReason(
      cc::mojom::MissingTileReason::kResourceNotReady);
  tiling2->tiles.push_back(std::move(tile3_missing));
  update_add_tiling2->tilings.push_back(std::move(tiling2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update_add_tiling2))
          .has_value());
  const cc::TileDisplayLayerTiling* tiling_impl2 =
      tile_display_layer_impl->GetTilingForTesting(kScaleKey2);
  ASSERT_NE(nullptr, tiling_impl2);
  EXPECT_EQ(tiling_impl2->tile_size(), kTileSize1);
  ASSERT_NE(nullptr, tiling_impl2->TileAt(kTileIndex1));
  EXPECT_TRUE(std::holds_alternative<cc::TileDisplayLayerImpl::NoContents>(
      tiling_impl2->TileAt(kTileIndex1)->contents()));
  EXPECT_EQ(std::get<cc::TileDisplayLayerImpl::NoContents>(
                tiling_impl2->TileAt(kTileIndex1)->contents())
                .reason,
            cc::mojom::MissingTileReason::kResourceNotReady);

  // Test Case 4: Explicitly delete a Tiling (tiling_impl1).
  auto update_delete_tiling1 = CreateDefaultUpdate();
  auto tiling1_deleted_marker = mojom::Tiling::New();
  tiling1_deleted_marker->layer_id = kLayerId;
  tiling1_deleted_marker->scale_key = kScaleKey1;
  tiling1_deleted_marker->is_deleted = true;
  update_delete_tiling1->tilings.push_back(std::move(tiling1_deleted_marker));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update_delete_tiling1))
          .has_value());
  EXPECT_EQ(nullptr, tile_display_layer_impl->GetTilingForTesting(kScaleKey1));
  EXPECT_NE(nullptr, tile_display_layer_impl->GetTilingForTesting(kScaleKey2));

  // Test Case 5: Implicitly delete a Tiling by removing all its tiles.
  // (tiling_impl2 currently has one kMissingReason tile).
  // Send an update for tiling_impl2 that marks its only tile as deleted.
  auto update_empty_tiling2 = CreateDefaultUpdate();
  auto tiling2_empty_update = mojom::Tiling::New();
  tiling2_empty_update->layer_id = kLayerId;
  tiling2_empty_update->scale_key = kScaleKey2;
  tiling2_empty_update->raster_scale = gfx::Vector2dF(kScaleKey2, kScaleKey2);
  tiling2_empty_update->tile_size = kTileSize1;  // Keep same tile size
  tiling2_empty_update->tiling_rect = kTilingRect2;
  auto tile_deleted_marker = mojom::Tile::New();
  tile_deleted_marker->column_index = kTileIndex1.i;
  tile_deleted_marker->row_index = kTileIndex1.j;
  tile_deleted_marker->contents = mojom::TileContents::NewMissingReason(
      cc::mojom::MissingTileReason::kTileDeleted);  // Mark for deletion
  tiling2_empty_update->tiles.push_back(std::move(tile_deleted_marker));
  update_empty_tiling2->tilings.push_back(std::move(tiling2_empty_update));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update_empty_tiling2))
          .has_value());
  EXPECT_EQ(nullptr, tile_display_layer_impl->GetTilingForTesting(kScaleKey2));

  // Test Case 6: Update a tile within a tiling (change its content type).
  // First, re-add tiling1 with a solid color tile.
  auto update_recreate_tiling1 = CreateDefaultUpdate();
  auto tiling1_recreate = mojom::Tiling::New();
  tiling1_recreate->layer_id = kLayerId;
  tiling1_recreate->scale_key = kScaleKey1;
  tiling1_recreate->raster_scale = gfx::Vector2dF(kScaleKey1, kScaleKey1);
  tiling1_recreate->tile_size = kTileSize1;
  tiling1_recreate->tiling_rect = kTilingRect1;
  auto tile_initial_solid = mojom::Tile::New();
  tile_initial_solid->column_index = kTileIndex1.i;
  tile_initial_solid->row_index = kTileIndex1.j;
  tile_initial_solid->contents =
      mojom::TileContents::NewSolidColor(SkColors::kBlue);
  tiling1_recreate->tiles.push_back(std::move(tile_initial_solid));
  update_recreate_tiling1->tilings.push_back(std::move(tiling1_recreate));
  EXPECT_TRUE(layer_context_impl_
                  ->DoUpdateDisplayTree(std::move(update_recreate_tiling1))
                  .has_value());
  tiling_impl1 = tile_display_layer_impl->GetTilingForTesting(kScaleKey1);
  ASSERT_NE(nullptr, tiling_impl1);
  ASSERT_NE(nullptr, tiling_impl1->TileAt(kTileIndex1));
  EXPECT_TRUE(tiling_impl1->TileAt(kTileIndex1)->solid_color().has_value());

  // Now, update that tile to be a resource tile.
  auto update_change_tile_type = CreateDefaultUpdate();
  auto tiling1_tile_update = mojom::Tiling::New();
  tiling1_tile_update->layer_id = kLayerId;
  tiling1_tile_update->scale_key = kScaleKey1;
  tiling1_tile_update->raster_scale = gfx::Vector2dF(kScaleKey1, kScaleKey1);
  tiling1_tile_update->tile_size = kTileSize1;  // Keep same tile size
  tiling1_tile_update->tiling_rect = kTilingRect1;
  auto tile_updated_to_resource = mojom::Tile::New();
  tile_updated_to_resource->column_index = kTileIndex1.i;
  tile_updated_to_resource->row_index = kTileIndex1.j;
  auto resource_contents_updated = mojom::TileResource::New();
  resource_contents_updated->resource = MakeFakeResource(kTileSize1);
  resource_contents_updated->resource.id = kResourceId2;
  tile_updated_to_resource->contents =
      mojom::TileContents::NewResource(std::move(resource_contents_updated));
  tiling1_tile_update->tiles.push_back(std::move(tile_updated_to_resource));
  update_change_tile_type->tilings.push_back(std::move(tiling1_tile_update));

  EXPECT_TRUE(layer_context_impl_
                  ->DoUpdateDisplayTree(std::move(update_change_tile_type))
                  .has_value());
  ASSERT_NE(nullptr, tiling_impl1->TileAt(kTileIndex1));
  EXPECT_FALSE(tiling_impl1->TileAt(kTileIndex1)->solid_color().has_value());
  EXPECT_TRUE(tiling_impl1->TileAt(kTileIndex1)->resource().has_value());

  // Test Case 7: Attempt to add a tile with an invalid resource ID.
  auto update_invalid_resource_tile = CreateDefaultUpdate();
  auto tiling1_invalid_resource_update = mojom::Tiling::New();
  tiling1_invalid_resource_update->layer_id = kLayerId;
  tiling1_invalid_resource_update->scale_key = kScaleKey1;
  tiling1_invalid_resource_update->raster_scale =
      gfx::Vector2dF(kScaleKey1, kScaleKey1);
  tiling1_invalid_resource_update->tile_size = kTileSize1;
  tiling1_invalid_resource_update->tiling_rect = kTilingRect1;
  auto tile_invalid_resource = mojom::Tile::New();
  tile_invalid_resource->column_index = kTileIndex1.i;  // Use existing index
  tile_invalid_resource->row_index = kTileIndex1.j;
  auto invalid_resource_contents = mojom::TileResource::New();
  invalid_resource_contents->resource = MakeFakeResource(kTileSize1);
  invalid_resource_contents->resource.id = kInvalidResourceId;  // Invalid ID
  tile_invalid_resource->contents =
      mojom::TileContents::NewResource(std::move(invalid_resource_contents));
  tiling1_invalid_resource_update->tiles.push_back(
      std::move(tile_invalid_resource));
  update_invalid_resource_tile->tilings.push_back(
      std::move(tiling1_invalid_resource_update));
  auto update_invalid_resource_tile_result =
      layer_context_impl_->DoUpdateDisplayTree(
          std::move(update_invalid_resource_tile));
  ASSERT_FALSE(update_invalid_resource_tile_result.has_value());
  EXPECT_EQ(update_invalid_resource_tile_result.error(),
            "Invalid tile resource");
}

TEST_F(LayerContextImplUpdateDisplayTreeTilingTest,
       TilingForNonTileDisplayLayerFails) {
  constexpr int kNonTileDisplayLayerId = 2;

  // Initial update: Create a regular Layer (not TileDisplayLayer).
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), cc::mojom::LayerType::kLayer,
                          kNonTileDisplayLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());

  // Attempt to send tiling data for this non-TileDisplayLayer.
  auto update_tiling = CreateDefaultUpdate();
  auto tiling = mojom::Tiling::New();
  tiling->layer_id = kNonTileDisplayLayerId;  // ID of a non-TileDisplayLayer
  tiling->scale_key = 1.0f;
  tiling->raster_scale = gfx::Vector2dF(1.0f, 1.0f);
  tiling->tile_size = gfx::Size(64, 64);
  tiling->tiling_rect = gfx::Rect(100, 100);
  auto tile = mojom::Tile::New();
  tile->column_index = 0;
  tile->row_index = 0;
  tile->contents = mojom::TileContents::NewSolidColor(SkColors::kRed);
  tiling->tiles.push_back(std::move(tile));
  update_tiling->tilings.push_back(std::move(tiling));

  auto result =
      layer_context_impl_->DoUpdateDisplayTree(std::move(update_tiling));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid tile update");
}

TEST_F(LayerContextImplUpdateDisplayTreeTilingTest,
       TilingWithInvalidLayerIdFails) {
  constexpr int kInvalidLayerId = 999;  // An ID that doesn't exist.

  auto update_tiling = CreateDefaultUpdate();
  auto tiling = mojom::Tiling::New();
  tiling->layer_id = kInvalidLayerId;
  tiling->scale_key = 1.0f;
  // Other tiling properties are set to valid defaults for this test.
  tiling->raster_scale = gfx::Vector2dF(1.0f, 1.0f);
  tiling->tile_size = gfx::Size(64, 64);
  tiling->tiling_rect = gfx::Rect(100, 100);
  update_tiling->tilings.push_back(std::move(tiling));

  // No specific error message for invalid layer ID in tiling, it's handled by
  // layer_impl being null. The update should still pass, but no tiling occurs.
  EXPECT_TRUE(layer_context_impl_->DoUpdateDisplayTree(std::move(update_tiling))
                  .has_value());
  // We can't directly verify that no tiling happened for the invalid ID without
  // more complex state inspection, but the update itself shouldn't crash.
}

class LayerContextImplUpdateDisplayTreeTextureLayerTest
    : public LayerContextImplLayerLifecycleTest {
 protected:
  cc::TextureLayerImpl* GetTextureLayerFromActiveTree(int layer_id) {
    cc::LayerImpl* layer = GetLayerFromActiveTree(layer_id);
    if (layer && layer->GetLayerType() == cc::mojom::LayerType::kTexture) {
      return static_cast<cc::TextureLayerImpl*>(layer);
    }
    return nullptr;
  }
};

TEST_F(LayerContextImplUpdateDisplayTreeTextureLayerTest, UpdateUVRect) {
  constexpr int kTextureLayerId = 2;
  const gfx::PointF kUpdatedUVTopLeft(0.1f, 0.2f);
  const gfx::PointF kUpdatedUVBottomRight(0.8f, 0.9f);

  // Initial update: Create TextureLayer.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), cc::mojom::LayerType::kTexture,
                          kTextureLayerId);

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  VerifyLayerExists(kTextureLayerId, true);

  cc::TextureLayerImpl* texture_layer_impl =
      GetTextureLayerFromActiveTree(kTextureLayerId);
  ASSERT_NE(nullptr, texture_layer_impl);

  EXPECT_EQ(texture_layer_impl->uv_top_left(), kDefaultUVTopLeft);
  EXPECT_EQ(texture_layer_impl->uv_bottom_right(), kDefaultUVBottomRight);

  // Second update: Update UV rect.
  auto update2 = CreateDefaultUpdate();
  auto layer_props =
      CreateManualLayer(kTextureLayerId, cc::mojom::LayerType::kTexture);
  auto& texture_extra = layer_props->layer_extra->get_texture_layer_extra();
  texture_extra->uv_top_left = kUpdatedUVTopLeft;
  texture_extra->uv_bottom_right = kUpdatedUVBottomRight;
  update2->layers.push_back(std::move(layer_props));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  ASSERT_NE(nullptr, texture_layer_impl);
  EXPECT_EQ(texture_layer_impl->uv_top_left(), kUpdatedUVTopLeft);
  EXPECT_EQ(texture_layer_impl->uv_bottom_right(), kUpdatedUVBottomRight);
}

TEST_F(LayerContextImplUpdateDisplayTreeTextureLayerTest,
       UpdateBlendBackgroundColor) {
  constexpr int kTextureLayerId = 2;
  constexpr bool kUpdatedBlendBackgroundColor = true;

  // Initial update: Create TextureLayer with default blend_background_color.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), cc::mojom::LayerType::kTexture,
                          kTextureLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  VerifyLayerExists(kTextureLayerId, true);

  cc::TextureLayerImpl* texture_layer_impl =
      GetTextureLayerFromActiveTree(kTextureLayerId);
  ASSERT_NE(nullptr, texture_layer_impl);
  EXPECT_EQ(texture_layer_impl->blend_background_color(),
            kDefaultBlendBackgroundColor);

  // Second update: Update blend_background_color to true.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 =
      CreateManualLayer(kTextureLayerId, cc::mojom::LayerType::kTexture);
  auto& texture_extra2 = layer_props2->layer_extra->get_texture_layer_extra();
  texture_extra2->blend_background_color = kUpdatedBlendBackgroundColor;
  update2->layers.push_back(std::move(layer_props2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  ASSERT_NE(nullptr, texture_layer_impl);
  EXPECT_EQ(texture_layer_impl->blend_background_color(),
            kUpdatedBlendBackgroundColor);
}

TEST_F(LayerContextImplUpdateDisplayTreeTextureLayerTest,
       UpdateForceTextureToOpaque) {
  constexpr int kTextureLayerId = 2;
  constexpr bool kUpdatedForceTextureToOpaque = true;

  // Initial update: Create TextureLayer with default
  // kDefaultForceTextureToOpaque.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), cc::mojom::LayerType::kTexture,
                          kTextureLayerId);
  // Default is false.
  // No need to explicitly set texture_extra1->force_texture_to_opaque = false;
  // as it's the default from CreateDefaultLayerExtra.

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  VerifyLayerExists(kTextureLayerId, true);

  cc::TextureLayerImpl* texture_layer_impl =
      GetTextureLayerFromActiveTree(kTextureLayerId);
  ASSERT_NE(nullptr, texture_layer_impl);
  EXPECT_EQ(texture_layer_impl->force_texture_to_opaque(),
            kDefaultForceTextureToOpaque);

  // Second update: Update force_texture_to_opaque to true.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 =
      CreateManualLayer(kTextureLayerId, cc::mojom::LayerType::kTexture);
  auto& texture_extra2 = layer_props2->layer_extra->get_texture_layer_extra();
  texture_extra2->force_texture_to_opaque = kUpdatedForceTextureToOpaque;
  update2->layers.push_back(std::move(layer_props2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  ASSERT_NE(nullptr, texture_layer_impl);
  EXPECT_EQ(texture_layer_impl->force_texture_to_opaque(),
            kUpdatedForceTextureToOpaque);
}

TEST_F(LayerContextImplUpdateDisplayTreeTextureLayerTest,
       UpdateTransferableResource) {
  constexpr int kTextureLayerId = 2;
  const gfx::Size kResourceSize1(10, 10);
  const gfx::Size kResourceSize2(12, 12);

  // Initial update: Create TextureLayer without a resource.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), cc::mojom::LayerType::kTexture,
                          kTextureLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  VerifyLayerExists(kTextureLayerId, true);

  cc::TextureLayerImpl* texture_layer_impl =
      GetTextureLayerFromActiveTree(kTextureLayerId);
  ASSERT_NE(nullptr, texture_layer_impl);
  EXPECT_TRUE(texture_layer_impl->transferable_resource().is_empty());

  // Second update: Set transferable_resource1.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(
      kTextureLayerId, cc::mojom::LayerType::kTexture, kResourceSize1);
  auto& texture_extra2 = layer_props2->layer_extra->get_texture_layer_extra();
  TransferableResource resource1 = MakeFakeResource(kResourceSize1);
  texture_extra2->transferable_resource = resource1;
  update2->layers.push_back(std::move(layer_props2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  // The TransferableResource matches what we sent.
  EXPECT_EQ(texture_layer_impl->transferable_resource(), resource1);

  // Third update: Set transferable_resource2 (different resource).
  auto update3 = CreateDefaultUpdate();
  auto layer_props3 = CreateManualLayer(
      kTextureLayerId, cc::mojom::LayerType::kTexture, kResourceSize2);
  auto& texture_extra3 = layer_props3->layer_extra->get_texture_layer_extra();
  TransferableResource resource2 = MakeFakeResource(kResourceSize2);
  texture_extra3->transferable_resource = resource2;
  update3->layers.push_back(std::move(layer_props3));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  EXPECT_EQ(texture_layer_impl->transferable_resource(), resource2);

  // Fourth update: Clear the resource.
  auto update4 = CreateDefaultUpdate();
  auto layer_props4 = CreateManualLayer(
      kTextureLayerId, cc::mojom::LayerType::kTexture, kResourceSize1);
  // Clearing has to be via an explicit empty resource.
  auto& texture_extra4 = layer_props4->layer_extra->get_texture_layer_extra();
  texture_extra4->transferable_resource = TransferableResource();
  update4->layers.push_back(std::move(layer_props4));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update4)).has_value());
  EXPECT_TRUE(texture_layer_impl->transferable_resource().is_empty());
}

class LayerContextImplUpdateDisplayTreeSurfaceLayerTest
    : public LayerContextImplLayerLifecycleTest {
 protected:
  cc::SurfaceLayerImpl* GetSurfaceLayerFromActiveTree(int layer_id) {
    cc::LayerImpl* layer = GetLayerFromActiveTree(layer_id);
    if (layer && layer->GetLayerType() == cc::mojom::LayerType::kSurface) {
      return static_cast<cc::SurfaceLayerImpl*>(layer);
    }
    return nullptr;
  }
};

TEST_F(LayerContextImplUpdateDisplayTreeSurfaceLayerTest,
       UpdateBooleanProperties) {
  constexpr int kSurfaceLayerId = 2;

  // Initial update: Create SurfaceLayer with default boolean values.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), cc::mojom::LayerType::kSurface,
                          kSurfaceLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  VerifyLayerExists(kSurfaceLayerId, true);

  cc::SurfaceLayerImpl* layer_impl =
      GetSurfaceLayerFromActiveTree(kSurfaceLayerId);
  ASSERT_NE(nullptr, layer_impl);

  // Defaults should be false from CreateDefaultLayerExtra.
  EXPECT_EQ(layer_impl->stretch_content_to_fill_bounds(),
            kDefaultStretchContentToFillBounds);
  EXPECT_EQ(layer_impl->surface_hit_testable(), kDefaultSurfaceHitTestable);
  EXPECT_EQ(layer_impl->has_pointer_events_none(),
            kDefaultHasPointerEventsNone);
  EXPECT_EQ(layer_impl->is_reflection(), kDefaultIsReflection);
  EXPECT_EQ(layer_impl->override_child_paint_flags(),
            kDefaultOverrideChildPaintFlags);

  // Second update: Update all boolean properties to true.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 =
      CreateManualLayer(kSurfaceLayerId, cc::mojom::LayerType::kSurface);
  auto& surface_extra2 = layer_props2->layer_extra->get_surface_layer_extra();
  surface_extra2->stretch_content_to_fill_bounds = true;
  surface_extra2->surface_hit_testable = true;
  surface_extra2->has_pointer_events_none = true;
  surface_extra2->is_reflection = true;
  surface_extra2->override_child_paint_flags = true;
  update2->layers.push_back(std::move(layer_props2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_TRUE(layer_impl->stretch_content_to_fill_bounds());
  EXPECT_TRUE(layer_impl->surface_hit_testable());
  EXPECT_TRUE(layer_impl->has_pointer_events_none());
  EXPECT_TRUE(layer_impl->is_reflection());
  EXPECT_TRUE(layer_impl->override_child_paint_flags());

  // Third update: Update all boolean properties back to false.
  auto update3 = CreateDefaultUpdate();
  auto layer_props3 =
      CreateManualLayer(kSurfaceLayerId, cc::mojom::LayerType::kSurface);
  auto& surface_extra3 = layer_props3->layer_extra->get_surface_layer_extra();
  surface_extra3->stretch_content_to_fill_bounds = false;
  surface_extra3->surface_hit_testable = false;
  surface_extra3->has_pointer_events_none = false;
  surface_extra3->is_reflection = false;
  surface_extra3->override_child_paint_flags = false;
  update3->layers.push_back(std::move(layer_props3));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_FALSE(layer_impl->stretch_content_to_fill_bounds());
  EXPECT_FALSE(layer_impl->surface_hit_testable());
  EXPECT_FALSE(layer_impl->has_pointer_events_none());
  EXPECT_FALSE(layer_impl->is_reflection());
  EXPECT_FALSE(layer_impl->override_child_paint_flags());
}

TEST_F(LayerContextImplUpdateDisplayTreeSurfaceLayerTest,
       UpdateSurfaceRangeAndDeadline) {
  constexpr int kSurfaceLayerId = 2;
  constexpr uint32_t kUpdatedDeadlineInFrames = 5u;

  // Initial update: Create SurfaceLayer with default range and deadline.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), cc::mojom::LayerType::kSurface,
                          kSurfaceLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::SurfaceLayerImpl* layer_impl =
      GetSurfaceLayerFromActiveTree(kSurfaceLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_EQ(layer_impl->range(), kDefaultSurfaceRange);
  EXPECT_EQ(layer_impl->deadline_in_frames(), kDefaultDeadlineInFrames);

  // Second update: Update surface_range and deadline_in_frames.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 =
      CreateManualLayer(kSurfaceLayerId, cc::mojom::LayerType::kSurface);
  auto& surface_extra2 = layer_props2->layer_extra->get_surface_layer_extra();
  LocalSurfaceId new_lsi(4, base::UnguessableToken::CreateForTesting(5, 6));
  surface_extra2->surface_range =
      SurfaceRange(std::nullopt, SurfaceId(kDefaultFrameSinkId, new_lsi));
  surface_extra2->deadline_in_frames = kUpdatedDeadlineInFrames;
  update2->layers.push_back(std::move(layer_props2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(layer_impl->range().end(), SurfaceId(kDefaultFrameSinkId, new_lsi));
  EXPECT_EQ(layer_impl->deadline_in_frames(), kUpdatedDeadlineInFrames);
}

TEST_F(LayerContextImplUpdateDisplayTreeSurfaceLayerTest,
       UpdateWillDrawNeedsReset) {
  constexpr int kSurfaceLayerId = 2;

  // Initial update: Create SurfaceLayer with default will_draw_needs_reset.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), cc::mojom::LayerType::kSurface,
                          kSurfaceLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::SurfaceLayerImpl* layer_impl1 =
      GetSurfaceLayerFromActiveTree(kSurfaceLayerId);
  ASSERT_NE(nullptr, layer_impl1);
  EXPECT_EQ(layer_impl1->will_draw_needs_reset(), kDefaultWillDrawNeedsReset);

  // Second update: Set will_draw_needs_reset to true.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 =
      CreateManualLayer(kSurfaceLayerId, cc::mojom::LayerType::kSurface);
  auto& surface_extra2 = layer_props2->layer_extra->get_surface_layer_extra();
  surface_extra2->will_draw_needs_reset = true;
  update2->layers.push_back(std::move(layer_props2));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  cc::SurfaceLayerImpl* layer_impl2 =
      GetSurfaceLayerFromActiveTree(kSurfaceLayerId);
  ASSERT_NE(nullptr, layer_impl2);
  EXPECT_TRUE(layer_impl2->will_draw_needs_reset());
}

// Test fixture for ScrollbarLayerImplBase specific property updates,
// parameterized by scrollbar layer type.
class LayerContextImplUpdateDisplayTreeScrollbarLayerBaseTest
    : public LayerContextImplLayerLifecycleTest,
      public testing::WithParamInterface<cc::mojom::LayerType> {
 protected:
  cc::ScrollbarLayerImplBase* GetScrollbarLayerBaseFromActiveTree(
      int layer_id) {
    cc::LayerImpl* layer = GetLayerFromActiveTree(layer_id);
    if (layer && layer->IsScrollbarLayer()) {
      return static_cast<cc::ScrollbarLayerImplBase*>(layer);
    }
    return nullptr;
  }

  mojom::ScrollbarLayerBaseExtra* GetScrollbarBaseExtra(
      mojom::LayerExtra& layer_extra) {
    switch (GetParam()) {
      case cc::mojom::LayerType::kSolidColorScrollbar:
        return layer_extra.get_solid_color_scrollbar_layer_extra()
            ->scrollbar_base_extra.get();
      case cc::mojom::LayerType::kPaintedScrollbar:
        return layer_extra.get_painted_scrollbar_layer_extra()
            ->scrollbar_base_extra.get();
      case cc::mojom::LayerType::kNinePatchThumbScrollbar:
        return layer_extra.get_nine_patch_thumb_scrollbar_layer_extra()
            ->scrollbar_base_extra.get();
      default:
        NOTREACHED();
    }
  }

  const gfx::Size kDefaultScrollbarLayerBounds = gfx::Size(10, 100);
};

TEST_P(LayerContextImplUpdateDisplayTreeScrollbarLayerBaseTest,
       InitialIsHorizontalOrientation) {
  constexpr int kScrollbarLayerId1 = 2;
  constexpr int kScrollbarLayerId2 = 3;

  // Test 1: Initial is_horizontal_orientation setting is respected.
  auto update1 = CreateDefaultUpdate();
  auto layer_props1 = CreateManualLayer(kScrollbarLayerId1, GetParam(),
                                        kDefaultScrollbarLayerBounds);
  auto layer_props2 = CreateManualLayer(kScrollbarLayerId2, GetParam(),
                                        kDefaultScrollbarLayerBounds);

  GetScrollbarBaseExtra(*layer_props1->layer_extra)->is_horizontal_orientation =
      false;
  GetScrollbarBaseExtra(*layer_props2->layer_extra)->is_horizontal_orientation =
      true;

  update1->layers.push_back(std::move(layer_props1));
  update1->layers.push_back(std::move(layer_props2));
  layer_order_.push_back(kScrollbarLayerId1);
  layer_order_.push_back(kScrollbarLayerId2);
  update1->layer_order = layer_order_;

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::ScrollbarLayerImplBase* layer_impl1 =
      GetScrollbarLayerBaseFromActiveTree(kScrollbarLayerId1);
  cc::ScrollbarLayerImplBase* layer_impl2 =
      GetScrollbarLayerBaseFromActiveTree(kScrollbarLayerId2);
  ASSERT_NE(nullptr, layer_impl1);
  ASSERT_NE(nullptr, layer_impl2);
  EXPECT_EQ(layer_impl1->orientation(), cc::ScrollbarOrientation::kVertical);
  EXPECT_EQ(layer_impl2->orientation(), cc::ScrollbarOrientation::kHorizontal);

  // Test 2: Updating the is_horizontal_orientation property on an
  // existing layer has no effect.
  auto update2 = CreateDefaultUpdate();
  auto layer_props3 = CreateManualLayer(kScrollbarLayerId1, GetParam(),
                                        kDefaultScrollbarLayerBounds);
  auto layer_props4 = CreateManualLayer(kScrollbarLayerId2, GetParam(),
                                        kDefaultScrollbarLayerBounds);

  // Try to swap the values
  GetScrollbarBaseExtra(*layer_props3->layer_extra)->is_horizontal_orientation =
      true;
  GetScrollbarBaseExtra(*layer_props4->layer_extra)->is_horizontal_orientation =
      false;

  update2->layers.push_back(std::move(layer_props3));
  update2->layers.push_back(std::move(layer_props4));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());

  EXPECT_EQ(layer_impl1->orientation(), cc::ScrollbarOrientation::kVertical);
  EXPECT_EQ(layer_impl2->orientation(), cc::ScrollbarOrientation::kHorizontal);
}

TEST_P(LayerContextImplUpdateDisplayTreeScrollbarLayerBaseTest,
       InitialIsLeftSideVerticalScrollbar) {
  constexpr int kScrollbarLayerId1 = 2;
  constexpr int kScrollbarLayerId2 = 3;

  // Test 1: Initial is_left_side_vertical_scrollbar setting is respected.
  auto update1 = CreateDefaultUpdate();
  auto layer_props1 = CreateManualLayer(kScrollbarLayerId1, GetParam(),
                                        kDefaultScrollbarLayerBounds);
  auto layer_props2 = CreateManualLayer(kScrollbarLayerId2, GetParam(),
                                        kDefaultScrollbarLayerBounds);
  GetScrollbarBaseExtra(*layer_props1->layer_extra)
      ->is_left_side_vertical_scrollbar = false;
  GetScrollbarBaseExtra(*layer_props2->layer_extra)
      ->is_left_side_vertical_scrollbar = true;

  update1->layers.push_back(std::move(layer_props1));
  update1->layers.push_back(std::move(layer_props2));
  layer_order_.push_back(kScrollbarLayerId1);
  layer_order_.push_back(kScrollbarLayerId2);
  update1->layer_order = layer_order_;

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::ScrollbarLayerImplBase* layer_impl1 =
      GetScrollbarLayerBaseFromActiveTree(kScrollbarLayerId1);
  cc::ScrollbarLayerImplBase* layer_impl2 =
      GetScrollbarLayerBaseFromActiveTree(kScrollbarLayerId2);
  ASSERT_NE(nullptr, layer_impl1);
  ASSERT_NE(nullptr, layer_impl2);
  EXPECT_EQ(layer_impl1->is_left_side_vertical_scrollbar(), false);
  EXPECT_EQ(layer_impl2->is_left_side_vertical_scrollbar(), true);

  // Test 2: Updating the is_left_side_vertical_scrollbar property on an
  // existing layer has no effect.
  auto update2 = CreateDefaultUpdate();
  auto layer_props3 = CreateManualLayer(kScrollbarLayerId1, GetParam(),
                                        kDefaultScrollbarLayerBounds);
  auto layer_props4 = CreateManualLayer(kScrollbarLayerId2, GetParam(),
                                        kDefaultScrollbarLayerBounds);

  // Try to swap the values.
  GetScrollbarBaseExtra(*layer_props3->layer_extra)
      ->is_left_side_vertical_scrollbar = true;
  GetScrollbarBaseExtra(*layer_props4->layer_extra)
      ->is_left_side_vertical_scrollbar = false;

  update2->layers.push_back(std::move(layer_props3));
  update2->layers.push_back(std::move(layer_props4));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());

  EXPECT_EQ(layer_impl1->is_left_side_vertical_scrollbar(), false);
  EXPECT_EQ(layer_impl2->is_left_side_vertical_scrollbar(), true);
}

TEST_P(LayerContextImplUpdateDisplayTreeScrollbarLayerBaseTest,
       InitialIsOverlayScrollbar) {
  constexpr int kScrollbarLayerId1 = 2;
  constexpr int kScrollbarLayerId2 = 3;

  // Test 1: Initial is_overlay_scrollbar setting is respected.
  auto update1 = CreateDefaultUpdate();
  auto layer_props1 = CreateManualLayer(kScrollbarLayerId1, GetParam(),
                                        kDefaultScrollbarLayerBounds);
  auto layer_props2 = CreateManualLayer(kScrollbarLayerId2, GetParam(),
                                        kDefaultScrollbarLayerBounds);

  GetScrollbarBaseExtra(*layer_props1->layer_extra)->is_overlay_scrollbar =
      false;
  GetScrollbarBaseExtra(*layer_props2->layer_extra)->is_overlay_scrollbar =
      true;

  update1->layers.push_back(std::move(layer_props1));
  update1->layers.push_back(std::move(layer_props2));
  layer_order_.push_back(kScrollbarLayerId1);
  layer_order_.push_back(kScrollbarLayerId2);
  update1->layer_order = layer_order_;

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::ScrollbarLayerImplBase* layer_impl1 =
      GetScrollbarLayerBaseFromActiveTree(kScrollbarLayerId1);
  cc::ScrollbarLayerImplBase* layer_impl2 =
      GetScrollbarLayerBaseFromActiveTree(kScrollbarLayerId2);
  ASSERT_NE(nullptr, layer_impl1);
  ASSERT_NE(nullptr, layer_impl2);

  EXPECT_FALSE(layer_impl1->is_overlay_scrollbar());
  EXPECT_TRUE(layer_impl2->is_overlay_scrollbar());

  // Test 2: Updating is_overlay_scrollbar mode is respected.
  auto update2 = CreateDefaultUpdate();
  auto layer_props3 = CreateManualLayer(kScrollbarLayerId1, GetParam(),
                                        kDefaultScrollbarLayerBounds);
  auto layer_props4 = CreateManualLayer(kScrollbarLayerId2, GetParam(),
                                        kDefaultScrollbarLayerBounds);

  // Try to swap the values
  GetScrollbarBaseExtra(*layer_props3->layer_extra)->is_overlay_scrollbar =
      true;
  GetScrollbarBaseExtra(*layer_props4->layer_extra)->is_overlay_scrollbar =
      false;

  update2->layers.push_back(std::move(layer_props3));
  update2->layers.push_back(std::move(layer_props4));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());

  EXPECT_TRUE(layer_impl1->is_overlay_scrollbar());
  EXPECT_FALSE(layer_impl2->is_overlay_scrollbar());
}

TEST_P(LayerContextImplUpdateDisplayTreeScrollbarLayerBaseTest,
       UpdateScrollElementId) {
  constexpr int kScrollbarLayerId = 2;
  const cc::ElementId kInitialScrollElementId = kDefaultScrollElementId;
  const cc::ElementId kUpdatedScrollElementId1 = cc::ElementId(12345);
  const cc::ElementId kUpdatedScrollElementId2 = cc::ElementId(54321);

  // Initial update: Create with default scroll_element_id.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), GetParam(), kScrollbarLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::ScrollbarLayerImplBase* layer_impl =
      GetScrollbarLayerBaseFromActiveTree(kScrollbarLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_EQ(layer_impl->scroll_element_id(), kInitialScrollElementId);

  // Second update: Update scroll_element_id.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(kScrollbarLayerId, GetParam(),
                                        kDefaultScrollbarLayerBounds);
  GetScrollbarBaseExtra(*layer_props2->layer_extra)->scroll_element_id =
      kUpdatedScrollElementId1;
  update2->layers.push_back(std::move(layer_props2));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(layer_impl->scroll_element_id(), kUpdatedScrollElementId1);

  // Third update: Update scroll_element_id again.
  auto update3 = CreateDefaultUpdate();
  auto layer_props3 = CreateManualLayer(kScrollbarLayerId, GetParam(),
                                        kDefaultScrollbarLayerBounds);
  GetScrollbarBaseExtra(*layer_props3->layer_extra)->scroll_element_id =
      kUpdatedScrollElementId2;
  update3->layers.push_back(std::move(layer_props3));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  EXPECT_EQ(layer_impl->scroll_element_id(), kUpdatedScrollElementId2);
}

TEST_P(LayerContextImplUpdateDisplayTreeScrollbarLayerBaseTest,
       UpdateIsWebTest) {
  constexpr int kScrollbarLayerId = 2;

  // Initial update: Create with default is_web_test (false).
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), GetParam(), kScrollbarLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::ScrollbarLayerImplBase* layer_impl =
      GetScrollbarLayerBaseFromActiveTree(kScrollbarLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_EQ(layer_impl->is_web_test(), kDefaultIsWebTest);

  // Second update: Set is_web_test to true.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(kScrollbarLayerId, GetParam(),
                                        kDefaultScrollbarLayerBounds);
  GetScrollbarBaseExtra(*layer_props2->layer_extra)->is_web_test = true;
  update2->layers.push_back(std::move(layer_props2));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_TRUE(layer_impl->is_web_test());

  // Third update: Set is_web_test back to false.
  auto update3 = CreateDefaultUpdate();
  auto layer_props3 = CreateManualLayer(kScrollbarLayerId, GetParam(),
                                        kDefaultScrollbarLayerBounds);
  GetScrollbarBaseExtra(*layer_props3->layer_extra)->is_web_test = false;
  update3->layers.push_back(std::move(layer_props3));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  EXPECT_FALSE(layer_impl->is_web_test());
}

TEST_P(LayerContextImplUpdateDisplayTreeScrollbarLayerBaseTest,
       UpdateThumbThicknessScaleFactor) {
  constexpr int kScrollbarLayerId = 2;
  constexpr float kUpdatedFactor = 0.5f;

  // Initial update.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), GetParam(), kScrollbarLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::ScrollbarLayerImplBase* layer_impl =
      GetScrollbarLayerBaseFromActiveTree(kScrollbarLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_EQ(layer_impl->thumb_thickness_scale_factor(),
            kDefaultThumbThicknessScaleFactor);

  // Second update.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(kScrollbarLayerId, GetParam(),
                                        kDefaultScrollbarLayerBounds);
  GetScrollbarBaseExtra(*layer_props2->layer_extra)
      ->thumb_thickness_scale_factor = kUpdatedFactor;
  update2->layers.push_back(std::move(layer_props2));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(layer_impl->thumb_thickness_scale_factor(), kUpdatedFactor);
}

TEST_P(LayerContextImplUpdateDisplayTreeScrollbarLayerBaseTest,
       UpdateCurrentPosAndLengthsAndVerticalAdjustAndTickmarks) {
  constexpr int kScrollbarLayerId = 2;
  constexpr float kUpdatedCurrentPos = 10.f;
  constexpr float kUpdatedClipLayerLength = 100.f;
  constexpr float kUpdatedScrollLayerLength = 200.f;
  constexpr float kUpdatedVerticalAdjust = 5.f;

  // Initial update.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), GetParam(), kScrollbarLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::ScrollbarLayerImplBase* layer_impl =
      GetScrollbarLayerBaseFromActiveTree(kScrollbarLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_EQ(layer_impl->current_pos(), kDefaultCurrentPos);
  EXPECT_EQ(layer_impl->clip_layer_length(), kDefaultClipLayerLength);
  EXPECT_EQ(layer_impl->scroll_layer_length(), kDefaultScrollLayerLength);
  EXPECT_EQ(layer_impl->vertical_adjust(), kDefaultVerticalAdjust);
  EXPECT_EQ(layer_impl->has_find_in_page_tickmarks(),
            kDefaultHasFindInPageTickmarks);

  // Second update.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(kScrollbarLayerId, GetParam(),
                                        kDefaultScrollbarLayerBounds);
  mojom::ScrollbarLayerBaseExtra* base_extra2 =
      GetScrollbarBaseExtra(*layer_props2->layer_extra);
  base_extra2->current_pos = kUpdatedCurrentPos;
  base_extra2->clip_layer_length = kUpdatedClipLayerLength;
  base_extra2->scroll_layer_length = kUpdatedScrollLayerLength;
  base_extra2->vertical_adjust = kUpdatedVerticalAdjust;
  base_extra2->has_find_in_page_tickmarks = true;
  update2->layers.push_back(std::move(layer_props2));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(layer_impl->current_pos(), kUpdatedCurrentPos);
  EXPECT_EQ(layer_impl->clip_layer_length(), kUpdatedClipLayerLength);
  EXPECT_EQ(layer_impl->scroll_layer_length(), kUpdatedScrollLayerLength);
  EXPECT_EQ(layer_impl->vertical_adjust(), kUpdatedVerticalAdjust);
  EXPECT_TRUE(layer_impl->has_find_in_page_tickmarks());
}

INSTANTIATE_TEST_SUITE_P(
    AllScrollbarTypes,
    LayerContextImplUpdateDisplayTreeScrollbarLayerBaseTest,
    testing::Values(cc::mojom::LayerType::kSolidColorScrollbar,
                    cc::mojom::LayerType::kNinePatchThumbScrollbar,
                    cc::mojom::LayerType::kPaintedScrollbar),
    [](const testing::TestParamInfo<
        LayerContextImplUpdateDisplayTreeScrollbarLayerBaseTest::ParamType>&
           info) {
      switch (info.param) {
        case cc::mojom::LayerType::kSolidColorScrollbar:
          return "SolidColorScrollbar";
        case cc::mojom::LayerType::kPaintedScrollbar:
          return "PaintedScrollbar";
        case cc::mojom::LayerType::kNinePatchThumbScrollbar:
          return "NinePatchThumbScrollbar";
        default:
          return "UnknownScrollbarType";
      }
    });

// Test fixture for SolidColorScrollbarLayerImpl specific property updates.
class LayerContextImplUpdateDisplayTreeSolidColorScrollbarLayerTest
    : public LayerContextImplUpdateDisplayTreeScrollbarLayerBaseTest {
 protected:
  cc::SolidColorScrollbarLayerImpl* GetSolidColorScrollbarLayerFromActiveTree(
      int layer_id) {
    cc::LayerImpl* layer = GetLayerFromActiveTree(layer_id);
    if (!layer ||
        layer->GetLayerType() != cc::mojom::LayerType::kSolidColorScrollbar) {
      return nullptr;
    }
    return static_cast<cc::SolidColorScrollbarLayerImpl*>(layer);
  }
};

TEST_F(LayerContextImplUpdateDisplayTreeSolidColorScrollbarLayerTest,
       InitialThumbThickness) {
  constexpr int kScrollbarLayerId = 2;
  constexpr int kInitialThumbThickness = 5;
  constexpr int kUpdatedThumbThickness = 10;

  // Test 1: Create SolidColorScrollbarLayer with a specific
  // thumb_thickness.
  auto update1 = CreateDefaultUpdate();
  auto layer_props1 = CreateManualLayer(
      kScrollbarLayerId, cc::mojom::LayerType::kSolidColorScrollbar,
      kDefaultScrollbarLayerBounds);
  auto& scrollbar_extra1 =
      layer_props1->layer_extra->get_solid_color_scrollbar_layer_extra();
  scrollbar_extra1->thumb_thickness = kInitialThumbThickness;
  update1->layers.push_back(std::move(layer_props1));
  // AddDefaultLayerToUpdate normally handles this. Since we used
  // CreateManualLayer, update the layer_order here.
  layer_order_.push_back(kScrollbarLayerId);
  update1->layer_order = layer_order_;

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::SolidColorScrollbarLayerImpl* layer_impl =
      GetSolidColorScrollbarLayerFromActiveTree(kScrollbarLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_EQ(layer_impl->thumb_thickness(), kInitialThumbThickness);

  // Test 2: Updating the thumb_thickness should have no effect.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(
      kScrollbarLayerId, cc::mojom::LayerType::kSolidColorScrollbar,
      kDefaultScrollbarLayerBounds);
  auto& scrollbar_extra2 =
      layer_props2->layer_extra->get_solid_color_scrollbar_layer_extra();
  scrollbar_extra2->thumb_thickness = kUpdatedThumbThickness;
  update2->layers.push_back(std::move(layer_props2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());

  EXPECT_EQ(layer_impl->thumb_thickness(), kInitialThumbThickness);
}

TEST_F(LayerContextImplUpdateDisplayTreeSolidColorScrollbarLayerTest,
       InitialTrackStart) {
  constexpr int kScrollbarLayerId = 2;
  constexpr int kInitialTrackStart = 2;
  constexpr int kUpdatedTrackStart = 4;

  // Test 1: Create SolidColorScrollbarLayer with a specific track_start.
  auto update1 = CreateDefaultUpdate();
  auto layer_props1 = CreateManualLayer(
      kScrollbarLayerId, cc::mojom::LayerType::kSolidColorScrollbar,
      kDefaultScrollbarLayerBounds);
  auto& scrollbar_extra1 =
      layer_props1->layer_extra->get_solid_color_scrollbar_layer_extra();
  scrollbar_extra1->track_start = kInitialTrackStart;
  update1->layers.push_back(std::move(layer_props1));
  // AddDefaultLayerToUpdate normally handles this. Since we used
  // CreateManualLayer, update the layer_order here.
  layer_order_.push_back(kScrollbarLayerId);
  update1->layer_order = layer_order_;

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::SolidColorScrollbarLayerImpl* layer_impl =
      GetSolidColorScrollbarLayerFromActiveTree(kScrollbarLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_EQ(layer_impl->track_start(), kInitialTrackStart);

  // Test 2: Updating the track_start should have no effect.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(
      kScrollbarLayerId, cc::mojom::LayerType::kSolidColorScrollbar,
      kDefaultScrollbarLayerBounds);
  auto& scrollbar_extra2 =
      layer_props2->layer_extra->get_solid_color_scrollbar_layer_extra();
  scrollbar_extra2->track_start = kUpdatedTrackStart;
  update2->layers.push_back(std::move(layer_props2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(layer_impl->track_start(), kInitialTrackStart);
}

TEST_F(LayerContextImplUpdateDisplayTreeSolidColorScrollbarLayerTest,
       UpdateColor) {
  constexpr int kScrollbarLayerId = 2;
  const SkColor4f kUpdatedScrollbarColor = SkColors::kRed;

  // Initial update: Create SolidColorScrollbarLayer with default color.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(),
                          cc::mojom::LayerType::kSolidColorScrollbar,
                          kScrollbarLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::SolidColorScrollbarLayerImpl* layer_impl =
      GetSolidColorScrollbarLayerFromActiveTree(kScrollbarLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_EQ(layer_impl->color(), kDefaultSolidColorScrollbarColor);  // Default

  // Second update: Update color.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(
      kScrollbarLayerId, cc::mojom::LayerType::kSolidColorScrollbar,
      kDefaultScrollbarLayerBounds);
  auto& scrollbar_extra2 =
      layer_props2->layer_extra->get_solid_color_scrollbar_layer_extra();
  scrollbar_extra2->color = kUpdatedScrollbarColor;
  update2->layers.push_back(std::move(layer_props2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(layer_impl->color(), kUpdatedScrollbarColor);
}

// Test fixture for NinePatchThumbScrollbarLayerImpl specific property updates.
class LayerContextImplUpdateDisplayTreeNinePatchThumbScrollbarLayerTest
    : public LayerContextImplUpdateDisplayTreeScrollbarLayerBaseTest {
 protected:
  cc::NinePatchThumbScrollbarLayerImpl*
  GetNinePatchThumbScrollbarLayerFromActiveTree(int layer_id) {
    cc::LayerImpl* layer = GetLayerFromActiveTree(layer_id);
    if (!layer || layer->GetLayerType() !=
                      cc::mojom::LayerType::kNinePatchThumbScrollbar) {
      return nullptr;
    }
    return static_cast<cc::NinePatchThumbScrollbarLayerImpl*>(layer);
  }
};

TEST_F(LayerContextImplUpdateDisplayTreeNinePatchThumbScrollbarLayerTest,
       UpdateThumbThicknessAndLength) {
  constexpr int kScrollbarLayerId = 2;

  // Initial update: Create with default thickness and length.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(),
                          cc::mojom::LayerType::kNinePatchThumbScrollbar,
                          kScrollbarLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::NinePatchThumbScrollbarLayerImpl* layer_impl =
      GetNinePatchThumbScrollbarLayerFromActiveTree(kScrollbarLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_EQ(layer_impl->thumb_thickness(),
            kDefaultNinePatchThumbScrollbarThumbThickness);
  EXPECT_EQ(layer_impl->thumb_length(),
            kDefaultNinePatchThumbScrollbarThumbLength);

  // Second update: Update thumb_thickness and thumb_length.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(
      kScrollbarLayerId, cc::mojom::LayerType::kNinePatchThumbScrollbar,
      kDefaultScrollbarLayerBounds);
  auto& scrollbar_extra2 =
      layer_props2->layer_extra->get_nine_patch_thumb_scrollbar_layer_extra();
  scrollbar_extra2->thumb_thickness = 5;
  scrollbar_extra2->thumb_length = 20;
  update2->layers.push_back(std::move(layer_props2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(layer_impl->thumb_thickness(), 5);
  EXPECT_EQ(layer_impl->thumb_length(), 20);
}

TEST_F(LayerContextImplUpdateDisplayTreeNinePatchThumbScrollbarLayerTest,
       UpdateTrackStartAndLength) {
  constexpr int kScrollbarLayerId = 2;

  // Initial update: Create with default track_start and track_length.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(),
                          cc::mojom::LayerType::kNinePatchThumbScrollbar,
                          kScrollbarLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::NinePatchThumbScrollbarLayerImpl* layer_impl =
      GetNinePatchThumbScrollbarLayerFromActiveTree(kScrollbarLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_EQ(layer_impl->track_start(),
            kDefaultNinePatchThumbScrollbarTrackStart);
  EXPECT_EQ(layer_impl->track_length(),
            kDefaultNinePatchThumbScrollbarTrackLength);

  // Second update: Update track_start and track_length.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(
      kScrollbarLayerId, cc::mojom::LayerType::kNinePatchThumbScrollbar,
      kDefaultScrollbarLayerBounds);
  auto& scrollbar_extra2 =
      layer_props2->layer_extra->get_nine_patch_thumb_scrollbar_layer_extra();
  scrollbar_extra2->track_start = 2;
  scrollbar_extra2->track_length = 90;
  update2->layers.push_back(std::move(layer_props2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(layer_impl->track_start(), 2);
  EXPECT_EQ(layer_impl->track_length(), 90);
}

TEST_F(LayerContextImplUpdateDisplayTreeNinePatchThumbScrollbarLayerTest,
       UpdateImageBoundsAndAperture) {
  constexpr int kScrollbarLayerId = 2;

  // Initial update: Create with default image_bounds and aperture.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(),
                          cc::mojom::LayerType::kNinePatchThumbScrollbar,
                          kScrollbarLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::NinePatchThumbScrollbarLayerImpl* layer_impl =
      GetNinePatchThumbScrollbarLayerFromActiveTree(kScrollbarLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_EQ(layer_impl->image_bounds(),
            kDefaultNinePatchThumbScrollbarImageBounds);
  EXPECT_EQ(layer_impl->aperture(), kDefaultNinePatchThumbScrollbarAperture);

  // Second update: Update image_bounds and aperture.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(
      kScrollbarLayerId, cc::mojom::LayerType::kNinePatchThumbScrollbar,
      kDefaultScrollbarLayerBounds);
  auto& scrollbar_extra2 =
      layer_props2->layer_extra->get_nine_patch_thumb_scrollbar_layer_extra();
  scrollbar_extra2->image_bounds = gfx::Size(30, 30);
  scrollbar_extra2->aperture = gfx::Rect(5, 5, 20, 20);
  update2->layers.push_back(std::move(layer_props2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(layer_impl->image_bounds(), gfx::Size(30, 30));
  EXPECT_EQ(layer_impl->aperture(), gfx::Rect(5, 5, 20, 20));
}

TEST_F(LayerContextImplUpdateDisplayTreeNinePatchThumbScrollbarLayerTest,
       UpdateUIResourceIds) {
  constexpr int kScrollbarLayerId = 2;
  constexpr cc::UIResourceId kThumbId = 101;
  constexpr cc::UIResourceId kTrackId = 102;

  // Initial update: Create with default resource IDs.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(),
                          cc::mojom::LayerType::kNinePatchThumbScrollbar,
                          kScrollbarLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::NinePatchThumbScrollbarLayerImpl* layer_impl =
      GetNinePatchThumbScrollbarLayerFromActiveTree(kScrollbarLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_EQ(layer_impl->thumb_ui_resource_id(),
            kDefaultNinePatchThumbScrollbarThumbUIResourceId);
  EXPECT_EQ(layer_impl->track_and_buttons_ui_resource_id(),
            kDefaultNinePatchThumbScrollbarTrackAndButtonsUIResourceId);

  // Second update: Update resource IDs.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(
      kScrollbarLayerId, cc::mojom::LayerType::kNinePatchThumbScrollbar,
      kDefaultScrollbarLayerBounds);
  auto& scrollbar_extra2 =
      layer_props2->layer_extra->get_nine_patch_thumb_scrollbar_layer_extra();
  scrollbar_extra2->thumb_ui_resource_id = kThumbId;
  scrollbar_extra2->track_and_buttons_ui_resource_id = kTrackId;
  update2->layers.push_back(std::move(layer_props2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(layer_impl->thumb_ui_resource_id(), kThumbId);
  EXPECT_EQ(layer_impl->track_and_buttons_ui_resource_id(), kTrackId);
}

// Test fixture for PaintedScrollbarLayerImpl specific property updates.
class LayerContextImplUpdateDisplayTreePaintedScrollbarLayerTest
    : public LayerContextImplUpdateDisplayTreeScrollbarLayerBaseTest {
 protected:
  cc::PaintedScrollbarLayerImpl* GetPaintedScrollbarLayerFromActiveTree(
      int layer_id) {
    cc::LayerImpl* layer = GetLayerFromActiveTree(layer_id);
    if (!layer ||
        layer->GetLayerType() != cc::mojom::LayerType::kPaintedScrollbar) {
      return nullptr;
    }
    return static_cast<cc::PaintedScrollbarLayerImpl*>(layer);
  }
};

TEST_F(LayerContextImplUpdateDisplayTreePaintedScrollbarLayerTest,
       UpdateInternalContentsScaleAndBounds) {
  constexpr int kScrollbarLayerId = 2;
  constexpr float kUpdatedInternalContentsScale = 1.5f;
  const gfx::Size kUpdatedInternalContentBounds(15, 150);

  // Initial update: Create with default values.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(),
                          cc::mojom::LayerType::kPaintedScrollbar,
                          kScrollbarLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::PaintedScrollbarLayerImpl* layer_impl =
      GetPaintedScrollbarLayerFromActiveTree(kScrollbarLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_EQ(layer_impl->internal_contents_scale(),
            kDefaultPaintedScrollbarInternalContentsScale);
  EXPECT_EQ(layer_impl->internal_content_bounds(),
            kDefaultPaintedScrollbarInternalContentBounds);

  // Second update: Update internal_contents_scale and internal_content_bounds.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(kScrollbarLayerId,
                                        cc::mojom::LayerType::kPaintedScrollbar,
                                        kDefaultScrollbarLayerBounds);
  auto& scrollbar_extra2 =
      layer_props2->layer_extra->get_painted_scrollbar_layer_extra();
  scrollbar_extra2->internal_contents_scale = kUpdatedInternalContentsScale;
  scrollbar_extra2->internal_content_bounds = kUpdatedInternalContentBounds;
  update2->layers.push_back(std::move(layer_props2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(layer_impl->internal_contents_scale(),
            kUpdatedInternalContentsScale);
  EXPECT_EQ(layer_impl->internal_content_bounds(),
            kUpdatedInternalContentBounds);
}

TEST_F(LayerContextImplUpdateDisplayTreePaintedScrollbarLayerTest,
       UpdateThumbThicknessAndLength) {
  constexpr int kScrollbarLayerId = 2;
  constexpr int kUpdatedThumbThickness = 5;
  constexpr int kUpdatedThumbLength = 20;

  // Initial update.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(),
                          cc::mojom::LayerType::kPaintedScrollbar,
                          kScrollbarLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::PaintedScrollbarLayerImpl* layer_impl =
      GetPaintedScrollbarLayerFromActiveTree(kScrollbarLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_EQ(layer_impl->thumb_thickness(),
            kDefaultPaintedScrollbarThumbThickness);
  EXPECT_EQ(layer_impl->thumb_length(), kDefaultPaintedScrollbarThumbLength);

  // Second update.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(kScrollbarLayerId,
                                        cc::mojom::LayerType::kPaintedScrollbar,
                                        kDefaultScrollbarLayerBounds);
  auto& scrollbar_extra2 =
      layer_props2->layer_extra->get_painted_scrollbar_layer_extra();
  scrollbar_extra2->thumb_thickness = kUpdatedThumbThickness;
  scrollbar_extra2->thumb_length = kUpdatedThumbLength;
  update2->layers.push_back(std::move(layer_props2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(layer_impl->thumb_thickness(), kUpdatedThumbThickness);
  EXPECT_EQ(layer_impl->thumb_length(), kUpdatedThumbLength);
}

TEST_F(LayerContextImplUpdateDisplayTreePaintedScrollbarLayerTest,
       UpdateButtonAndTrackRects) {
  constexpr int kScrollbarLayerId = 2;
  const gfx::Rect kUpdatedBackButtonRect(0, 0, 10, 10);
  const gfx::Rect kUpdatedForwardButtonRect(0, 90, 10, 10);
  const gfx::Rect kUpdatedTrackRect(0, 10, 10, 80);

  // Initial update.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(),
                          cc::mojom::LayerType::kPaintedScrollbar,
                          kScrollbarLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::PaintedScrollbarLayerImpl* layer_impl =
      GetPaintedScrollbarLayerFromActiveTree(kScrollbarLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_EQ(layer_impl->back_button_rect(),
            kDefaultPaintedScrollbarBackButtonRect);
  EXPECT_EQ(layer_impl->forward_button_rect(),
            kDefaultPaintedScrollbarForwardButtonRect);
  EXPECT_EQ(layer_impl->track_rect(), kDefaultPaintedScrollbarTrackRect);

  // Second update.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(kScrollbarLayerId,
                                        cc::mojom::LayerType::kPaintedScrollbar,
                                        kDefaultScrollbarLayerBounds);
  auto& scrollbar_extra2 =
      layer_props2->layer_extra->get_painted_scrollbar_layer_extra();
  scrollbar_extra2->back_button_rect = kUpdatedBackButtonRect;
  scrollbar_extra2->forward_button_rect = kUpdatedForwardButtonRect;
  scrollbar_extra2->track_rect = kUpdatedTrackRect;
  update2->layers.push_back(std::move(layer_props2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(layer_impl->back_button_rect(), kUpdatedBackButtonRect);
  EXPECT_EQ(layer_impl->forward_button_rect(), kUpdatedForwardButtonRect);
  EXPECT_EQ(layer_impl->track_rect(), kUpdatedTrackRect);
}

TEST_F(LayerContextImplUpdateDisplayTreePaintedScrollbarLayerTest,
       UpdatePaintedOpacityAndThumbColor) {
  constexpr int kScrollbarLayerId = 2;
  constexpr float kUpdatedPaintedOpacity = 0.5f;
  const SkColor4f kUpdatedThumbColor = SkColors::kGreen;

  // Initial update.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(),
                          cc::mojom::LayerType::kPaintedScrollbar,
                          kScrollbarLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::PaintedScrollbarLayerImpl* layer_impl =
      GetPaintedScrollbarLayerFromActiveTree(kScrollbarLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_EQ(layer_impl->painted_opacity(),
            kDefaultPaintedScrollbarPaintedOpacity);
  EXPECT_EQ(layer_impl->thumb_color(), kDefaultPaintedScrollbarThumbColor);

  // Second update.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(kScrollbarLayerId,
                                        cc::mojom::LayerType::kPaintedScrollbar,
                                        kDefaultScrollbarLayerBounds);
  auto& scrollbar_extra2 =
      layer_props2->layer_extra->get_painted_scrollbar_layer_extra();
  scrollbar_extra2->painted_opacity = kUpdatedPaintedOpacity;
  scrollbar_extra2->thumb_color = kUpdatedThumbColor;
  update2->layers.push_back(std::move(layer_props2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(layer_impl->painted_opacity(), kUpdatedPaintedOpacity);
  ASSERT_TRUE(layer_impl->thumb_color().has_value());
  EXPECT_EQ(layer_impl->thumb_color().value(), kUpdatedThumbColor);

  // Third update: Clearing thumb_color is not supported by
  // PaintedScrollbarLayerImpl. Check that it has no effect.
  auto update3 = CreateDefaultUpdate();
  auto layer_props3 = CreateManualLayer(kScrollbarLayerId,
                                        cc::mojom::LayerType::kPaintedScrollbar,
                                        kDefaultScrollbarLayerBounds);
  auto& scrollbar_extra3 =
      layer_props3->layer_extra->get_painted_scrollbar_layer_extra();
  scrollbar_extra3->thumb_color = std::nullopt;  // Explicitly clear
  update3->layers.push_back(std::move(layer_props3));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  EXPECT_TRUE(layer_impl->thumb_color().has_value());
  EXPECT_EQ(layer_impl->thumb_color().value(), kUpdatedThumbColor);
}

TEST_F(LayerContextImplUpdateDisplayTreePaintedScrollbarLayerTest,
       UpdateJumpOnTrackClick) {
  constexpr int kScrollbarLayerId = 2;
  constexpr bool kUpdatedJumpOnTrackClick = true;

  // Initial update.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(),
                          cc::mojom::LayerType::kPaintedScrollbar,
                          kScrollbarLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::PaintedScrollbarLayerImpl* layer_impl =
      GetPaintedScrollbarLayerFromActiveTree(kScrollbarLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_EQ(layer_impl->jump_on_track_click(),
            kDefaultPaintedScrollbarJumpOnTrackClick);

  // Second update.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(kScrollbarLayerId,
                                        cc::mojom::LayerType::kPaintedScrollbar,
                                        kDefaultScrollbarLayerBounds);
  auto& scrollbar_extra2 =
      layer_props2->layer_extra->get_painted_scrollbar_layer_extra();
  scrollbar_extra2->jump_on_track_click = kUpdatedJumpOnTrackClick;
  update2->layers.push_back(std::move(layer_props2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_TRUE(layer_impl->jump_on_track_click());
}

TEST_F(LayerContextImplUpdateDisplayTreePaintedScrollbarLayerTest,
       UpdateSupportsDragSnapBack) {
  constexpr int kScrollbarLayerId = 2;
  constexpr bool kUpdatedSupportsDragSnapBack = true;

  // Initial update.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(),
                          cc::mojom::LayerType::kPaintedScrollbar,
                          kScrollbarLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::PaintedScrollbarLayerImpl* layer_impl =
      GetPaintedScrollbarLayerFromActiveTree(kScrollbarLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_EQ(layer_impl->supports_drag_snap_back(),
            kDefaultPaintedScrollbarSupportsDragSnapBack);

  // Second update.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(kScrollbarLayerId,
                                        cc::mojom::LayerType::kPaintedScrollbar,
                                        kDefaultScrollbarLayerBounds);
  auto& scrollbar_extra2 =
      layer_props2->layer_extra->get_painted_scrollbar_layer_extra();
  scrollbar_extra2->supports_drag_snap_back = kUpdatedSupportsDragSnapBack;
  update2->layers.push_back(std::move(layer_props2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_TRUE(layer_impl->supports_drag_snap_back());
}

TEST_F(LayerContextImplUpdateDisplayTreePaintedScrollbarLayerTest,
       UpdateUIResourceIds) {
  constexpr int kScrollbarLayerId = 2;
  constexpr cc::UIResourceId kThumbId = 101;
  constexpr cc::UIResourceId kTrackId = 102;

  // Initial update.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(),
                          cc::mojom::LayerType::kPaintedScrollbar,
                          kScrollbarLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::PaintedScrollbarLayerImpl* layer_impl =
      GetPaintedScrollbarLayerFromActiveTree(kScrollbarLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_EQ(layer_impl->thumb_ui_resource_id(),
            kDefaultPaintedScrollbarThumbUIResourceId);
  EXPECT_EQ(layer_impl->track_and_buttons_ui_resource_id(),
            kDefaultPaintedScrollbarTrackAndButtonsUIResourceId);

  // Second update.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(kScrollbarLayerId,
                                        cc::mojom::LayerType::kPaintedScrollbar,
                                        kDefaultScrollbarLayerBounds);
  auto& scrollbar_extra2 =
      layer_props2->layer_extra->get_painted_scrollbar_layer_extra();
  scrollbar_extra2->thumb_ui_resource_id = kThumbId;
  scrollbar_extra2->track_and_buttons_ui_resource_id = kTrackId;
  update2->layers.push_back(std::move(layer_props2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(layer_impl->thumb_ui_resource_id(), kThumbId);
  EXPECT_EQ(layer_impl->track_and_buttons_ui_resource_id(), kTrackId);
}

TEST_F(LayerContextImplUpdateDisplayTreePaintedScrollbarLayerTest,
       UpdateUsesNinePatchTrackAndButtons) {
  constexpr int kScrollbarLayerId = 2;
  constexpr bool kUpdatedUsesNinePatch = true;

  // Initial update.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(),
                          cc::mojom::LayerType::kPaintedScrollbar,
                          kScrollbarLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::PaintedScrollbarLayerImpl* layer_impl =
      GetPaintedScrollbarLayerFromActiveTree(kScrollbarLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_EQ(layer_impl->uses_nine_patch_track_and_buttons(),
            kDefaultPaintedScrollbarUsesNinePatchTrackAndButtons);

  // Second update.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(kScrollbarLayerId,
                                        cc::mojom::LayerType::kPaintedScrollbar,
                                        kDefaultScrollbarLayerBounds);
  auto& scrollbar_extra2 =
      layer_props2->layer_extra->get_painted_scrollbar_layer_extra();
  scrollbar_extra2->uses_nine_patch_track_and_buttons = kUpdatedUsesNinePatch;
  update2->layers.push_back(std::move(layer_props2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_TRUE(layer_impl->uses_nine_patch_track_and_buttons());
}

TEST_F(LayerContextImplUpdateDisplayTreePaintedScrollbarLayerTest,
       UpdateTrackAndButtonsImageBoundsAndAperture) {
  constexpr int kScrollbarLayerId = 2;
  const gfx::Size kUpdatedImageBounds(50, 50);
  const gfx::Rect kUpdatedAperture(10, 10, 30, 30);

  // Initial update.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(),
                          cc::mojom::LayerType::kPaintedScrollbar,
                          kScrollbarLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::PaintedScrollbarLayerImpl* layer_impl =
      GetPaintedScrollbarLayerFromActiveTree(kScrollbarLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_EQ(layer_impl->track_and_buttons_image_bounds(),
            kDefaultPaintedScrollbarTrackAndButtonsImageBounds);
  EXPECT_EQ(layer_impl->track_and_buttons_aperture(),
            kDefaultPaintedScrollbarTrackAndButtonsAperture);

  // Second update.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(kScrollbarLayerId,
                                        cc::mojom::LayerType::kPaintedScrollbar,
                                        kDefaultScrollbarLayerBounds);
  auto& scrollbar_extra2 =
      layer_props2->layer_extra->get_painted_scrollbar_layer_extra();
  scrollbar_extra2->track_and_buttons_image_bounds = kUpdatedImageBounds;
  scrollbar_extra2->track_and_buttons_aperture = kUpdatedAperture;
  update2->layers.push_back(std::move(layer_props2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(layer_impl->track_and_buttons_image_bounds(), kUpdatedImageBounds);
  EXPECT_EQ(layer_impl->track_and_buttons_aperture(), kUpdatedAperture);
}

TEST_F(LayerContextImplUpdateDisplayTreePaintedScrollbarLayerTest,
       UpdateScrollElementId) {
  constexpr int kScrollbarLayerId = 2;
  const cc::ElementId kScrollElementId1 = cc::ElementId(12345);
  const cc::ElementId kScrollElementId2 = cc::ElementId(54321);

  // Initial update.
  auto update1 = CreateDefaultUpdate();
  auto layer_props1 = CreateManualLayer(kScrollbarLayerId,
                                        cc::mojom::LayerType::kPaintedScrollbar,
                                        gfx::Size(10, 100));
  auto& scrollbar_extra1 =
      layer_props1->layer_extra->get_painted_scrollbar_layer_extra();
  scrollbar_extra1->scrollbar_base_extra->scroll_element_id = kScrollElementId1;
  update1->layers.push_back(std::move(layer_props1));
  layer_order_.push_back(kScrollbarLayerId);
  update1->layer_order = layer_order_;

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::PaintedScrollbarLayerImpl* layer_impl =
      GetPaintedScrollbarLayerFromActiveTree(kScrollbarLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_EQ(layer_impl->scroll_element_id(), kScrollElementId1);

  // Second update.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(kScrollbarLayerId,
                                        cc::mojom::LayerType::kPaintedScrollbar,
                                        kDefaultScrollbarLayerBounds);
  auto& scrollbar_extra2 =
      layer_props2->layer_extra->get_painted_scrollbar_layer_extra();
  scrollbar_extra2->scrollbar_base_extra->scroll_element_id = kScrollElementId2;
  update2->layers.push_back(std::move(layer_props2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(layer_impl->scroll_element_id(), kScrollElementId2);
}

TEST_F(LayerContextImplUpdateDisplayTreePaintedScrollbarLayerTest,
       UpdateIsWebTest) {
  constexpr int kScrollbarLayerId = 2;
  constexpr bool kUpdatedIsWebTest = true;

  // Initial update.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(),
                          cc::mojom::LayerType::kPaintedScrollbar,
                          kScrollbarLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::PaintedScrollbarLayerImpl* layer_impl =
      GetPaintedScrollbarLayerFromActiveTree(kScrollbarLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_FALSE(layer_impl->is_web_test());  // Default

  // Second update.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(kScrollbarLayerId,
                                        cc::mojom::LayerType::kPaintedScrollbar,
                                        kDefaultScrollbarLayerBounds);
  auto& scrollbar_extra2 =
      layer_props2->layer_extra->get_painted_scrollbar_layer_extra();
  scrollbar_extra2->scrollbar_base_extra->is_web_test = kUpdatedIsWebTest;
  update2->layers.push_back(std::move(layer_props2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_TRUE(layer_impl->is_web_test());
}

TEST_F(LayerContextImplUpdateDisplayTreePaintedScrollbarLayerTest,
       UpdateThumbThicknessScaleFactor) {
  constexpr int kScrollbarLayerId = 2;
  constexpr float kUpdatedThumbThicknessScaleFactor = 0.5f;

  // Initial update.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(),
                          cc::mojom::LayerType::kPaintedScrollbar,
                          kScrollbarLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::PaintedScrollbarLayerImpl* layer_impl =
      GetPaintedScrollbarLayerFromActiveTree(kScrollbarLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_EQ(layer_impl->thumb_thickness_scale_factor(), 0.f);  // Default

  // Second update.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(kScrollbarLayerId,
                                        cc::mojom::LayerType::kPaintedScrollbar,
                                        kDefaultScrollbarLayerBounds);
  auto& scrollbar_extra2 =
      layer_props2->layer_extra->get_painted_scrollbar_layer_extra();
  scrollbar_extra2->scrollbar_base_extra->thumb_thickness_scale_factor =
      kUpdatedThumbThicknessScaleFactor;
  update2->layers.push_back(std::move(layer_props2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(layer_impl->thumb_thickness_scale_factor(),
            kUpdatedThumbThicknessScaleFactor);
}

TEST_F(LayerContextImplUpdateDisplayTreePaintedScrollbarLayerTest,
       UpdateCurrentPosAndLengths) {
  constexpr int kScrollbarLayerId = 2;
  constexpr float kUpdatedCurrentPos = 10.f;
  constexpr float kUpdatedClipLayerLength = 100.f;
  constexpr float kUpdatedScrollLayerLength = 200.f;

  // Initial update.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(),
                          cc::mojom::LayerType::kPaintedScrollbar,
                          kScrollbarLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::PaintedScrollbarLayerImpl* layer_impl =
      GetPaintedScrollbarLayerFromActiveTree(kScrollbarLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_EQ(layer_impl->current_pos(), 0.f);
  EXPECT_EQ(layer_impl->clip_layer_length(), 0.f);
  EXPECT_EQ(layer_impl->scroll_layer_length(), 0.f);

  // Second update.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(kScrollbarLayerId,
                                        cc::mojom::LayerType::kPaintedScrollbar,
                                        kDefaultScrollbarLayerBounds);
  auto& scrollbar_extra2 =
      layer_props2->layer_extra->get_painted_scrollbar_layer_extra();
  scrollbar_extra2->scrollbar_base_extra->current_pos = kUpdatedCurrentPos;
  scrollbar_extra2->scrollbar_base_extra->clip_layer_length =
      kUpdatedClipLayerLength;
  scrollbar_extra2->scrollbar_base_extra->scroll_layer_length =
      kUpdatedScrollLayerLength;
  update2->layers.push_back(std::move(layer_props2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(layer_impl->current_pos(), kUpdatedCurrentPos);
  EXPECT_EQ(layer_impl->clip_layer_length(), kUpdatedClipLayerLength);
  EXPECT_EQ(layer_impl->scroll_layer_length(), kUpdatedScrollLayerLength);
}

TEST_F(LayerContextImplUpdateDisplayTreePaintedScrollbarLayerTest,
       UpdateVerticalAdjustAndHasFindInPageTickmarks) {
  constexpr int kScrollbarLayerId = 2;
  constexpr float kUpdatedVerticalAdjust = 5.f;
  constexpr bool kUpdatedHasFindInPageTickmarks = true;

  // Initial update.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(),
                          cc::mojom::LayerType::kPaintedScrollbar,
                          kScrollbarLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::PaintedScrollbarLayerImpl* layer_impl =
      GetPaintedScrollbarLayerFromActiveTree(kScrollbarLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_EQ(layer_impl->vertical_adjust(), 0.f);
  EXPECT_FALSE(layer_impl->has_find_in_page_tickmarks());

  // Second update.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(kScrollbarLayerId,
                                        cc::mojom::LayerType::kPaintedScrollbar,
                                        kDefaultScrollbarLayerBounds);
  auto& scrollbar_extra2 =
      layer_props2->layer_extra->get_painted_scrollbar_layer_extra();
  scrollbar_extra2->scrollbar_base_extra->vertical_adjust =
      kUpdatedVerticalAdjust;
  scrollbar_extra2->scrollbar_base_extra->has_find_in_page_tickmarks =
      kUpdatedHasFindInPageTickmarks;
  update2->layers.push_back(std::move(layer_props2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(layer_impl->vertical_adjust(), kUpdatedVerticalAdjust);
  EXPECT_TRUE(layer_impl->has_find_in_page_tickmarks());
}

// Test fixture for MirrorLayerImpl specific property updates.
class LayerContextImplUpdateDisplayTreeMirrorLayerTest
    : public LayerContextImplLayerLifecycleTest {
 protected:
  cc::MirrorLayerImpl* GetMirrorLayerFromActiveTree(int layer_id) {
    cc::LayerImpl* layer = GetLayerFromActiveTree(layer_id);
    if (!layer || layer->GetLayerType() != cc::mojom::LayerType::kMirror) {
      return nullptr;
    }
    return static_cast<cc::MirrorLayerImpl*>(layer);
  }
};

TEST_F(LayerContextImplUpdateDisplayTreeMirrorLayerTest,
       UpdateMirroredLayerId) {
  constexpr int kMirrorLayerId = 2;
  constexpr int kMirroredLayerId1 = 1;  // Mirroring the root layer by default.
  constexpr int kMirroredLayerId2 = 3;

  // Initial update: Create MirrorLayer with default mirrored_layer_id.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), cc::mojom::LayerType::kMirror,
                          kMirrorLayerId);
  // Create the layer that will be mirrored.
  AddDefaultLayerToUpdate(update1.get(), cc::mojom::LayerType::kLayer,
                          kMirroredLayerId2);

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::MirrorLayerImpl* layer_impl =
      GetMirrorLayerFromActiveTree(kMirrorLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_EQ(layer_impl->mirrored_layer_id(),
            kDefaultMirrorLayerMirroredLayerId);

  // Second update: Update mirrored_layer_id to kMirroredLayerId1.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 =
      CreateManualLayer(kMirrorLayerId, cc::mojom::LayerType::kMirror);
  auto& mirror_extra2 = layer_props2->layer_extra->get_mirror_layer_extra();
  mirror_extra2->mirrored_layer_id = kMirroredLayerId1;
  update2->layers.push_back(std::move(layer_props2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(layer_impl->mirrored_layer_id(), kMirroredLayerId1);

  // Third update: Update mirrored_layer_id to kMirroredLayerId2.
  auto update3 = CreateDefaultUpdate();
  auto layer_props3 =
      CreateManualLayer(kMirrorLayerId, cc::mojom::LayerType::kMirror);
  auto& mirror_extra3 = layer_props3->layer_extra->get_mirror_layer_extra();
  mirror_extra3->mirrored_layer_id = kMirroredLayerId2;
  update3->layers.push_back(std::move(layer_props3));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  EXPECT_EQ(layer_impl->mirrored_layer_id(), kMirroredLayerId2);
}

// Test fixture for ViewTransitionContentLayerImpl specific property updates.
class LayerContextImplUpdateDisplayTreeViewTransitionContentLayerTest
    : public LayerContextImplLayerLifecycleTest {
 protected:
  cc::ViewTransitionContentLayerImpl*
  GetViewTransitionContentLayerFromActiveTree(int layer_id) {
    cc::LayerImpl* layer = GetLayerFromActiveTree(layer_id);
    if (!layer ||
        layer->GetLayerType() != cc::mojom::LayerType::kViewTransitionContent) {
      return nullptr;
    }
    return static_cast<cc::ViewTransitionContentLayerImpl*>(layer);
  }
};

TEST_F(LayerContextImplUpdateDisplayTreeViewTransitionContentLayerTest,
       InitialResourceId) {
  constexpr int kVTContentLayerId = 2;
  const ViewTransitionElementResourceId kInitialResourceId(
      blink::ViewTransitionToken(), 3, true);
  const ViewTransitionElementResourceId kAttemptedUpdateResourceId1(
      blink::ViewTransitionToken(), 1, false);

  // Initial update: Create with a specific, non-default resource_id.
  auto update1 = CreateDefaultUpdate();
  auto layer_props1 = CreateManualLayer(
      kVTContentLayerId, cc::mojom::LayerType::kViewTransitionContent);
  auto& vt_extra1 =
      layer_props1->layer_extra->get_view_transition_content_layer_extra();
  vt_extra1->resource_id = kInitialResourceId;
  update1->layers.push_back(std::move(layer_props1));
  // AddDefaultLayerToUpdate normally handles this. Since we used
  // CreateManualLayer, update the layer_order here.
  layer_order_.push_back(kVTContentLayerId);
  update1->layer_order = layer_order_;

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::ViewTransitionContentLayerImpl* layer_impl =
      GetViewTransitionContentLayerFromActiveTree(kVTContentLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_EQ(layer_impl->ViewTransitionResourceId(), kInitialResourceId);

  // Second update: Attempt to update resource_id. This should have no effect
  // as resource_id is a constructor parameter for the Impl layer.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(
      kVTContentLayerId, cc::mojom::LayerType::kViewTransitionContent);
  // We need to set the resource_id on the update struct, even though it won't
  // change the impl layer, to reflect what the client might send.
  auto& vt_extra2 =
      layer_props2->layer_extra->get_view_transition_content_layer_extra();
  vt_extra2->resource_id = kAttemptedUpdateResourceId1;
  update2->layers.push_back(std::move(layer_props2));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  // Expect the resource_id to remain the initially set one.
  EXPECT_EQ(layer_impl->ViewTransitionResourceId(), kInitialResourceId);
}

TEST_F(LayerContextImplUpdateDisplayTreeViewTransitionContentLayerTest,
       UpdateIsLiveContentLayer) {
  constexpr int kVTContentLayerId = 2;
  constexpr bool kInitialIsLiveContentLayer = true;
  constexpr bool kAttemptedUpdateIsLiveContentLayer = false;

  // Initial update: Create with a specific, non-default is_live_content_layer.
  auto update1 = CreateDefaultUpdate();
  auto layer_props1 = CreateManualLayer(
      kVTContentLayerId, cc::mojom::LayerType::kViewTransitionContent);
  auto& vt_extra1 =
      layer_props1->layer_extra->get_view_transition_content_layer_extra();
  vt_extra1->is_live_content_layer = kInitialIsLiveContentLayer;
  update1->layers.push_back(std::move(layer_props1));
  // AddDefaultLayerToUpdate normally handles this. Since we used
  // CreateManualLayer, update the layer_order here.
  layer_order_.push_back(kVTContentLayerId);
  update1->layer_order = layer_order_;

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::ViewTransitionContentLayerImpl* layer_impl =
      GetViewTransitionContentLayerFromActiveTree(kVTContentLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_EQ(layer_impl->is_live_content_layer(), kInitialIsLiveContentLayer);

  // Second update: Attempt to update is_live_content_layer. This should have
  // no effect as is_live_content_layer is a constructor parameter for the Impl
  // layer.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(
      kVTContentLayerId, cc::mojom::LayerType::kViewTransitionContent);
  auto& vt_extra2 =
      layer_props2->layer_extra->get_view_transition_content_layer_extra();
  // We need to set the is_live_content_layer on the update struct, even though
  // it won't change the impl layer, to reflect what the client might send.
  vt_extra2->is_live_content_layer = kAttemptedUpdateIsLiveContentLayer;
  update2->layers.push_back(std::move(layer_props2));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  // Expect the is_live_content_layer to remain the initially set one.
  EXPECT_EQ(layer_impl->is_live_content_layer(), kInitialIsLiveContentLayer);
}

TEST_F(LayerContextImplUpdateDisplayTreeViewTransitionContentLayerTest,
       UpdateMaxExtentsRect) {
  constexpr int kVTContentLayerId = 2;
  const gfx::RectF kMaxExtentsRect1(10.f, 10.f, 80.f, 80.f);
  const gfx::RectF kMaxExtentsRect2(5.f, 5.f, 90.f, 90.f);

  // Initial update: Create with default max_extents_rect.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(),
                          cc::mojom::LayerType::kViewTransitionContent,
                          kVTContentLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::ViewTransitionContentLayerImpl* layer_impl =
      GetViewTransitionContentLayerFromActiveTree(kVTContentLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_EQ(layer_impl->max_extents_rect(),
            kDefaultViewTransitionContentLayerMaxExtentsRect);

  // Second update: Update max_extents_rect.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(
      kVTContentLayerId, cc::mojom::LayerType::kViewTransitionContent);
  auto& vt_extra2 =
      layer_props2->layer_extra->get_view_transition_content_layer_extra();
  vt_extra2->max_extents_rect = kMaxExtentsRect1;
  update2->layers.push_back(std::move(layer_props2));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(layer_impl->max_extents_rect(), kMaxExtentsRect1);

  // Third update: Update max_extents_rect again.
  auto update3 = CreateDefaultUpdate();
  auto layer_props3 = CreateManualLayer(
      kVTContentLayerId, cc::mojom::LayerType::kViewTransitionContent);
  auto& vt_extra3 =
      layer_props3->layer_extra->get_view_transition_content_layer_extra();
  vt_extra3->max_extents_rect = kMaxExtentsRect2;
  update3->layers.push_back(std::move(layer_props3));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  EXPECT_EQ(layer_impl->max_extents_rect(), kMaxExtentsRect2);

  // Fourth update: Update max_extents_rect back to default (empty).
  auto update4 = CreateDefaultUpdate();
  auto layer_props4 = CreateManualLayer(
      kVTContentLayerId, cc::mojom::LayerType::kViewTransitionContent);
  // Default is already set by CreateDefaultLayerExtra.
  update4->layers.push_back(std::move(layer_props4));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update4)).has_value());
  EXPECT_EQ(layer_impl->max_extents_rect(),
            kDefaultViewTransitionContentLayerMaxExtentsRect);
}

TEST_F(LayerContextImplTest, UpdateDisplayTreeWithTargetLocalSurfaceId) {
  auto update = CreateDefaultUpdate();
  const LocalSurfaceId target_local_surface_id(
      1, base::UnguessableToken::CreateForTesting(2, 3));
  update->target_local_surface_id = target_local_surface_id;

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(layer_context_impl_->host_impl()->target_local_surface_id(),
            target_local_surface_id);
}

TEST_F(LayerContextImplTest, UpdateDisplayTreeWithTargetSurfaceRanges) {
  const SurfaceRange ranges[] = {
      {SurfaceId(kDefaultFrameSinkId,
                 {1, base::UnguessableToken::CreateForTesting(2, 3)}),
       SurfaceId(kDefaultFrameSinkId,
                 {10, base::UnguessableToken::CreateForTesting(11, 12)})},
      {SurfaceId(kDefaultFrameSinkId,
                 {4, base::UnguessableToken::CreateForTesting(5, 6)}),
       SurfaceId(kDefaultFrameSinkId,
                 {13, base::UnguessableToken::CreateForTesting(14, 15)})},
      {SurfaceId(kDefaultFrameSinkId,
                 {7, base::UnguessableToken::CreateForTesting(8, 9)}),
       SurfaceId(kDefaultFrameSinkId,
                 {16, base::UnguessableToken::CreateForTesting(17, 18)})}};

  auto update = CreateDefaultUpdate();
  update->surface_ranges.emplace({ranges[0], ranges[1]});

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(
      layer_context_impl_->host_impl()->active_tree()->SurfaceRanges().size(),
      2U);
  EXPECT_TRUE(
      layer_context_impl_->host_impl()->active_tree()->SurfaceRanges().contains(
          ranges[0]));
  EXPECT_TRUE(
      layer_context_impl_->host_impl()->active_tree()->SurfaceRanges().contains(
          ranges[1]));
  EXPECT_FALSE(
      layer_context_impl_->host_impl()->active_tree()->SurfaceRanges().contains(
          ranges[2]));
}

TEST_F(LayerContextImplTest, UpdateDisplayTreeWithNextFrameToken) {
  auto update = CreateDefaultUpdate();
  const uint32_t kTestNextFrameToken = 12345;
  update->next_frame_token = kTestNextFrameToken;

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(layer_context_impl_->host_impl()->next_frame_token(),
            kTestNextFrameToken);
}

TEST_F(LayerContextImplTest, UpdateDisplayTreeWithSendFrameTokenToEmbedder) {
  auto update = CreateDefaultUpdate();
  const bool kTestSendFrameTokenToEmbedder = true;
  update->send_frame_token_to_embedder = kTestSendFrameTokenToEmbedder;

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(layer_context_impl_->host_impl()->send_frame_token_to_embedder(),
            kTestSendFrameTokenToEmbedder);
}

TEST_F(LayerContextImplTest, UpdateDisplayTreeWithFullTreeDamaged) {
  auto update = CreateDefaultUpdate();
  const bool kTestFullTreeDamaged = true;
  update->full_tree_damaged = kTestFullTreeDamaged;

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  cc::LayerTreeImpl* active_tree =
      layer_context_impl_->host_impl()->active_tree();
  gfx::Rect viewport_rect = active_tree->GetDeviceViewport();
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(
      viewport_rect,
      layer_context_impl_->host_impl()->viewport_damage_rect_for_testing());
}

class LayerContextImplUpdateDisplayTreeInvalidValuesTest
    : public LayerContextImplTest,
      public ::testing::WithParamInterface<float> {};

TEST_P(LayerContextImplUpdateDisplayTreeInvalidValuesTest, PageScaleFactor) {
  const float invalid_value = GetParam();
  auto update = CreateDefaultUpdate();
  update->page_scale_factor = invalid_value;

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid page scale factors");
}

TEST_P(LayerContextImplUpdateDisplayTreeInvalidValuesTest, MinPageScaleFactor) {
  const float invalid_value = GetParam();
  auto update = CreateDefaultUpdate();
  update->min_page_scale_factor = invalid_value;

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid page scale factors");
}

TEST_P(LayerContextImplUpdateDisplayTreeInvalidValuesTest, MaxPageScaleFactor) {
  const float invalid_value = GetParam();
  auto update = CreateDefaultUpdate();
  update->max_page_scale_factor = invalid_value;

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid page scale factors");
}

INSTANTIATE_TEST_SUITE_P(
    InvalidFloats,
    LayerContextImplUpdateDisplayTreeInvalidValuesTest,
    ::testing::Values(std::numeric_limits<float>::infinity(),
                      -std::numeric_limits<float>::infinity(),
                      std::numeric_limits<float>::quiet_NaN(),
                      0.0f,
                      -1.0f),
    [](const testing::TestParamInfo<
        LayerContextImplUpdateDisplayTreeInvalidValuesTest::ParamType>& info) {
      if (std::isinf(info.param)) {
        return info.param > 0 ? "Infinity" : "NegativeInfinity";
      }
      if (std::isnan(info.param)) {
        return "NaN";
      }
      if (info.param == 0.0f) {
        return "Zero";
      }
      if (info.param < 0.0f) {
        return "Negative";
      }
      return "Other";
    });

TEST_F(LayerContextImplTest, InvalidMinMaxPageScaleFactor) {
  auto update = CreateDefaultUpdate();
  update->min_page_scale_factor = 2.0f;
  update->max_page_scale_factor = 1.0f;

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid page scale factors");
}

class LayerContextImplUpdateDisplayTreeInvalidBrowserControlsTest
    : public LayerContextImplTest,
      public ::testing::WithParamInterface<float> {};

TEST_P(LayerContextImplUpdateDisplayTreeInvalidBrowserControlsTest,
       TopControlsHeight) {
  const float invalid_value = GetParam();
  auto update = CreateDefaultUpdate();
  update->browser_controls_params.top_controls_height = invalid_value;

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid browser controls params");
}

TEST_P(LayerContextImplUpdateDisplayTreeInvalidBrowserControlsTest,
       TopControlsMinHeight) {
  const float invalid_value = GetParam();
  auto update = CreateDefaultUpdate();
  update->browser_controls_params.top_controls_min_height = invalid_value;

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid browser controls params");
}

TEST_P(LayerContextImplUpdateDisplayTreeInvalidBrowserControlsTest,
       BottomControlsHeight) {
  const float invalid_value = GetParam();
  auto update = CreateDefaultUpdate();
  update->browser_controls_params.bottom_controls_height = invalid_value;

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid browser controls params");
}

TEST_P(LayerContextImplUpdateDisplayTreeInvalidBrowserControlsTest,
       BottomControlsMinHeight) {
  const float invalid_value = GetParam();
  auto update = CreateDefaultUpdate();
  update->browser_controls_params.bottom_controls_min_height = invalid_value;

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid browser controls params");
}

INSTANTIATE_TEST_SUITE_P(
    InvalidFloats,
    LayerContextImplUpdateDisplayTreeInvalidBrowserControlsTest,
    ::testing::Values(std::numeric_limits<float>::infinity(),
                      -std::numeric_limits<float>::infinity(),
                      std::numeric_limits<float>::quiet_NaN(),
                      -1.0f),
    [](const testing::TestParamInfo<
        LayerContextImplUpdateDisplayTreeInvalidBrowserControlsTest::ParamType>&
           info) {
      if (std::isinf(info.param)) {
        return info.param > 0 ? "Infinity" : "NegativeInfinity";
      }
      if (std::isnan(info.param)) {
        return "NaN";
      }
      if (info.param < 0.0f) {
        return "Negative";
      }
      return "Other";
    });

TEST_F(LayerContextImplTest, InvalidMinMaxBrowserControlsHeight) {
  auto update = CreateDefaultUpdate();
  update->browser_controls_params.top_controls_height = 10.f;
  update->browser_controls_params.top_controls_min_height = 20.f;

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid browser controls params");

  update = CreateDefaultUpdate();
  update->browser_controls_params.bottom_controls_height = 10.f;
  update->browser_controls_params.bottom_controls_min_height = 20.f;

  result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid browser controls params");
}

class LayerContextImplUpdateDisplayTreeInvalidElasticOverscrollTest
    : public LayerContextImplTest,
      public ::testing::WithParamInterface<float> {};

TEST_P(LayerContextImplUpdateDisplayTreeInvalidElasticOverscrollTest,
       ElasticOverscrollX) {
  const float invalid_value = GetParam();
  auto update = CreateDefaultUpdate();
  update->scroll_tree_update = mojom::ScrollTreeUpdate::New();
  update->scroll_tree_update->elastic_overscroll[cc::ElementId(123)] =
      gfx::Vector2dF(invalid_value, 0.f);

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid elastic_overscroll");
}

TEST_P(LayerContextImplUpdateDisplayTreeInvalidElasticOverscrollTest,
       ElasticOverscrollY) {
  const float invalid_value = GetParam();
  auto update = CreateDefaultUpdate();
  update->scroll_tree_update = mojom::ScrollTreeUpdate::New();
  update->scroll_tree_update->elastic_overscroll[cc::ElementId(123)] =
      gfx::Vector2dF(0.f, invalid_value);

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid elastic_overscroll");
}

INSTANTIATE_TEST_SUITE_P(
    InvalidFloats,
    LayerContextImplUpdateDisplayTreeInvalidElasticOverscrollTest,
    ::testing::Values(std::numeric_limits<float>::infinity(),
                      -std::numeric_limits<float>::infinity(),
                      std::numeric_limits<float>::quiet_NaN()),
    [](const testing::TestParamInfo<
        LayerContextImplUpdateDisplayTreeInvalidElasticOverscrollTest::
            ParamType>& info) {
      if (std::isinf(info.param)) {
        return info.param > 0 ? "Infinity" : "NegativeInfinity";
      }
      if (std::isnan(info.param)) {
        return "NaN";
      }
      return "Other";
    });

class LayerContextImplUpdateDisplayTreeInvalidBackdropFilterQualityTest
    : public LayerContextImplTest,
      public ::testing::WithParamInterface<float> {};

TEST_P(LayerContextImplUpdateDisplayTreeInvalidBackdropFilterQualityTest,
       BackdropFilterQuality) {
  const float invalid_value = GetParam();
  auto update = CreateDefaultUpdate();
  update->num_effect_nodes = 2;
  auto effect_node = mojom::EffectNode::New();
  effect_node->id = 1;
  effect_node->parent_id = 0;
  effect_node->transform_id = 0;
  effect_node->clip_id = 0;
  effect_node->target_id = 0;
  effect_node->backdrop_filter_quality = invalid_value;
  update->effect_nodes.push_back(std::move(effect_node));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid backdrop_filter_quality");
}

INSTANTIATE_TEST_SUITE_P(
    InvalidFloats,
    LayerContextImplUpdateDisplayTreeInvalidBackdropFilterQualityTest,
    ::testing::Values(std::numeric_limits<float>::infinity(),
                      -std::numeric_limits<float>::infinity(),
                      std::numeric_limits<float>::quiet_NaN(),
                      0.0f,
                      -0.1f,  // Less than 0
                      1.1f),  // Greater than 1
    [](const testing::TestParamInfo<
        LayerContextImplUpdateDisplayTreeInvalidBackdropFilterQualityTest::
            ParamType>& info) {
      if (std::isinf(info.param)) {
        return info.param > 0 ? "Infinity" : "NegativeInfinity";
      }
      if (std::isnan(info.param)) {
        return "NaN";
      }
      if (info.param == 0.0f) {
        return "Zero";
      }
      if (info.param < 0.0f) {
        return "Negative";
      }
      if (info.param > 1.0f) {
        return "GreaterThanOne";
      }
      return "Other";
    });

class LayerContextImplUpdateDisplayTreeInvalidTransformTreeUpdateTest
    : public LayerContextImplTest,
      public ::testing::WithParamInterface<float> {};

TEST_P(LayerContextImplUpdateDisplayTreeInvalidTransformTreeUpdateTest,
       PageScaleFactor) {
  const float invalid_value = GetParam();
  auto update = CreateDefaultUpdate();
  update->transform_tree_update = mojom::TransformTreeUpdate::New();
  update->transform_tree_update->page_scale_factor = invalid_value;

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid page_scale_factor");
}

TEST_P(LayerContextImplUpdateDisplayTreeInvalidTransformTreeUpdateTest,
       DeviceScaleFactor) {
  const float invalid_value = GetParam();
  auto update = CreateDefaultUpdate();
  update->transform_tree_update = mojom::TransformTreeUpdate::New();
  update->transform_tree_update->device_scale_factor = invalid_value;

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid device_scale_factor");
}

TEST_P(LayerContextImplUpdateDisplayTreeInvalidTransformTreeUpdateTest,
       DeviceTransformScaleFactor) {
  const float invalid_value = GetParam();
  auto update = CreateDefaultUpdate();
  update->transform_tree_update = mojom::TransformTreeUpdate::New();
  update->transform_tree_update->device_transform_scale_factor = invalid_value;

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid device_transform_scale_factor");
}

INSTANTIATE_TEST_SUITE_P(
    InvalidFloats,
    LayerContextImplUpdateDisplayTreeInvalidTransformTreeUpdateTest,
    ::testing::Values(std::numeric_limits<float>::infinity(),
                      -std::numeric_limits<float>::infinity(),
                      std::numeric_limits<float>::quiet_NaN(),
                      -1.0f),
    [](const testing::TestParamInfo<
        LayerContextImplUpdateDisplayTreeInvalidTransformTreeUpdateTest::
            ParamType>& info) {
      if (std::isinf(info.param)) {
        return info.param > 0 ? "Infinity" : "NegativeInfinity";
      }
      if (std::isnan(info.param)) {
        return "NaN";
      }
      if (info.param < 0.0f) {
        return "Negative";
      }
      return "Other";
    });

}  // namespace
}  // namespace viz
