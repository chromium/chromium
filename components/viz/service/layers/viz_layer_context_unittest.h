// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_LAYERS_VIZ_LAYER_CONTEXT_UNITTEST_H_
#define COMPONENTS_VIZ_SERVICE_LAYERS_VIZ_LAYER_CONTEXT_UNITTEST_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "cc/base/features.h"
#include "cc/mojo_embedder/viz_layer_context.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/test_task_graph_runner.h"
#include "components/viz/common/performance_hint_utils.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"
#include "services/viz/public/mojom/compositing/layer_context.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace viz {

class FakeLayerContext : public mojom::LayerContext {
 public:
  FakeLayerContext();
  ~FakeLayerContext() override;

  void Bind(mojo::PendingAssociatedReceiver<mojom::LayerContext> receiver);

  // mojom::LayerContext:
  void SetVisible(bool visible) override;
  void UpdateDisplayTree(mojom::LayerTreeUpdatePtr update) override;
  void UpdateDisplayTiling(mojom::TilingPtr tiling) override;

  mojom::LayerTreeUpdatePtr last_update_;
  base::OnceClosure on_update_display_tree_;

 private:
  mojo::AssociatedReceiver<mojom::LayerContext> receiver_{this};
};

class FakeCompositorFrameSink : public mojom::CompositorFrameSink {
 public:
  explicit FakeCompositorFrameSink(FakeLayerContext* layer_context);
  ~FakeCompositorFrameSink() override;

  MOCK_METHOD1(SetParams, void(mojom::CompositorFrameSinkParamsPtr));
  MOCK_METHOD1(SetNeedsBeginFrame, void(bool));
  void SubmitCompositorFrame(
      const LocalSurfaceId&,
      CompositorFrame frame,
      std::optional<HitTestRegionList> hit_test_region_list,
      uint64_t) override {}
  MOCK_METHOD1(DidNotProduceFrame, void(const BeginFrameAck&));
  void BindLayerContext(mojom::PendingLayerContextPtr context,
                        mojom::LayerContextSettingsPtr settings) override;
  MOCK_METHOD0(NotifyNewLocalSurfaceIdExpectedWhilePaused, void());
  MOCK_METHOD0(InitializeCompositorFrameSinkType, void());
  MOCK_METHOD1(SetThreads, void(const std::vector<Thread>&));

 private:
  raw_ptr<FakeLayerContext> layer_context_;
};

class MyFakeLayerTreeHostImpl : public cc::FakeLayerTreeHostImpl {
 public:
  using cc::FakeLayerTreeHostImpl::FakeLayerTreeHostImpl;
  void IncrementFrameToken() {
    cc::FrameData frame_data;
    frame_data.begin_frame_ack = BeginFrameAck::CreateManualAckWithDamage();
    GenerateCompositorFrame(&frame_data);
  }
};

class VizLayerContextTest : public testing::Test {
 public:
  VizLayerContextTest();
  ~VizLayerContextTest() override;

  void SetUp() override;

  cc::LayerImpl* SetupRootLayer();

  void UpdateDisplayTreeAndWait();

  static constexpr int kRootLayerId = 1;
  static constexpr int kChildLayerId = 2;
  static constexpr int kTraceId = 1;
  static constexpr gfx::Size kDefaultSize{10, 10};
  static constexpr gfx::Rect kDefaultDamageRect{10, 10};

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  FakeLayerContext fake_layer_context_;
  FakeCompositorFrameSink fake_compositor_frame_sink_;

  cc::FakeImplTaskRunnerProvider task_runner_provider_;
  cc::TestTaskGraphRunner task_graph_runner_;
  std::unique_ptr<cc::LayerTreeFrameSink> layer_tree_frame_sink_;
  std::unique_ptr<MyFakeLayerTreeHostImpl> host_impl_;

  std::unique_ptr<cc::mojo_embedder::VizLayerContext> viz_layer_context_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_LAYERS_VIZ_LAYER_CONTEXT_UNITTEST_H_
