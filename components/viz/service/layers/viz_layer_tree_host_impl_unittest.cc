// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/layers/viz_layer_tree_host_impl.h"

#include "base/functional/callback_helpers.h"
#include "base/threading/thread.h"
#include "cc/animation/animation_host.h"
#include "cc/layers/solid_color_layer_impl.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_host_impl_test_base.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class VizLayerTreeHostImplTest : public LayerTreeHostImplTest {
 public:
  static constexpr gfx::Size kDefaultSize{10, 10};
  static constexpr gfx::Rect kDefaultViewport{10, 10};

  LayerTreeSettings DefaultSettings() override {
    LayerTreeSettings settings = LayerTreeHostImplTest::DefaultSettings();
    settings.trees_in_viz_in_viz_process = true;
    return settings;
  }

  bool CreateHostImpl(
      const LayerTreeSettings& settings,
      std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink) override {
    if (host_impl_) {
      host_impl_->ReleaseLayerTreeFrameSink();
    }
    host_impl_.reset();
    InitializeImageWorker(settings);
    host_impl_ = viz::VizLayerTreeHostImpl::Create(
        settings, this, &task_runner_provider_, &stats_instrumentation_,
        &task_graph_runner_,
        AnimationHost::CreateForTesting(ThreadInstance::kImpl), nullptr, 0,
        image_worker_ ? image_worker_->task_runner() : nullptr, nullptr);
    InputHandler::Create(static_cast<CompositorDelegateForInput&>(*host_impl_));
    layer_tree_frame_sink_ = std::move(layer_tree_frame_sink);
    host_impl_->SetVisible(true);
    bool init = host_impl_->InitializeFrameSink(layer_tree_frame_sink_.get());
    host_impl_->active_tree()->SetDeviceViewportRect(kDefaultViewport);
    host_impl_->active_tree()->PushPageScaleFromMainThread(1, 1, 1);
    host_impl_->active_tree()->SetLocalSurfaceIdFromParent(viz::LocalSurfaceId(
        1, base::UnguessableToken::CreateForTesting(2u, 3u)));
    return init;
  }

 protected:
  viz::VizLayerTreeHostImpl* viz_host_impl() {
    return static_cast<viz::VizLayerTreeHostImpl*>(host_impl_.get());
  }

  FakeLayerTreeFrameSink* fake_frame_sink() {
    return static_cast<FakeLayerTreeFrameSink*>(
        host_impl_->layer_tree_frame_sink());
  }
};

INSTANTIATE_COMMIT_TO_TREE_TEST_P(VizLayerTreeHostImplTest);

TEST_P(VizLayerTreeHostImplTest, FrameDataTimestampsGetSetInCFMetadata) {
  auto* root = SetupRootLayer<DidDrawCheckLayer>(host_impl_->active_tree(),
                                                 kDefaultSize);

  // Make a child layer that draws.
  auto* layer = AddLayer<SolidColorLayerImpl>(host_impl_->active_tree());
  layer->SetBounds(kDefaultSize);
  layer->SetDrawsContent(true);
  layer->SetBackgroundColor(SkColors::kRed);
  CopyProperties(root, layer);

  UpdateDrawProperties(host_impl_->active_tree());
  TestFrameData frame;
  frame.set_trees_in_viz_timestamps(
      {base::TimeTicks::Now(), base::TimeTicks::Now() + base::Milliseconds(1),
       base::TimeTicks::Now() + base::Milliseconds(2),
       base::TimeTicks::Now() + base::Milliseconds(3)});
  auto args = viz::CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
      base::TimeTicks() + base::Milliseconds(1));
  host_impl_->WillBeginImplFrame(args);
  // This would be set by LayerContextImpl as part of UpdateDisplayTree, set
  // manually to avoid DCHECK failure.
  viz_host_impl()->set_next_frame_token_from_client(frame.frame_token + 1);
  EXPECT_EQ(DrawResult::kSuccess, host_impl_->PrepareToDraw(&frame));

  // This function sets the metadata timestamps from FrameData.
  std::optional<SubmitInfo> submit_info = host_impl_->DrawLayers(&frame);

  const viz::CompositorFrameMetadata& metadata =
      fake_frame_sink()->last_sent_frame()->metadata;

  // Asset that the timestamps are assigned as expected.
  EXPECT_EQ(frame.trees_in_viz_timing_details->start_update_display_tree,
            metadata.trees_in_viz_timing_details.start_update_display_tree);
  EXPECT_EQ(frame.trees_in_viz_timing_details->start_prepare_to_draw,
            metadata.trees_in_viz_timing_details.start_prepare_to_draw);
  EXPECT_EQ(frame.trees_in_viz_timing_details->start_draw_layers,
            metadata.trees_in_viz_timing_details.start_draw_layers);
  // This timestamp is set inside DrawLayers, so it should be
  // equat to submit info submit time.
  EXPECT_EQ(submit_info.value().time,
            metadata.trees_in_viz_timing_details.submit_compositor_frame);
}

TEST_P(VizLayerTreeHostImplTest, CreateUIResourceFromImportedResource) {
  const UIResourceId ui_resource_id = 1;
  const gfx::Size size(100, 200);

  auto client_shared_image = gpu::ClientSharedImage::CreateForTesting(
      gpu::Mailbox::Generate(),
      {viz::SinglePlaneFormat::kRGBA_8888, size, gfx::ColorSpace(),
       GrSurfaceOrigin::kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
       gpu::SHARED_IMAGE_USAGE_DISPLAY_READ},
      gpu::SyncToken(),
      /*texture_target=*/3553,
      /*is_software=*/true);

  viz::TransferableResource transfer_resource = viz::TransferableResource::Make(
      client_shared_image, viz::TransferableResource::ResourceSource::kTest,
      gpu::SyncToken());
  transfer_resource.id = viz::ResourceId(10);

  viz::ResourceId resource_id = host_impl_->resource_provider()->ImportResource(
      transfer_resource, base::DoNothing());

  viz_host_impl()->CreateUIResourceFromImportedResource(
      ui_resource_id, resource_id, size, true);

  EXPECT_EQ(resource_id, host_impl_->ResourceIdForUIResource(ui_resource_id));
  EXPECT_EQ(size, host_impl_->GetUIResourceSize(ui_resource_id));
  EXPECT_TRUE(host_impl_->IsUIResourceOpaque(ui_resource_id));

  host_impl_->DeleteUIResource(ui_resource_id);
}

}  // namespace
}  // namespace cc
