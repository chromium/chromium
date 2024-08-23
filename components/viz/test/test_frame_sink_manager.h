// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_TEST_FRAME_SINK_MANAGER_H_
#define COMPONENTS_VIZ_TEST_TEST_FRAME_SINK_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/input/render_input_router.mojom.h"
#include "components/viz/common/surfaces/frame_sink_bundle_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_manager.mojom.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_manager_test_api.mojom.h"
#include "services/viz/privileged/mojom/compositing/frame_sinks_metrics_recorder.mojom.h"

namespace viz {

class TestFrameSinkManagerImpl : public mojom::FrameSinkManager {
 public:
  TestFrameSinkManagerImpl();

  TestFrameSinkManagerImpl(const TestFrameSinkManagerImpl&) = delete;
  TestFrameSinkManagerImpl& operator=(const TestFrameSinkManagerImpl&) = delete;

  ~TestFrameSinkManagerImpl() override;

  void BindReceiver(mojo::PendingReceiver<mojom::FrameSinkManager> receiver,
                    mojo::PendingRemote<mojom::FrameSinkManagerClient> client);

 private:
  // mojom::FrameSinkManager:
  void RegisterFrameSinkId(const FrameSinkId& frame_sink_id,
                           bool report_activation) override {}
  void InvalidateFrameSinkId(const FrameSinkId& frame_sink_id) override {}
  void SetFrameSinkDebugLabel(const FrameSinkId& frame_sink_id,
                              const std::string& debug_label) override {}
  void CreateRootCompositorFrameSink(
      mojom::RootCompositorFrameSinkParamsPtr params) override {}
  void CreateFrameSinkBundle(
      const FrameSinkBundleId& bundle_id,
      mojo::PendingReceiver<mojom::FrameSinkBundle> receiver,
      mojo::PendingRemote<mojom::FrameSinkBundleClient> client) override {}
  void CreateCompositorFrameSink(
      const FrameSinkId& frame_sink_id,
      const std::optional<FrameSinkBundleId>& bundle_id,
      mojo::PendingReceiver<mojom::CompositorFrameSink> receiver,
      mojo::PendingRemote<mojom::CompositorFrameSinkClient> client,
      input::mojom::RenderInputRouterConfigPtr render_input_router_config)
      override {}
  void DestroyCompositorFrameSink(
      const FrameSinkId& frame_sink_id,
      DestroyCompositorFrameSinkCallback callback) override {}
  void RegisterFrameSinkHierarchy(
      const FrameSinkId& parent_frame_sink_id,
      const FrameSinkId& child_frame_sink_id) override {}
  void UnregisterFrameSinkHierarchy(
      const FrameSinkId& parent_frame_sink_id,
      const FrameSinkId& child_frame_sink_id) override {}
  void AddVideoDetectorObserver(
      mojo::PendingRemote<mojom::VideoDetectorObserver> observer) override {}
  void CreateVideoCapturer(
      mojo::PendingReceiver<mojom::FrameSinkVideoCapturer> receiver) override {}
  void EvictSurfaces(const std::vector<SurfaceId>& surface_ids) override {}
  void RequestCopyOfOutput(const SurfaceId& surface_id,
                           std::unique_ptr<CopyOutputRequest> request,
                           bool capture_exact_surface_id) override {}
#if BUILDFLAG(IS_ANDROID)
  void CacheBackBuffer(uint32_t cache_id,
                       const FrameSinkId& root_frame_sink_id) override {}
  void EvictBackBuffer(uint32_t cache_id,
                       EvictBackBufferCallback callback) override {}
#endif
  void UpdateDebugRendererSettings(
      const DebugRendererSettings& debug_settings) override {}
  void Throttle(const std::vector<FrameSinkId>& ids,
                base::TimeDelta interval) override {}
  void StartThrottlingAllFrameSinks(base::TimeDelta interval) override {}
  void StopThrottlingAllFrameSinks() override {}
  void ClearUnclaimedViewTransitionResources(
      const blink::ViewTransitionToken& transition_token) override {}
  void CreateMetricsRecorderForTest(
      mojo::PendingReceiver<mojom::FrameSinksMetricsRecorder> receiver)
      override {}
  void EnableFrameSinkManagerTestApi(
      mojo::PendingReceiver<mojom::FrameSinkManagerTestApi> receiver) override {
  }

  mojo::Receiver<mojom::FrameSinkManager> receiver_{this};
  mojo::Remote<mojom::FrameSinkManagerClient> client_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_TEST_FRAME_SINK_MANAGER_H_
