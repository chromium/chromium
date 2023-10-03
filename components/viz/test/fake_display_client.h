// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_FAKE_DISPLAY_CLIENT_H_
#define COMPONENTS_VIZ_TEST_FAKE_DISPLAY_CLIENT_H_

#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/privileged/mojom/compositing/display_private.mojom.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/buildflags.h"
#endif

namespace viz {

class FakeDisplayClient : public mojom::DisplayClient {
 public:
  FakeDisplayClient();

  FakeDisplayClient(const FakeDisplayClient&) = delete;
  FakeDisplayClient& operator=(const FakeDisplayClient&) = delete;

  ~FakeDisplayClient() override;

  mojo::PendingRemote<mojom::DisplayClient> BindRemote();

  // mojom::DisplayClient implementation.
#if BUILDFLAG(IS_APPLE)
  void OnDisplayReceivedCALayerParams(
      const gfx::CALayerParams& ca_layer_params) override;
#endif

#if BUILDFLAG(IS_WIN)
  void CreateLayeredWindowUpdater(
      mojo::PendingReceiver<mojom::LayeredWindowUpdater> receiver) override;
  void AddChildWindowToBrowser(gpu::SurfaceHandle child_window) override;
#endif

#if BUILDFLAG(IS_OZONE)
#if BUILDFLAG(OZONE_PLATFORM_X11)
  void DidCompleteSwapWithNewSize(const gfx::Size& size) override;
#endif  // BUILDFLAG(OZONE_PLATFORM_X11)
#endif  // BUILFFLAG(IS_OZONE)

 private:
  mojo::Receiver<mojom::DisplayClient> receiver_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_FAKE_DISPLAY_CLIENT_H_
