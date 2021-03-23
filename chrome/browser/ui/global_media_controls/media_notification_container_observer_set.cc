// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/media_notification_container_observer_set.h"

MediaNotificationContainerObserverSet::MediaNotificationContainerObserverSet(
    MediaNotificationContainerObserver* owner)
    : owner_(owner) {}

MediaNotificationContainerObserverSet::
    ~MediaNotificationContainerObserverSet() {
  for (auto container_pair : observed_containers_)
    container_pair.second->RemoveObserver(this);
}

void MediaNotificationContainerObserverSet::Observe(
    const std::string& id,
    MediaNotificationContainerImpl* container) {
  container->AddObserver(this);
  observed_containers_[id] = container;
}

void MediaNotificationContainerObserverSet::StopObserving(
    const std::string& id) {
  auto observed_iter = observed_containers_.find(id);
  if (observed_iter != observed_containers_.end()) {
    observed_iter->second->RemoveObserver(this);
    observed_containers_.erase(observed_iter);
  }
}

void MediaNotificationContainerObserverSet::OnContainerSizeChanged() {
  owner_->OnContainerSizeChanged();
}

void MediaNotificationContainerObserverSet::OnContainerMetadataChanged() {
  owner_->OnContainerMetadataChanged();
}

void MediaNotificationContainerObserverSet::OnContainerActionsChanged() {
  owner_->OnContainerActionsChanged();
}

void MediaNotificationContainerObserverSet::OnContainerClicked(
    const std::string& id) {
  owner_->OnContainerClicked(id);
}

void MediaNotificationContainerObserverSet::OnContainerDismissed(
    const std::string& id) {
  owner_->OnContainerDismissed(id);
}

void MediaNotificationContainerObserverSet::OnContainerDestroyed(
    const std::string& id) {
  owner_->OnContainerDestroyed(id);
  StopObserving(id);
}

void MediaNotificationContainerObserverSet::OnContainerDraggedOut(
    const std::string& id,
    gfx::Rect bounds) {
  owner_->OnContainerDraggedOut(id, std::move(bounds));
}

void MediaNotificationContainerObserverSet::OnAudioSinkChosen(
    const std::string& id,
    const std::string& sink_id) {
  owner_->OnAudioSinkChosen(id, sink_id);
}
