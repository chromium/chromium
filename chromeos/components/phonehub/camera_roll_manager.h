// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_CAMERA_ROLL_MANAGER_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_CAMERA_ROLL_MANAGER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/components/phonehub/proto/phonehub_api.pb.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"

namespace chromeos {
namespace phonehub {

class CameraRollItem;

// Manages camera roll items sent from the connected Android device.
class CameraRollManager {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;
    // Notifies observers that camera view needs be refreshed,
    // the access state of camera roll feature is updated or current camera roll
    // items has changed.
    virtual void OnCameraRollViewUiStateUpdated();
  };

  enum class CameraRollUiState {
    // Feature is either not supported, or supported and enabled, but haven't
    // received any items yet
    SHOULD_HIDE,
    // Feature is supported by the phone but the settings hasn't been enabled on
    // system settings and not have been dismissed by user
    CAN_OPT_IN,
    // We have items that can be displayed
    ITEMS_VISIBLE,
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
  CameraRollUiState ui_state();

  // Downloads a full-quality photo or video file from the connected Android
  // device specified by the |item_metadata| to the Downloads folder.
  virtual void DownloadItem(
      const proto::CameraRollItemMetadata& item_metadata) = 0;
  // Attempt to enable camera roll feature; return whether the operation is
  // succeeded. It can only be changed via this function if the current
  // state is mojom::FeatureState::kFurtherSetupRequired or
  // mojom::FeatureState::kDisabledByUser.
  virtual void EnableCameraRollFeatureInSystemSetting() = 0;

  // Record user have dismissed the onboarding dialog.
  virtual void OnCameraRollOnboardingUiDismissed() = 0;

 protected:
  CameraRollManager();

  CameraRollUiState ui_state_ = CameraRollUiState::SHOULD_HIDE;
  void SetCurrentItems(const std::vector<CameraRollItem>& items);
  void ClearCurrentItems();
  virtual void ComputeAndUpdateUiState() = 0;
  void NotifyCameraRollViewUiStateUpdated();

 private:
  std::vector<CameraRollItem> current_items_;
  base::ObserverList<Observer> observer_list_;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_CAMERA_ROLL_MANAGER_H_
