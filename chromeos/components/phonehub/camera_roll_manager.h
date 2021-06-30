// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_CAMERA_ROLL_MANAGER_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_CAMERA_ROLL_MANAGER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/components/phonehub/message_receiver.h"
#include "chromeos/components/phonehub/proto/phonehub_api.pb.h"

namespace chromeos {
namespace phonehub {

class CameraRollItem;

// Manages camera roll items sent from the connected Android device.
class CameraRollManager : public MessageReceiver::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Notifies observers that the set of current camera roll items has changed.
    // The item set can be retrieved from CameraRollManager::GetCurrentItems().
    virtual void OnCameraRollItemsChanged() = 0;
  };

  CameraRollManager(MessageReceiver* message_receiver);
  CameraRollManager(const CameraRollManager&) = delete;
  CameraRollManager& operator=(const CameraRollManager&) = delete;
  ~CameraRollManager() override;

  // Returns the set of current camera roll items in the order in which they
  // should be displayed/
  std::vector<const CameraRollItem*> GetCurrentItems() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // MessageReceiver::Observer
  void OnFetchCameraRollItemsResponseReceived(
      const proto::FetchCameraRollItemsResponse& response) override;

  MessageReceiver* message_receiver_;
  std::vector<std::unique_ptr<CameraRollItem>> current_items_;
  base::ObserverList<Observer> observer_list_;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_CAMERA_ROLL_MANAGER_H_
