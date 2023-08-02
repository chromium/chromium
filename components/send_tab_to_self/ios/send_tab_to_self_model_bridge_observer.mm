// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/send_tab_to_self/ios/send_tab_to_self_model_bridge_observer.h"

#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"

namespace send_tab_to_self {

SendTabToSelfModelBridge::SendTabToSelfModelBridge(
    id<SendTabToSelfModelBridgeObserver> observer,
    SendTabToSelfModel* model)
    : observer_(observer), model_(model) {
  DCHECK(model_);
  model_->AddObserver(this);
}

SendTabToSelfModelBridge::~SendTabToSelfModelBridge() {
  model_->RemoveObserver(this);
}

void SendTabToSelfModelBridge::SendTabToSelfModelLoaded() {
  [observer_ sendTabToSelfModelLoaded:model_];
}

void SendTabToSelfModelBridge::EntriesAddedRemotely(
    const std::vector<const SendTabToSelfEntry*>& new_entries) {
  [observer_ sendTabToSelfModel:model_ didAddEntriesRemotely:new_entries];
}

void SendTabToSelfModelBridge::EntriesRemovedRemotely(
    const std::vector<std::string>& guids) {
  [observer_ sendTabToSelfModel:model_ didRemoveEntriesRemotely:guids];
}

}  // namespace send_tab_to_self
