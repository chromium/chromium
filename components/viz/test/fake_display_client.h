// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_FAKE_DISPLAY_CLIENT_H_
#define COMPONENTS_VIZ_TEST_FAKE_DISPLAY_CLIENT_H_

#include <vector>

#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/privileged/mojom/compositing/display_private.mojom.h"

namespace viz {

class FakeDisplayClient : public mojom::DisplayClient {
 public:
  FakeDisplayClient();
  ~FakeDisplayClient() override;

  mojo::PendingRemote<mojom::DisplayClient> BindRemote();

  // mojom::DisplayClient implementation.
#if defined(OS_MACOSX)
  void OnDisplayReceivedCALayerParams(
      const gfx::CALayerParams& ca_layer_params) override;
#endif

#if defined(OS_WIN)
  void CreateLayeredWindowUpdater(
      mojo::PendingReceiver<mojom::LayeredWindowUpdater> receiver) override;
#endif

#if defined(USE_X11)
  void DidCompleteSwapWithNewSize(const gfx::Size& size) override;
#endif

 private:
  mojo::Receiver<mojom::DisplayClient> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeDisplayClient);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_FAKE_DISPLAY_CLIENT_H_
