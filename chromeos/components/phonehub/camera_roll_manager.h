// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_CAMERA_ROLL_MANAGER_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_CAMERA_ROLL_MANAGER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace chromeos {
namespace phonehub {

class CameraRollItem;

// Manages camera roll items sent from the connected Android device.
class CameraRollManager {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Notifies observers that the set of current camera roll items has changed.
    // The item set can be retrieved from CameraRollManager::current_items().
    virtual void OnCameraRollItemsChanged() = 0;
  };

  CameraRollManager(const CameraRollManager&) = delete;
  CameraRollManager& operator=(const CameraRollManager&) = delete;
  virtual ~CameraRollManager();

  // Returns the set of current camera roll items in the order in which they
  // should be displayed
  const std::vector<CameraRollItem>& current_items() const {
    return current_items_;
  }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  CameraRollManager();

  void SetCurrentItems(const std::vector<CameraRollItem>& items);
  void ClearCurrentItems();

  void NotifyCameraRollItemsChanged();

 private:
  std::vector<CameraRollItem> current_items_;
  base::ObserverList<Observer> observer_list_;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_CAMERA_ROLL_MANAGER_H_
