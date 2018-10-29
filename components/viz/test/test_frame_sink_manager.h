// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_TEST_FRAME_SINK_MANAGER_H_
#define COMPONENTS_VIZ_TEST_TEST_FRAME_SINK_MANAGER_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/viz/privileged/interfaces/compositing/frame_sink_manager.mojom.h"

namespace viz {

class TestFrameSinkManagerImpl : public mojom::FrameSinkManager {
 public:
  TestFrameSinkManagerImpl();
  ~TestFrameSinkManagerImpl() override;

  void BindRequest(mojom::FrameSinkManagerRequest request,
                   mojom::FrameSinkManagerClientPtr client);

 private:
  // mojom::FrameSinkManager:
  void RegisterFrameSinkId(const FrameSinkId& frame_sink_id,
                           bool report_activation) override {}
  void InvalidateFrameSinkId(const FrameSinkId& frame_sink_id) override {}
  void EnableSynchronizationReporting(
      const FrameSinkId& frame_sink_id,
      const std::string& reporting_label) override {}
  void SetFrameSinkDebugLabel(const FrameSinkId& frame_sink_id,
                              const std::string& debug_label) override {}
  void CreateRootCompositorFrameSink(
      mojom::RootCompositorFrameSinkParamsPtr params) override {}
  void CreateCompositorFrameSink(
      const FrameSinkId& frame_sink_id,
      mojom::CompositorFrameSinkRequest request,
      mojom::CompositorFrameSinkClientPtr client) override {}
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
      mojom::VideoDetectorObserverPtr observer) override {}
  void CreateVideoCapturer(
      mojom::FrameSinkVideoCapturerRequest request) override {}
  void EvictSurfaces(const std::vector<SurfaceId>& surface_ids) override {}
  void RequestCopyOfOutput(
      const SurfaceId& surface_id,
      std::unique_ptr<CopyOutputRequest> request) override {}

  mojo::Binding<mojom::FrameSinkManager> binding_;
  mojom::FrameSinkManagerClientPtr client_;

  DISALLOW_COPY_AND_ASSIGN(TestFrameSinkManagerImpl);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_TEST_FRAME_SINK_MANAGER_H_
