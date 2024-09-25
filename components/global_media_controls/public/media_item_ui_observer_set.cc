// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/media_item_ui_observer_set.h"

namespace global_media_controls {

MediaItemUIObserverSet::MediaItemUIObserverSet(MediaItemUIObserver* owner)
    : owner_(owner) {}

MediaItemUIObserverSet::~MediaItemUIObserverSet() {
  for (auto item_ui_pair : observed_item_uis_)
    item_ui_pair.second->RemoveObserver(this);
}

void MediaItemUIObserverSet::Observe(const std::string& id,
                                     MediaItemUI* item_ui) {
  // If there is an old MediaItemUI with the same ID not fully closed, stop
  // observing it before adding the new one.
  StopObserving(id);

  item_ui->AddObserver(this);
  observed_item_uis_[id] = item_ui;
}

void MediaItemUIObserverSet::StopObserving(const std::string& id) {
  auto observed_item = observed_item_uis_.find(id);
  if (observed_item != observed_item_uis_.end()) {
    observed_item->second->RemoveObserver(this);
    observed_item_uis_.erase(observed_item);
  }
}

void MediaItemUIObserverSet::OnMediaItemUISizeChanged() {
  owner_->OnMediaItemUISizeChanged();
}

void MediaItemUIObserverSet::OnMediaItemUIMetadataChanged() {
  owner_->OnMediaItemUIMetadataChanged();
}

void MediaItemUIObserverSet::OnMediaItemUIActionsChanged() {
  owner_->OnMediaItemUIActionsChanged();
}

void MediaItemUIObserverSet::OnMediaItemUIClicked(
    const std::string& id,
    bool activate_original_media) {
  owner_->OnMediaItemUIClicked(id, activate_original_media);
}

void MediaItemUIObserverSet::OnMediaItemUIDismissed(const std::string& id) {
  owner_->OnMediaItemUIDismissed(id);
}

void MediaItemUIObserverSet::OnMediaItemUIDestroyed(const std::string& id) {
  owner_->OnMediaItemUIDestroyed(id);
  observed_item_uis_.erase(id);
}

void MediaItemUIObserverSet::OnMediaItemUIShowDevices(const std::string& id) {
  owner_->OnMediaItemUIShowDevices(id);
}

}  // namespace global_media_controls
