// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_MOCK_DISPLAY_CLIENT_H_
#define COMPONENTS_VIZ_TEST_MOCK_DISPLAY_CLIENT_H_

#include "build/build_config.h"
#include "gpu/command_buffer/common/context_result.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_manager.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace viz {

class MockDisplayClient : public mojom::DisplayClient {
 public:
  MockDisplayClient();
  ~MockDisplayClient() override;

  mojo::PendingRemote<mojom::DisplayClient> BindRemote();

  // mojom::DisplayClient implementation.
#if defined(OS_MACOSX)
  MOCK_METHOD1(OnDisplayReceivedCALayerParams, void(const gfx::CALayerParams&));
#endif
#if defined(OS_WIN)
  MOCK_METHOD1(CreateLayeredWindowUpdater,
               void(mojo::PendingReceiver<mojom::LayeredWindowUpdater>));
#endif
#if defined(OS_ANDROID)
  MOCK_METHOD1(DidCompleteSwapWithSize, void(const gfx::Size&));
  MOCK_METHOD1(OnContextCreationResult, void(gpu::ContextResult));
  MOCK_METHOD1(SetPreferredRefreshRate, void(float refresh_rate));
#endif
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  MOCK_METHOD1(DidCompleteSwapWithNewSize, void(const gfx::Size&));
#endif

 private:
  mojo::Receiver<mojom::DisplayClient> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(MockDisplayClient);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_MOCK_DISPLAY_CLIENT_H_
