// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/layers/viz_layer_context_unittest.h"

#include <utility>

#include "base/unguessable_token.h"
#include "cc/layers/layer_impl.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/test/property_tree_test_utils.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/viz/client/client_resource_provider.h"

namespace viz {

FakeLayerContext::FakeLayerContext() = default;
FakeLayerContext::~FakeLayerContext() = default;

void FakeLayerContext::Bind(
    mojo::PendingAssociatedReceiver<mojom::LayerContext> receiver) {
  receiver.EnableUnassociatedUsage();
  receiver_.Bind(std::move(receiver));
}

void FakeLayerContext::SetVisible(bool visible) {}

void FakeLayerContext::UpdateDisplayTree(mojom::LayerTreeUpdatePtr update) {
  last_update_ = std::move(update);
  if (on_update_display_tree_) {
    std::move(on_update_display_tree_).Run();
  }
}

// TODO(b/492322546): Add tests for UpdateDisplayTiling in the future.
void FakeLayerContext::UpdateDisplayTiling(mojom::TilingPtr tiling) {}

void FakeLayerContext::SetTargetLocalSurfaceId(
    const LocalSurfaceId& target_local_surface_id) {}

FakeCompositorFrameSink::FakeCompositorFrameSink(
    FakeLayerContext* layer_context)
    : layer_context_(layer_context) {}

FakeCompositorFrameSink::~FakeCompositorFrameSink() = default;

void FakeCompositorFrameSink::BindLayerContext(
    mojom::PendingLayerContextPtr context,
    mojom::LayerContextSettingsPtr settings) {
  layer_context_->Bind(std::move(context->receiver));
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

void VizLayerContextTest::UpdateDisplayTreeAndWait() {
  const gfx::Rect& viewport_damage_rect =
      VizLayerContextTest::kDefaultDamageRect;
  bool frame_has_damage = false;
  std::vector<ui::LatencyInfo> latency_info = {};

  viz_layer_context_->UpdateDisplayTreeFrom(
      *host_impl_->active_tree(), *host_impl_->resource_provider(),
      /*shared_image_interface=*/nullptr, viewport_damage_rect,
      frame_has_damage, std::move(latency_info));

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

}  // namespace viz
