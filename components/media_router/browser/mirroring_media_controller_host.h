// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_BROWSER_MIRRORING_MEDIA_CONTROLLER_HOST_H_
#define COMPONENTS_MEDIA_ROUTER_BROWSER_MIRRORING_MEDIA_CONTROLLER_HOST_H_

#include "base/observer_list.h"
#include "components/media_router/common/media_route.h"
#include "components/media_router/common/mojom/media_controller.mojom.h"
#include "components/media_router/common/mojom/media_status.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media_router {

// MirroringMediaControllerHost is a per-MediaRoute object which hosts a
// MediaController, and passes to it commands related mirroring-specific media
// controls.
class MirroringMediaControllerHost : public mojom::MediaStatusObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Invoked when a mirroring route's ability to freeze/unfreeze changes, or
    // when the freeze state of the route changes.
    virtual void OnFreezeInfoChanged() {}

   protected:
    ~Observer() override = default;
  };

  explicit MirroringMediaControllerHost(
      mojo::Remote<media_router::mojom::MediaController> mirroring_controller);
  MirroringMediaControllerHost(const MirroringMediaControllerHost&) = delete;
  MirroringMediaControllerHost& operator=(const MirroringMediaControllerHost&) =
      delete;
  ~MirroringMediaControllerHost() override;

  mojo::PendingRemote<media_router::mojom::MediaStatusObserver>
  GetMediaStatusObserverPendingRemote();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  bool can_freeze() const { return can_freeze_; }
  bool is_frozen() const { return is_frozen_; }

  void Freeze();
  void Unfreeze();

  // mojom::MediaStatusObserver:
  void OnMediaStatusUpdated(
      media_router::mojom::MediaStatusPtr status) override;

 private:
  mojo::Remote<media_router::mojom::MediaController> mirroring_controller_;
  mojo::Receiver<media_router::mojom::MediaStatusObserver> observer_receiver_{
      this};

  // The current state of freeze info for the associated route, as interpreted
  // from MediaStatus updates.
  bool can_freeze_ = false;
  bool is_frozen_ = false;

  base::ObserverList<Observer> observers_;
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_BROWSER_MIRRORING_MEDIA_CONTROLLER_HOST_H_
