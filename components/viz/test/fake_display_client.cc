// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/fake_display_client.h"

#include "build/build_config.h"

namespace viz {

FakeDisplayClient::FakeDisplayClient() = default;
FakeDisplayClient::~FakeDisplayClient() = default;

mojo::PendingRemote<mojom::DisplayClient> FakeDisplayClient::BindRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

#if BUILDFLAG(IS_APPLE)
void FakeDisplayClient::OnDisplayReceivedCALayerParams(
    const gfx::CALayerParams& ca_layer_params) {}
#endif

#if BUILDFLAG(IS_WIN)
void FakeDisplayClient::CreateLayeredWindowUpdater(
    mojo::PendingReceiver<mojom::LayeredWindowUpdater> receiver) {}
void FakeDisplayClient::AddChildWindowToBrowser(
    gpu::SurfaceHandle child_window) {}
#endif

#if BUILDFLAG(IS_LINUX) && BUILDFLAG(IS_OZONE_X11)
void FakeDisplayClient::DidCompleteSwapWithNewSize(const gfx::Size& size) {}
#endif  // BUILDFLAG(IS_LINUX) && BUILDFLAG(IS_OZONE_X11)

#if BUILDFLAG(IS_CHROMEOS_ASH)
void FakeDisplayClient::SetPreferredRefreshRate(float refresh_rate) {}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace viz
