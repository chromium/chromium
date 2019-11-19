// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/fake_display_client.h"

namespace viz {

FakeDisplayClient::FakeDisplayClient() = default;
FakeDisplayClient::~FakeDisplayClient() = default;

mojo::PendingRemote<mojom::DisplayClient> FakeDisplayClient::BindRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

#if defined(OS_MACOSX)
void FakeDisplayClient::OnDisplayReceivedCALayerParams(
    const gfx::CALayerParams& ca_layer_params) {}
#endif

#if defined(OS_WIN)
void FakeDisplayClient::CreateLayeredWindowUpdater(
    mojo::PendingReceiver<mojom::LayeredWindowUpdater> receiver) {}
#endif

#if defined(USE_X11)
void FakeDisplayClient::DidCompleteSwapWithNewSize(const gfx::Size& size) {}
#endif

}  // namespace viz
