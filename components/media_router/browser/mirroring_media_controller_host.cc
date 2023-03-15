// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/mirroring_media_controller_host.h"

namespace media_router {

MirroringMediaControllerHost::MirroringMediaControllerHost(
    mojo::Remote<media_router::mojom::MediaController> mirroring_controller)
    : mirroring_controller_(std::move(mirroring_controller)) {}

MirroringMediaControllerHost::~MirroringMediaControllerHost() {
  // Notify that freeze info is changing, since this object is deleting and the
  // route may no longer be frozen.
  for (MirroringMediaControllerHost::Observer& observer : observers_) {
    observer.OnFreezeInfoChanged();
  }
}

mojo::PendingRemote<media_router::mojom::MediaStatusObserver>
MirroringMediaControllerHost::GetMediaStatusObserverPendingRemote() {
  return observer_receiver_.BindNewPipeAndPassRemote();
}

void MirroringMediaControllerHost::AddObserver(
    MirroringMediaControllerHost::Observer* observer) {
  CHECK(observer);
  observers_.AddObserver(observer);
}

void MirroringMediaControllerHost::RemoveObserver(
    MirroringMediaControllerHost::Observer* observer) {
  CHECK(observer);
  observers_.RemoveObserver(observer);
}

// TODO(b/271442872): Update these two method once freeze and unfreeze are
// implemented in MediaController.
void MirroringMediaControllerHost::Freeze() {}

void MirroringMediaControllerHost::Unfreeze() {}

void MirroringMediaControllerHost::OnMediaStatusUpdated(
    media_router::mojom::MediaStatusPtr status) {
  // TODO(b/271442872): Once freeze info is implemented in MediaStatus, set
  // can_freeze_ and is_frozen_ and update observers.
  for (MirroringMediaControllerHost::Observer& observer : observers_) {
    observer.OnFreezeInfoChanged();
  }
}

}  // namespace media_router
