// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/camera_roll_manager.h"

#include "base/observer_list.h"
#include "chromeos/components/phonehub/camera_roll_item.h"
#include "chromeos/components/phonehub/message_receiver.h"
#include "chromeos/components/phonehub/proto/phonehub_api.pb.h"

namespace chromeos {
namespace phonehub {

CameraRollManager::CameraRollManager(MessageReceiver* message_receiver)
    : message_receiver_(message_receiver) {
  message_receiver->AddObserver(this);
}

CameraRollManager::~CameraRollManager() {
  message_receiver_->RemoveObserver(this);
}

void CameraRollManager::OnFetchCameraRollItemsResponseReceived(
    const proto::FetchCameraRollItemsResponse& response) {
  current_items_.clear();

  // TODO(http://crbug.com/1221297): Decode thumbnail data. Existing items that
  // haven't changed won't have thumbnail data from the new proto. They need to
  // be copied from the old vector into the new one.
  for (const proto::CameraRollItem& item_proto : response.items()) {
    current_items_.push_back(
        std::make_unique<CameraRollItem>(item_proto.metadata()));
  }

  for (auto& observer : observer_list_) {
    observer.OnCameraRollItemsChanged();
  }
}

std::vector<const CameraRollItem*> CameraRollManager::GetCurrentItems() const {
  std::vector<const CameraRollItem*> items;
  for (const std::unique_ptr<CameraRollItem>& current_item : current_items_) {
    items.push_back(current_item.get());
  }
  return items;
}

void CameraRollManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void CameraRollManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

}  // namespace phonehub
}  // namespace chromeos
