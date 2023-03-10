// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_BROWSER_MIRRORING_MEDIA_CONTROLLER_HOST_H_
#define COMPONENTS_MEDIA_ROUTER_BROWSER_MIRRORING_MEDIA_CONTROLLER_HOST_H_

#include "components/media_router/common/media_route.h"
#include "components/media_router/common/mojom/media_controller.mojom.h"
#include "components/media_router/common/mojom/media_status.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media_router {

class MirroringMediaControllerHost : public mojom::MediaStatusObserver {
 public:
  explicit MirroringMediaControllerHost(
      mojo::Remote<media_router::mojom::MediaController> mirroring_controller);
  MirroringMediaControllerHost(const MirroringMediaControllerHost&) = delete;
  MirroringMediaControllerHost& operator=(const MirroringMediaControllerHost&) =
      delete;
  ~MirroringMediaControllerHost() override;

  mojo::PendingRemote<media_router::mojom::MediaStatusObserver>
  GetMediaStatusObserverPendingRemote();

  // mojom::MediaStatusObserver:
  void OnMediaStatusUpdated(
      media_router::mojom::MediaStatusPtr status) override;

 private:
  mojo::Remote<media_router::mojom::MediaController> mirroring_controller_;
  mojo::Receiver<media_router::mojom::MediaStatusObserver> observer_receiver_{
      this};
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_BROWSER_MIRRORING_MEDIA_CONTROLLER_HOST_H_
