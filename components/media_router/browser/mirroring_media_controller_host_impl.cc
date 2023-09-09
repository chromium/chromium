// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/mirroring_media_controller_host_impl.h"

namespace media_router {

MirroringMediaControllerHostImpl::MirroringMediaControllerHostImpl(
    mojo::Remote<media_router::mojom::MediaController> mirroring_controller)
    : mirroring_controller_(std::move(mirroring_controller)) {}

MirroringMediaControllerHostImpl::~MirroringMediaControllerHostImpl() {
  // Notify that freeze info is changing, since this object is deleting and the
  // route may no longer be frozen.
  for (MirroringMediaControllerHostImpl::Observer& observer : observers_) {
    observer.OnFreezeInfoChanged();
  }
}

mojo::PendingRemote<media_router::mojom::MediaStatusObserver>
MirroringMediaControllerHostImpl::GetMediaStatusObserverPendingRemote() {
  return observer_receiver_.BindNewPipeAndPassRemote();
}

void MirroringMediaControllerHostImpl::AddObserver(
    MirroringMediaControllerHostImpl::Observer* observer) {
  CHECK(observer);
  observers_.AddObserver(observer);
}

void MirroringMediaControllerHostImpl::RemoveObserver(
    MirroringMediaControllerHostImpl::Observer* observer) {
  CHECK(observer);
  observers_.RemoveObserver(observer);
}

bool MirroringMediaControllerHostImpl::CanFreeze() const {
  return can_freeze_;
}

bool MirroringMediaControllerHostImpl::IsFrozen() const {
  return is_frozen_;
}

void MirroringMediaControllerHostImpl::Freeze() {
  // Do nothing if the user has recently tried to pause mirroring.
  if (freeze_timer_.IsRunning()) {
    return;
  }

  freeze_timer_.Start(
      FROM_HERE, kPauseDelay,
      base::BindOnce(&MirroringMediaControllerHostImpl::DoPauseController,
                     base::Unretained(this)));
}

void MirroringMediaControllerHostImpl::Unfreeze() {
  if (mirroring_controller_) {
    mirroring_controller_->Play();
  }
}

void MirroringMediaControllerHostImpl::OnMediaStatusUpdated(
    media_router::mojom::MediaStatusPtr status) {
  can_freeze_ = status->can_play_pause;
  is_frozen_ = can_freeze_ &&
               (status->play_state == mojom::MediaStatus::PlayState::PAUSED);

  for (MirroringMediaControllerHostImpl::Observer& observer : observers_) {
    observer.OnFreezeInfoChanged();
  }
}

void MirroringMediaControllerHostImpl::DoPauseController() {
  if (mirroring_controller_) {
    mirroring_controller_->Pause();
  }
}

}  // namespace media_router
