// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_BROWSER_MIRRORING_MEDIA_CONTROLLER_HOST_H_
#define COMPONENTS_MEDIA_ROUTER_BROWSER_MIRRORING_MEDIA_CONTROLLER_HOST_H_

#include "base/observer_list.h"
#include "components/media_router/common/mojom/media_status.mojom.h"

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
  };

  // Returns a PendingRemote bound to `this`.
  virtual mojo::PendingRemote<media_router::mojom::MediaStatusObserver>
  GetMediaStatusObserverPendingRemote() = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Freezing refers to pausing a mirroring stream on a frame.
  virtual bool CanFreeze() const = 0;
  virtual bool IsFrozen() const = 0;
  virtual void Freeze() = 0;
  virtual void Unfreeze() = 0;
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_BROWSER_MIRRORING_MEDIA_CONTROLLER_HOST_H_
