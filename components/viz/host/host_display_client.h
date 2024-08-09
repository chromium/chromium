// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_HOST_HOST_DISPLAY_CLIENT_H_
#define COMPONENTS_VIZ_HOST_HOST_DISPLAY_CLIENT_H_

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/viz/host/viz_host_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/privileged/mojom/compositing/display_private.mojom.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/gfx/native_widget_types.h"

namespace viz {

class LayeredWindowUpdaterImpl;

// mojom::DisplayClient implementation that relays calls to platform specific
// functions.
class VIZ_HOST_EXPORT HostDisplayClient : public mojom::DisplayClient {
 public:
  explicit HostDisplayClient(gfx::AcceleratedWidget widget);

  HostDisplayClient(const HostDisplayClient&) = delete;
  HostDisplayClient& operator=(const HostDisplayClient&) = delete;

  ~HostDisplayClient() override;

  mojo::PendingRemote<mojom::DisplayClient> GetBoundRemote(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

 protected:
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)
  gfx::AcceleratedWidget widget() const { return widget_; }
#endif

 private:
  // mojom::DisplayClient implementation:
#if BUILDFLAG(IS_APPLE)
  void OnDisplayReceivedCALayerParams(
      const gfx::CALayerParams& ca_layer_params) override;
#endif

#if BUILDFLAG(IS_WIN)
  void CreateLayeredWindowUpdater(
      mojo::PendingReceiver<mojom::LayeredWindowUpdater> receiver) override;
  void AddChildWindowToBrowser(gpu::SurfaceHandle child_window) override;
#endif

#if BUILDFLAG(IS_LINUX) && BUILDFLAG(IS_OZONE_X11)
  void DidCompleteSwapWithNewSize(const gfx::Size& size) override;
#endif  // BUILDFLAG(IS_LINUX) && BUILDFLAG(IS_OZONE_X11)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetPreferredRefreshRate(float refresh_rate) override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  mojo::Receiver<mojom::DisplayClient> receiver_{this};
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)
  gfx::AcceleratedWidget widget_;
#endif

#if BUILDFLAG(IS_WIN)
  std::unique_ptr<LayeredWindowUpdaterImpl> layered_window_updater_;
#endif
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_HOST_HOST_DISPLAY_CLIENT_H_
