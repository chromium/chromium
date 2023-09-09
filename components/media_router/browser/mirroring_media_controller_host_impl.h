// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_BROWSER_MIRRORING_MEDIA_CONTROLLER_HOST_IMPL_H_
#define COMPONENTS_MEDIA_ROUTER_BROWSER_MIRRORING_MEDIA_CONTROLLER_HOST_IMPL_H_

#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/media_router/browser/mirroring_media_controller_host.h"
#include "components/media_router/common/media_route.h"
#include "components/media_router/common/mojom/media_controller.mojom.h"
#include "components/media_router/common/mojom/media_status.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media_router {

// The delay to wait before pausing the route. This ensures that any UI has can
// be fully hidden before freezing.
constexpr base::TimeDelta kPauseDelay = base::Milliseconds(500);

// MirroringMediaControllerHostImpl is a per-MediaRoute object which hosts a
// MediaController, and passes to it commands related mirroring-specific media
// controls.
class MirroringMediaControllerHostImpl : public MirroringMediaControllerHost {
 public:
  explicit MirroringMediaControllerHostImpl(
      mojo::Remote<media_router::mojom::MediaController> mirroring_controller);
  MirroringMediaControllerHostImpl(const MirroringMediaControllerHostImpl&) =
      delete;
  MirroringMediaControllerHostImpl& operator=(
      const MirroringMediaControllerHostImpl&) = delete;
  ~MirroringMediaControllerHostImpl() override;

  // MirroringMediaControllerHost:
  mojo::PendingRemote<media_router::mojom::MediaStatusObserver>
  GetMediaStatusObserverPendingRemote() override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool CanFreeze() const override;
  bool IsFrozen() const override;
  void Freeze() override;
  void Unfreeze() override;
  void OnMediaStatusUpdated(
      media_router::mojom::MediaStatusPtr status) override;

 private:
  // Calls pause on the `mirroring_controller_` without delay.
  void DoPauseController();

  mojo::Remote<media_router::mojom::MediaController> mirroring_controller_;
  mojo::Receiver<media_router::mojom::MediaStatusObserver> observer_receiver_{
      this};

  // The current state of freeze info for the associated route, as interpreted
  // from MediaStatus updates.
  bool can_freeze_ = false;
  bool is_frozen_ = false;

  // When running, the associated route is about to freeze.
  base::OneShotTimer freeze_timer_;

  base::ObserverList<Observer> observers_;
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_BROWSER_MIRRORING_MEDIA_CONTROLLER_HOST_IMPL_H_
