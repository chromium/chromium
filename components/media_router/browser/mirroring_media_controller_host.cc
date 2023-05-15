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

void MirroringMediaControllerHost::Freeze() {
  if (mirroring_controller_) {
    mirroring_controller_->Pause();
  }
}

void MirroringMediaControllerHost::Unfreeze() {
  if (mirroring_controller_) {
    mirroring_controller_->Play();
  }
}

void MirroringMediaControllerHost::OnMediaStatusUpdated(
    media_router::mojom::MediaStatusPtr status) {
  can_freeze_ = status->can_play_pause;
  is_frozen_ = can_freeze_ &&
               (status->play_state == mojom::MediaStatus::PlayState::PAUSED);

  for (MirroringMediaControllerHost::Observer& observer : observers_) {
    observer.OnFreezeInfoChanged();
  }
}

}  // namespace media_router
