// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/viz/service/layers/layer_context_impl.h"
#include "components/viz/service/layers/layer_context_impl_base_unittest.h"
#include "services/viz/public/mojom/compositing/layer_context.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace viz {
namespace {

class LayerContextImplUpdateDisplayTilingTest : public LayerContextImplTest {
 protected:
  cc::TileDisplayLayerImpl* GetTileDisplayLayerFromActiveTree(int layer_id) {
    cc::LayerImpl* layer = GetLayerFromActiveTree(layer_id);
    if (layer && layer->GetLayerType() == cc::mojom::LayerType::kTileDisplay) {
      return static_cast<cc::TileDisplayLayerImpl*>(layer);
    }
    return nullptr;
  }

  mojom::TilingPtr CreateTiling(int layer_id,
                                float scale_key,
                                const gfx::Size& tile_size,
                                const gfx::Rect& tiling_rect) {
    auto tiling = mojom::Tiling::New();
    tiling->layer_id = layer_id;
    tiling->scale_key = scale_key;
    tiling->raster_scale = gfx::Vector2dF(scale_key, scale_key);
    tiling->tile_size = tile_size;
    tiling->tiling_rect = tiling_rect;
    return tiling;
  }

  mojom::TilePtr CreateSolidColorTile(const cc::TileIndex& index,
                                      SkColor4f color) {
    auto tile = mojom::Tile::New();
    tile->column_index = index.i;
    tile->row_index = index.j;
    tile->contents = mojom::TileContents::NewSolidColor(color);
    return tile;
  }

  mojom::TilePtr CreateResourceTile(const cc::TileIndex& index,
                                    ResourceId resource_id,
                                    const gfx::Size& resource_size) {
    auto tile = mojom::Tile::New();
    tile->column_index = index.i;
    tile->row_index = index.j;
    auto resource_contents = mojom::TileResource::New();
    resource_contents->resource = MakeFakeResource(resource_size);
    resource_contents->resource.id = resource_id;
    tile->contents =
        mojom::TileContents::NewResource(std::move(resource_contents));
    return tile;
  }
};

TEST_F(LayerContextImplUpdateDisplayTilingTest, TilingAndTileLifecycle) {
  constexpr int kLayerId = 2;
  constexpr float kScaleKey1 = 1.0f;
  constexpr float kScaleKey2 = 2.0f;
  const gfx::Size kTileSize(64, 64);
  const gfx::Rect kTilingRect(0, 0, 200, 200);
  const cc::TileIndex kTileIndex1(0, 0);
  const cc::TileIndex kTileIndex2(1, 1);
  const cc::TileIndex kTileIndex3(0, 1);
  const gfx::Size kNewTileSize(32, 32);
  const ResourceId kResourceId1(23);

  // Initial update: Create TileDisplayLayer.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), cc::mojom::LayerType::kTileDisplay,
                          kLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::TileDisplayLayerImpl* layer_impl =
      GetTileDisplayLayerFromActiveTree(kLayerId);
  ASSERT_NE(nullptr, layer_impl);

  // Test Case 1: Create a new Tiling with a SolidColor Tile.
  auto tiling1 = mojom::Tiling::New();
  tiling1->layer_id = kLayerId;
  tiling1->scale_key = kScaleKey1;
  tiling1->raster_scale = gfx::Vector2dF(kScaleKey1, kScaleKey1);
  tiling1->tile_size = kTileSize;
  tiling1->tiling_rect = kTilingRect;
  auto tile1_solid = mojom::Tile::New();
  tile1_solid->column_index = kTileIndex1.i;
  tile1_solid->row_index = kTileIndex1.j;
  tile1_solid->contents =
      mojom::TileContents::NewSolidColor(SkColors::kMagenta);
  tiling1->tiles.push_back(std::move(tile1_solid));
  EXPECT_TRUE(layer_context_impl_
                  ->DoUpdateDisplayTiling(std::move(tiling1),
                                          /*update_damage=*/true)
                  .has_value());

  const cc::TileDisplayLayerTiling* tiling_impl1 =
      layer_impl->GetTilingForTesting(kScaleKey1);
  ASSERT_NE(nullptr, tiling_impl1);
  EXPECT_EQ(tiling_impl1->tile_size(), kTileSize);
  ASSERT_NE(nullptr, tiling_impl1->TileAt(kTileIndex1));
  EXPECT_TRUE(tiling_impl1->TileAt(kTileIndex1)->solid_color().has_value());

  // Test Case 2: Add a Resource Tile to the existing Tiling.
  auto tiling1_update = mojom::Tiling::New();
  tiling1_update->layer_id = kLayerId;
  tiling1_update->scale_key = kScaleKey1;
  tiling1_update->raster_scale = gfx::Vector2dF(kScaleKey1, kScaleKey1);
  tiling1_update->tile_size = kTileSize;
  tiling1_update->tiling_rect = kTilingRect;
  auto tile2_resource = mojom::Tile::New();
  tile2_resource->column_index = kTileIndex2.i;
  tile2_resource->row_index = kTileIndex2.j;
  auto resource_contents = mojom::TileResource::New();
  resource_contents->resource = MakeFakeResource(kTileSize);
  ;
  resource_contents->resource.id = kResourceId1;
  tile2_resource->contents =
      mojom::TileContents::NewResource(std::move(resource_contents));
  tiling1_update->tiles.push_back(std::move(tile2_resource));
  EXPECT_TRUE(layer_context_impl_
                  ->DoUpdateDisplayTiling(std::move(tiling1_update),
                                          /*update_damage=*/true)
                  .has_value());
  ASSERT_NE(nullptr, tiling_impl1->TileAt(kTileIndex1));  // Should still exist.
  ASSERT_NE(nullptr, tiling_impl1->TileAt(kTileIndex2));
  EXPECT_TRUE(tiling_impl1->TileAt(kTileIndex2)->resource().has_value());

  // Test Case 3: Add a second Tiling.
  auto tiling2 = mojom::Tiling::New();
  tiling2->layer_id = kLayerId;
  tiling2->scale_key = kScaleKey2;
  tiling2->raster_scale = gfx::Vector2dF(kScaleKey2, kScaleKey2);
  tiling2->tile_size = kTileSize;
  tiling2->tiling_rect = kTilingRect;
  auto tile3_solid = mojom::Tile::New();
  tile3_solid->column_index = kTileIndex1.i;
  tile3_solid->row_index = kTileIndex1.j;
  tile3_solid->contents = mojom::TileContents::NewSolidColor(SkColors::kCyan);
  tiling2->tiles.push_back(std::move(tile3_solid));
  EXPECT_TRUE(layer_context_impl_
                  ->DoUpdateDisplayTiling(std::move(tiling2),
                                          /*update_damage=*/true)
                  .has_value());
  const cc::TileDisplayLayerTiling* tiling_impl2 =
      layer_impl->GetTilingForTesting(kScaleKey2);
  ASSERT_NE(nullptr, tiling_impl2);

  // Test Case 4: Explicitly delete a Tiling.
  auto tiling1_deleted_marker = mojom::Tiling::New();
  tiling1_deleted_marker->layer_id = kLayerId;
  tiling1_deleted_marker->scale_key = kScaleKey1;
  tiling1_deleted_marker->is_deleted = true;
  EXPECT_TRUE(layer_context_impl_
                  ->DoUpdateDisplayTiling(std::move(tiling1_deleted_marker),
                                          /*update_damage=*/true)
                  .has_value());
  EXPECT_EQ(nullptr, layer_impl->GetTilingForTesting(kScaleKey1));
  EXPECT_NE(nullptr, layer_impl->GetTilingForTesting(kScaleKey2));

  // Test Case 5: Implicitly delete a Tiling by removing all its tiles.
  // (tiling_impl2 currently has one solid color tile).
  // Send an update for tiling_impl2 that marks its only tile as deleted.
  auto tiling2_empty_update = mojom::Tiling::New();
  tiling2_empty_update->layer_id = kLayerId;
  tiling2_empty_update->scale_key = kScaleKey2;
  tiling2_empty_update->raster_scale = gfx::Vector2dF(kScaleKey2, kScaleKey2);
  tiling2_empty_update->tile_size = kTileSize;
  tiling2_empty_update->tiling_rect = kTilingRect;
  auto tile_deleted_marker = mojom::Tile::New();
  tile_deleted_marker->column_index = kTileIndex1.i;
  tile_deleted_marker->row_index = kTileIndex1.j;
  tile_deleted_marker->contents = mojom::TileContents::NewMissingReason(
      cc::mojom::MissingTileReason::kTileDeleted);  // Mark for deletion
  tiling2_empty_update->tiles.push_back(std::move(tile_deleted_marker));

  EXPECT_TRUE(layer_context_impl_
                  ->DoUpdateDisplayTiling(std::move(tiling2_empty_update),
                                          /*update_damage=*/true)
                  .has_value());
  EXPECT_EQ(nullptr, layer_impl->GetTilingForTesting(kScaleKey2));

  // Test Case 6: Changing tile_size does not remove existing tiles.
  // Recreate tiling1 with a solid color tile.
  auto tiling1_recreate_for_size_change =
      CreateTiling(kLayerId, kScaleKey1, kTileSize, kTilingRect);
  tiling1_recreate_for_size_change->tiles.push_back(
      CreateSolidColorTile(kTileIndex1, SkColors::kRed));
  EXPECT_TRUE(
      layer_context_impl_
          ->DoUpdateDisplayTiling(std::move(tiling1_recreate_for_size_change),
                                  /*update_damage=*/true)
          .has_value());
  tiling_impl1 = layer_impl->GetTilingForTesting(kScaleKey1);
  ASSERT_NE(nullptr, tiling_impl1);
  ASSERT_NE(nullptr, tiling_impl1->TileAt(kTileIndex1));

  // Update tile_size.
  auto tiling1_size_update =
      CreateTiling(kLayerId, kScaleKey1, kNewTileSize, kTilingRect);
  // Add a new tile to ensure the update is processed, but don't re-add
  // kTileIndex1.
  tiling1_size_update->tiles.push_back(
      CreateSolidColorTile(kTileIndex3, SkColors::kGreen));
  EXPECT_TRUE(layer_context_impl_
                  ->DoUpdateDisplayTiling(std::move(tiling1_size_update),
                                          /*update_damage=*/true)
                  .has_value());
  EXPECT_EQ(tiling_impl1, layer_impl->GetTilingForTesting(
                              kScaleKey1));  // Tiling should be the same
  EXPECT_EQ(tiling_impl1->tile_size(), kNewTileSize);
  EXPECT_NE(nullptr, tiling_impl1->TileAt(kTileIndex1));  // Should still exist
  EXPECT_NE(nullptr, tiling_impl1->TileAt(kTileIndex3));  // Should still exist

  // Test Case 7: Changing tiling_rect does not remove existing tiles.
  // kTileIndex1 and kTileIndex3 should still exist from the previous step.
  const gfx::Rect kNewTilingRect(50, 50, 100, 100);
  auto tiling1_rect_update =
      CreateTiling(kLayerId, kScaleKey1, kNewTileSize, kNewTilingRect);
  // No new tiles added in this update, just changing the rect.
  EXPECT_TRUE(layer_context_impl_
                  ->DoUpdateDisplayTiling(std::move(tiling1_rect_update),
                                          /*update_damage=*/true)
                  .has_value());
  EXPECT_EQ(tiling_impl1, layer_impl->GetTilingForTesting(
                              kScaleKey1));  // Tiling should be the same
  EXPECT_EQ(tiling_impl1->tiling_rect(), kNewTilingRect);
  EXPECT_NE(nullptr, tiling_impl1->TileAt(kTileIndex1));  // Should still exist
  EXPECT_NE(nullptr, tiling_impl1->TileAt(kTileIndex3));  // Should still exist
}

TEST_F(LayerContextImplUpdateDisplayTilingTest, InvalidTilingUpdate) {
  constexpr int kLayerId = 2;
  constexpr int kNonTileDisplayLayerId = 3;
  constexpr float kScaleKey = 1.0f;
  const gfx::Size kValidTileSize(64, 64);
  const gfx::Rect kValidTilingRect(0, 0, 128, 128);

  // Setup: Create a TileDisplayLayer and a regular Layer.
  auto setup_update = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(setup_update.get(),
                          cc::mojom::LayerType::kTileDisplay, kLayerId);
  AddDefaultLayerToUpdate(setup_update.get(), cc::mojom::LayerType::kLayer,
                          kNonTileDisplayLayerId);
  EXPECT_TRUE(layer_context_impl_->DoUpdateDisplayTree(std::move(setup_update))
                  .has_value());

  // Test Case 1: Tiling update for a non-TileDisplayLayer.
  auto tiling_for_wrong_layer = mojom::Tiling::New();
  tiling_for_wrong_layer->layer_id = kNonTileDisplayLayerId;
  tiling_for_wrong_layer->scale_key = kScaleKey;
  tiling_for_wrong_layer->raster_scale = gfx::Vector2dF(kScaleKey, kScaleKey);
  tiling_for_wrong_layer->tile_size = kValidTileSize;
  tiling_for_wrong_layer->tiling_rect = kValidTilingRect;
  auto result1 = layer_context_impl_->DoUpdateDisplayTiling(
      std::move(tiling_for_wrong_layer),
      /*update_damage=*/true);
  ASSERT_FALSE(result1.has_value());
  EXPECT_EQ(result1.error(), "Invalid tile update");

  // Test Case 2: Tiling update with invalid tile size.
  auto tiling_invalid_size = mojom::Tiling::New();
  tiling_invalid_size->layer_id = kLayerId;
  tiling_invalid_size->scale_key = kScaleKey;
  tiling_invalid_size->raster_scale = gfx::Vector2dF(kScaleKey, kScaleKey);
  tiling_invalid_size->tile_size = gfx::Size(0, 10);
  tiling_invalid_size->tiling_rect = kValidTilingRect;
  auto result2 =
      layer_context_impl_->DoUpdateDisplayTiling(std::move(tiling_invalid_size),
                                                 /*update_damage=*/true);
  ASSERT_FALSE(result2.has_value());
  EXPECT_EQ(result2.error(), "Invalid tile_size dimensions in Tiling");

  // Test Case 3: Tile with invalid resource ID.
  auto tiling_invalid_resource = mojom::Tiling::New();
  tiling_invalid_resource->layer_id = kLayerId;
  tiling_invalid_resource->scale_key = kScaleKey;
  tiling_invalid_resource->raster_scale = gfx::Vector2dF(kScaleKey, kScaleKey);
  tiling_invalid_resource->tile_size = kValidTileSize;
  tiling_invalid_resource->tiling_rect = kValidTilingRect;
  auto tile_invalid_resource_tile = mojom::Tile::New();
  tile_invalid_resource_tile->column_index = 0;
  tile_invalid_resource_tile->row_index = 0;
  auto resource_contents = mojom::TileResource::New();
  resource_contents->resource = MakeFakeResource(kValidTileSize);
  resource_contents->resource.id = kInvalidResourceId;
  tile_invalid_resource_tile->contents =
      mojom::TileContents::NewResource(std::move(resource_contents));
  tiling_invalid_resource->tiles.push_back(
      std::move(tile_invalid_resource_tile));
  auto result3 = layer_context_impl_->DoUpdateDisplayTiling(
      std::move(tiling_invalid_resource),
      /*update_damage=*/true);
  ASSERT_FALSE(result3.has_value());
  EXPECT_EQ(result3.error(), "Invalid tile resource");
}

TEST_F(LayerContextImplUpdateDisplayTilingTest,
       TilingWithInvalidLayerIdIsIgnored) {
  constexpr int kInvalidLayerId = 999;  // An ID that doesn't exist.

  auto tiling = CreateTiling(kInvalidLayerId, 1.0f, gfx::Size(64, 64),
                             gfx::Rect(100, 100));
  tiling->tiles.push_back(
      CreateSolidColorTile(cc::TileIndex(0, 0), SkColors::kRed));

  // An update for a non-existent layer should be silently ignored and not
  // cause a crash or return an error.
  auto result =
      layer_context_impl_->DoUpdateDisplayTiling(std::move(tiling),
                                                 /*update_damage=*/true);
  EXPECT_TRUE(result.has_value());
}

}  // namespace
}  // namespace viz
