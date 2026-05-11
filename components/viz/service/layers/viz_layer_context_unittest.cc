// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/layers/viz_layer_context_unittest.h"

#include <utility>

#include "base/test/run_until.h"
#include "base/unguessable_token.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/fake_raster_source.h"
#include "cc/test/property_tree_test_utils.h"
#include "cc/tiles/picture_layer_tiling.h"
#include "cc/tiles/tile.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/viz/client/client_resource_provider.h"
#include "ui/gfx/geometry/axis_transform2d.h"

namespace viz {

FakeLayerContext::FakeLayerContext() = default;
FakeLayerContext::~FakeLayerContext() = default;

void FakeLayerContext::Bind(mojom::PendingLayerContextPtr context) {
  context->receiver.EnableUnassociatedUsage();
  context->client.EnableUnassociatedUsage();
  receiver_.Bind(std::move(context->receiver));
  client_.Bind(std::move(context->client));
}

void FakeLayerContext::SetVisible(bool visible) {}

void FakeLayerContext::UpdateDisplayTree(mojom::LayerTreeUpdatePtr update) {
  last_update_ = std::move(update);
  if (on_update_display_tree_) {
    std::move(on_update_display_tree_).Run();
  }
}

void FakeLayerContext::UpdateDisplayTiling(mojom::TilingPtr tiling) {
  last_tiling_ = std::move(tiling);
  if (on_update_display_tiling_) {
    std::move(on_update_display_tiling_).Run();
  }
}

void FakeLayerContext::SetTargetLocalSurfaceId(
    const LocalSurfaceId& target_local_surface_id) {}

FakeCompositorFrameSink::FakeCompositorFrameSink(
    FakeLayerContext* layer_context)
    : layer_context_(layer_context) {}

FakeCompositorFrameSink::~FakeCompositorFrameSink() = default;

void FakeCompositorFrameSink::BindLayerContext(
    mojom::PendingLayerContextPtr context,
    mojom::LayerContextSettingsPtr settings) {
  layer_context_->Bind(std::move(context));
}

VizLayerContextTest::VizLayerContextTest()
    : fake_compositor_frame_sink_(&fake_layer_context_) {
  scoped_feature_list_.InitAndEnableFeature(features::kTreesInViz);
}

VizLayerContextTest::~VizLayerContextTest() = default;

void VizLayerContextTest::SetUp() {
  cc::LayerTreeSettings settings;
  settings.commit_to_active_tree = false;
  host_impl_ = std::make_unique<MyFakeLayerTreeHostImpl>(
      settings, &task_runner_provider_, &task_graph_runner_);
  layer_tree_frame_sink_ = cc::FakeLayerTreeFrameSink::Create3d();
  host_impl_->InitializeFrameSink(layer_tree_frame_sink_.get());
  host_impl_->CreatePendingTree();
  host_impl_->pending_tree()->SetLocalSurfaceIdFromParent(
      LocalSurfaceId(1, 1, base::UnguessableToken::Create()));
  host_impl_->ActivateSyncTree();
  host_impl_->IncrementFrameToken();

  viz_layer_context_ = std::make_unique<cc::mojo_embedder::VizLayerContext>(
      fake_compositor_frame_sink_, *host_impl_);
}

cc::LayerImpl* VizLayerContextTest::SetupRootLayer() {
  host_impl_->active_tree()->set_trace_id(
      cc::BeginMainFrameTraceId(VizLayerContextTest::kTraceId));
  host_impl_->active_tree()->SetRootLayerForTesting(cc::LayerImpl::Create(
      host_impl_->active_tree(), VizLayerContextTest::kRootLayerId));
  auto* root_layer = host_impl_->active_tree()->root_layer();
  root_layer->SetBounds(VizLayerContextTest::kDefaultSize);
  host_impl_->active_tree()->property_trees()->clear();
  cc::SetupRootProperties(root_layer);
  return root_layer;
}

cc::FakePictureLayerImpl* VizLayerContextTest::SetupPictureLayer() {
  constexpr gfx::Size kSize(100, 100);
  SetupRootLayer();

  host_impl_->CreatePendingTree();
  host_impl_->pending_tree()->SetRootLayerForTesting(cc::LayerImpl::Create(
      host_impl_->pending_tree(), VizLayerContextTest::kRootLayerId));
  auto* pending_root = host_impl_->pending_tree()->root_layer();
  cc::SetupRootProperties(pending_root);

  // Create a PictureLayerImpl and add it to the pending tree.
  auto picture_layer = cc::FakePictureLayerImpl::Create(
      host_impl_->pending_tree(), VizLayerContextTest::kChildLayerId,
      cc::FakeRasterSource::CreateFilled(kSize));
  picture_layer->SetBounds(kSize);
  picture_layer->SetDrawsContent(true);

  // Setup properties for the layer.
  auto* pending_layer_ptr = picture_layer.get();
  cc::CopyProperties(pending_root, pending_layer_ptr);
  host_impl_->pending_tree()->AddLayer(std::move(picture_layer));
  host_impl_->pending_tree()->AddLayerShouldPushProperties(pending_layer_ptr);

  // Activate the pending tree.
  host_impl_->ActivateSyncTree();
  return static_cast<cc::FakePictureLayerImpl*>(
      host_impl_->active_tree()->LayerById(VizLayerContextTest::kChildLayerId));
}

void VizLayerContextTest::UpdateDisplayTreeAndWait() {
  const gfx::Rect& viewport_damage_rect =
      VizLayerContextTest::kDefaultDamageRect;
  bool frame_has_damage = false;
  std::vector<ui::LatencyInfo> latency_info = {};

  viz_layer_context_->UpdateDisplayTreeFrom(
      *host_impl_->active_tree(), *host_impl_->resource_provider(),
      /*shared_image_interface=*/nullptr, viewport_damage_rect,
      frame_has_damage, /*is_flush=*/false, std::move(latency_info),
      TrackedElementRects());

  base::RunLoop run_loop;
  fake_layer_context_.on_update_display_tree_ = run_loop.QuitClosure();
  run_loop.Run();
}

TEST_F(VizLayerContextTest, BasicCreateAndActivate) {
  auto* root_layer = SetupRootLayer();
  root_layer->SetDrawsContent(true);
  host_impl_->active_tree()->property_trees()->clear();
  cc::SetupRootProperties(root_layer);

  UpdateDisplayTreeAndWait();

  ASSERT_TRUE(fake_layer_context_.last_update_);
  const auto& update = fake_layer_context_.last_update_;

  EXPECT_EQ(update->layers.size(), 1u);
  EXPECT_EQ(update->layers[0]->id, VizLayerContextTest::kRootLayerId);
}

TEST_F(VizLayerContextTest, MultipleUpdates) {
  SetupRootLayer();

  UpdateDisplayTreeAndWait();
  ASSERT_TRUE(fake_layer_context_.last_update_);

  // Add another layer and submit a second update.
  auto child_layer = cc::LayerImpl::Create(host_impl_->active_tree(),
                                           VizLayerContextTest::kChildLayerId);
  child_layer->SetBounds(gfx::Size(5, 5));
  host_impl_->active_tree()->AddLayer(std::move(child_layer));
  host_impl_->active_tree()->AddLayerShouldPushProperties(
      host_impl_->active_tree()->LayerById(VizLayerContextTest::kChildLayerId));

  UpdateDisplayTreeAndWait();

  const auto& update2 = fake_layer_context_.last_update_;
  EXPECT_EQ(update2->layers.size(), 1u);
}

TEST_F(VizLayerContextTest, UpdateLayerProperty) {
  const gfx::Size& kNewBounds = gfx::Size(20, 20);
  auto* root_layer = SetupRootLayer();

  UpdateDisplayTreeAndWait();

  // Update bounds
  root_layer->SetBounds(kNewBounds);
  root_layer->SetNeedsPushProperties(cc::LayerImpl::kChangedAllProperties);
  host_impl_->active_tree()->AddLayerShouldPushProperties(root_layer);

  UpdateDisplayTreeAndWait();

  const auto& update2 = fake_layer_context_.last_update_;
  EXPECT_EQ(update2->layers.size(), 1u);
  EXPECT_EQ(update2->layers[0]->id, VizLayerContextTest::kRootLayerId);
  EXPECT_EQ(update2->layers[0]->general_properties->bounds, kNewBounds);
}

TEST_F(VizLayerContextTest, UpdateDisplayTile) {
  auto* layer_ptr = SetupPictureLayer();

  // Add a tiling to the layer.
  constexpr float kScale = 1.0f;
  auto* pending_tiling =
      layer_ptr->AddTiling(gfx::AxisTransform2d(kScale, gfx::Vector2dF()));
  pending_tiling->set_resolution(cc::HIGH_RESOLUTION);

  cc::PictureLayerTiling* tiling =
      layer_ptr->picture_layer_tiling_set()->tiling_at(0);

  // Sync the display tree initially.
  UpdateDisplayTreeAndWait();

  // Create a tile in the tiling.
  layer_ptr->CreateAllTiles();
  cc::Tile* tile = tiling->TileAt(0, 0);
  ASSERT_NE(tile, nullptr);

  // Update the display tile and wait for the tiling update in FakeLayerContext.
  viz_layer_context_->UpdateDisplayTile(*layer_ptr, *tile,
                                        *host_impl_->resource_provider(),
                                        /*shared_image_interface=*/nullptr,
                                        /*update_damage=*/true);

  base::RunLoop run_loop;
  fake_layer_context_.on_update_display_tiling_ = run_loop.QuitClosure();
  run_loop.Run();

  ASSERT_TRUE(fake_layer_context_.last_tiling_);
  EXPECT_EQ(fake_layer_context_.last_tiling_->layer_id,
            VizLayerContextTest::kChildLayerId);
  EXPECT_EQ(fake_layer_context_.last_tiling_->tiles.size(), 1u);
  EXPECT_EQ(fake_layer_context_.last_tiling_->tiles[0]->column_index, 0u);
  EXPECT_EQ(fake_layer_context_.last_tiling_->tiles[0]->row_index, 0u);
}

TEST_F(VizLayerContextTest, OnTilingsReadyForCleanup) {
  auto* layer_ptr = SetupPictureLayer();

  // Add a tiling to the layer.
  constexpr float kScale = 1.0f;
  layer_ptr->AddTiling(gfx::AxisTransform2d(kScale, gfx::Vector2dF()));
  ASSERT_EQ(layer_ptr->picture_layer_tiling_set()->num_tilings(), 1u);

  // Trigger tiling cleanup from the service side.
  std::vector<float> scales = {kScale};
  fake_layer_context_.client_->OnTilingsReadyForCleanup(
      VizLayerContextTest::kChildLayerId, scales);
  fake_layer_context_.client_.FlushForTesting();

  // The tiling should be removed.
  EXPECT_EQ(layer_ptr->picture_layer_tiling_set()->num_tilings(), 0u);
}

TEST_F(VizLayerContextTest, SyncViewportContainerBoundsDeltas) {
  SetupRootLayer();
  auto* active_tree = host_impl_->active_tree();

  active_tree->property_trees()->SetInnerViewportContainerBoundsDelta(
      gfx::Vector2dF(10.f, 20.f));
  active_tree->property_trees()->SetOuterViewportContainerBoundsDelta(
      gfx::Vector2dF(30.f, 40.f));

  UpdateDisplayTreeAndWait();

  const auto& update = fake_layer_context_.last_update_;
  ASSERT_TRUE(update);
  EXPECT_EQ(update->inner_viewport_container_bounds_delta,
            gfx::Vector2dF(10.f, 20.f));
  EXPECT_EQ(update->outer_viewport_container_bounds_delta,
            gfx::Vector2dF(30.f, 40.f));
}

TEST_F(VizLayerContextTest, FlushOnlyUpdate) {
  SetupRootLayer();

  // Perform a regular update first.
  UpdateDisplayTreeAndWait();
  ASSERT_TRUE(fake_layer_context_.last_update_);
  EXPECT_FALSE(fake_layer_context_.last_update_->is_flush);

  // Now trigger a flush-only update manually.
  const gfx::Rect& viewport_damage_rect =
      VizLayerContextTest::kDefaultDamageRect;
  bool frame_has_damage = false;
  std::vector<ui::LatencyInfo> latency_info = {};

  viz_layer_context_->UpdateDisplayTreeFrom(
      *host_impl_->active_tree(), *host_impl_->resource_provider(),
      /*shared_image_interface=*/nullptr, viewport_damage_rect,
      frame_has_damage, /*is_flush=*/true, std::move(latency_info),
      TrackedElementRects());

  base::RunLoop run_loop;
  fake_layer_context_.on_update_display_tree_ = run_loop.QuitClosure();
  run_loop.Run();

  ASSERT_TRUE(fake_layer_context_.last_update_);
  EXPECT_TRUE(fake_layer_context_.last_update_->is_flush);
}

TEST_F(VizLayerContextTest, RecoveryFromMojoConnectionError) {
  // Simulate a connection error with a custom reason from the service side.
  // This is what LayerContextImpl::SubmitCompositorFrame does on failure.
  fake_layer_context_.client_.ResetWithReason(1, "test failure");

  viz_layer_context_->FlushReceiverForTesting();

  // host_impl_->DidLoseLayerTreeFrameSink() should have been called, which
  // in turn calls the client method.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return host_impl_->delegate()
        ->did_lose_layer_tree_frame_sink_on_impl_thread();
  }));
}

TEST_F(VizLayerContextTest, NoRecoveryFromNormalMojoDisconnect) {
  // Simulate a connection error WITHOUT a custom reason from the service side.
  // This happens when LayerContextImpl is destroyed normally.
  fake_layer_context_.client_.reset();

  viz_layer_context_->FlushReceiverForTesting();

  // host_impl_->DidLoseLayerTreeFrameSink() should NOT have been called.
  EXPECT_FALSE(
      host_impl_->delegate()->did_lose_layer_tree_frame_sink_on_impl_thread());
}

}  // namespace viz
