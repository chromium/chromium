// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_MOCK_DISPLAY_CLIENT_H_
#define COMPONENTS_VIZ_TEST_MOCK_DISPLAY_CLIENT_H_

#include "build/build_config.h"
#include "gpu/command_buffer/common/context_result.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/viz/privileged/interfaces/compositing/frame_sink_manager.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace viz {

class MockDisplayClient : public mojom::DisplayClient {
 public:
  MockDisplayClient();
  ~MockDisplayClient() override;

  mojom::DisplayClientPtr BindInterfacePtr();

  // mojom::DisplayClient implementation.
  MOCK_METHOD1(DidSwapAfterSnapshotRequestReceived,
               void(const std::vector<ui::LatencyInfo>&));
#if defined(OS_MACOSX)
  MOCK_METHOD1(OnDisplayReceivedCALayerParams, void(const gfx::CALayerParams&));
#endif
#if defined(OS_WIN)
  MOCK_METHOD1(CreateLayeredWindowUpdater,
               void(mojom::LayeredWindowUpdaterRequest));
#endif
#if defined(OS_ANDROID)
  MOCK_METHOD1(DidCompleteSwapWithSize, void(const gfx::Size&));
  MOCK_METHOD1(OnFatalOrSurfaceContextCreationFailure,
               void(gpu::ContextResult));
#endif

 private:
  mojo::Binding<mojom::DisplayClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(MockDisplayClient);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_MOCK_DISPLAY_CLIENT_H_
