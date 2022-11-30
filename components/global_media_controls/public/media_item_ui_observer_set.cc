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

void MediaItemUIObserverSet::OnMediaItemUIClicked(const std::string& id) {
  owner_->OnMediaItemUIClicked(id);
}

void MediaItemUIObserverSet::OnMediaItemUIDismissed(const std::string& id) {
  owner_->OnMediaItemUIDismissed(id);
}

void MediaItemUIObserverSet::OnMediaItemUIDestroyed(const std::string& id) {
  owner_->OnMediaItemUIDestroyed(id);
  StopObserving(id);
}

}  // namespace global_media_controls
