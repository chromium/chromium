// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_MULTIDEVICE_FEATURE_ACCESS_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_MULTIDEVICE_FEATURE_ACCESS_MANAGER_H_

#include <memory>
#include <ostream>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/phonehub/combined_access_setup_operation.h"
#include "chromeos/ash/components/phonehub/feature_setup_connection_operation.h"
#include "chromeos/ash/components/phonehub/feature_setup_response_processor.h"
#include "chromeos/ash/components/phonehub/notification_access_setup_operation.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"

namespace ash {
namespace phonehub {

// Tracks the status of whether the user has granted permissions for the
// following features to be enabled on the host device:
// 1. Notification
// 2. Camera roll
//
// While Phone Hub can be enabled via Chrome OS, access to
// notifications requires that the user grant access via Android settings. If a
// Phone Hub connection to the phone has never succeeded, we assume that access
// has not yet been granted. If there is no active Phone Hub connection, we
// assume that the last access value seen is the current value.
//
// This class provides two methods for requesting access permissions on the
// connected Android device:
//
// AttemptNotificationSetup() is the legacy setup flow that only supports setup
// of the Notifications feature.
//
// AttemptCombinedFeatureSetup() is the new setup flow that supports the
// Notifications and/or Camera Roll features. New features requiring setup
// should be added to this method's flow.
class MultideviceFeatureAccessManager {
 public:
  // Status of a feature's access. Numerical values are stored in prefs and
  // should not be changed or reused.
  enum class AccessStatus {
    // Access has not been granted and is prohibited from being granted (e.g.,
    // if the phone is using a Work Profile when trying to use notification
    // fearture).
    kProhibited = 0,

    // Access has not been granted, but the user is free to grant access.
    kAvailableButNotGranted = 1,

    // Access has been granted by the user.
    kAccessGranted = 2
  };

  enum class AccessProhibitedReason {
    // Access is either not prohibited or is unset. Use as a safe default value.
    kUnknown = 0,
    // Access is prohibited because the phone is using a Work Profile and on
    // Android version <N.
    kWorkProfile = 1,
    // Access is prohibited because the phone is using a Work Profile, and the
    // policy managing the phone disables access.
    kDisabledByPhonePolicy = 2
  };

  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called when notification access has changed; use
    // GetNotificationAccessStatus() for the new status.
    virtual void OnNotificationAccessChanged();

    // Called when camera roll access has changed; use
    // GetCameraRollAccessStatus() for the new status.
    virtual void OnCameraRollAccessChanged();

    // Called when apps access has changed; use
    // GetAppsAccessStatus() for the new status.
    virtual void OnAppsAccessChanged();

    // Called when FeatureSetupRequestSupported has changed; use
    // GetFeatureSetupRequestSupported() for the new status.
    virtual void OnFeatureSetupRequestSupportedChanged();
  };

  MultideviceFeatureAccessManager(MultideviceFeatureAccessManager&) = delete;
  MultideviceFeatureAccessManager& operator=(MultideviceFeatureAccessManager&) =
      delete;
  virtual ~MultideviceFeatureAccessManager();

  virtual AccessStatus GetNotificationAccessStatus() const = 0;

  virtual AccessStatus GetCameraRollAccessStatus() const = 0;

  virtual AccessStatus GetAppsAccessStatus() const = 0;

  // Return true if the feature status is ready for requesting access. If the
  // feature requires further access permission from phone side, we shouldn't
  // send out the access request until the feature state is fuly synced.
  virtual bool IsAccessRequestAllowed(
      multidevice_setup::mojom::Feature feature) = 0;

  virtual bool GetFeatureSetupRequestSupported() const = 0;

  // Returns the reason notification access status is prohibited. The return
  // result is valid if the current access status (from GetAccessStatus())
  // is AccessStatus::kProhibited. Otherwise, the result is undefined and should
  // not be used.
  virtual AccessProhibitedReason GetNotificationAccessProhibitedReason()
      const = 0;

  virtual bool HasMultideviceFeatureSetupUiBeenDismissed() const = 0;

  // Disables the ability to show the banner within the PhoneHub UI.
  virtual void DismissSetupRequiredUi() = 0;

  // Starts an attempt to enable the notification access. |delegate| will be
  // updated with the status of the flow as long as the operation object
  // returned by this function remains instantiated.
  //
  // To cancel an ongoing setup attempt, delete the operation. If a setup
  // attempt fails, clients can retry by calling AttemptNotificationSetup()
  // again to start a new attempt.
  //
  // If notification access has already been granted, this function returns null
  // since there is nothing to set up.
  std::unique_ptr<NotificationAccessSetupOperation> AttemptNotificationSetup(
      NotificationAccessSetupOperation::Delegate* delegate);

  // Starts an attempt to enable the access for multiple features. |delegate|
  // will be updated with the status of the flow as long as the operation object
  // returned by this function remains instantiated.
  //
  // To cancel an ongoing setup attempt, delete the operation. If a setup
  // attempt fails, clients can retry by calling AttemptCombinedFeatureSetup()
  // again to start a new attempt.
  //
  // If a requested feature's access has already been granted, or the
  // FeatureSetupRequest message is not supported on the phone, this function
  // returns null.
  std::unique_ptr<CombinedAccessSetupOperation> AttemptCombinedFeatureSetup(
      bool camera_roll,
      bool notifications,
      CombinedAccessSetupOperation::Delegate* delegate);

  std::unique_ptr<FeatureSetupConnectionOperation>
  AttemptFeatureSetupConnection(
      FeatureSetupConnectionOperation::Delegate* delegate);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  MultideviceFeatureAccessManager();

  void NotifyNotificationAccessChanged();
  void NotifyCameraRollAccessChanged();
  void NotifyAppsAccessChanged();
  void NotifyFeatureSetupRequestSupportedChanged();
  void SetNotificationSetupOperationStatus(
      NotificationAccessSetupOperation::Status new_status);
  void SetCombinedSetupOperationStatus(
      CombinedAccessSetupOperation::Status new_status);
  void SetFeatureSetupConnectionOperationStatus(
      FeatureSetupConnectionOperation::Status new_status);

  bool IsNotificationSetupOperationInProgress() const;
  bool IsCombinedSetupOperationInProgress() const;
  bool IsFeatureSetupConnectionOperationInProgress() const;

  virtual void OnNotificationSetupRequested();
  virtual void OnCombinedSetupRequested(bool camera_roll, bool notifications);
  virtual void OnFeatureSetupConnectionRequested();

 private:
  friend class MultideviceFeatureAccessManagerImplTest;
  friend class PhoneStatusProcessor;
  friend class FeatureSetupResponseProcessor;

  // Sets the internal AccessStatus but does not send a request for
  // a new status to the remote phone device.
  virtual void SetNotificationAccessStatusInternal(
      AccessStatus access_status,
      AccessProhibitedReason reason) = 0;
  // Sets the internal AccessStatus but does not send a request for
  // a new status to the remote phone device.
  virtual void SetCameraRollAccessStatusInternal(
      AccessStatus access_status) = 0;
  // Sets internal status tracking if feature setup request message is
  // supported by connected phone.
  virtual void SetFeatureSetupRequestSupportedInternal(bool supported) = 0;

  virtual void UpdatedFeatureSetupConnectionStatusIfNeeded();

  void OnNotificationSetupOperationDeleted(int operation_id);
  void OnCombinedSetupOperationDeleted(int operation_id);
  void OnFeatureSetupConnectionOperationDeleted(int operation_id);

  int next_operation_id_ = 0;
  base::flat_map<int,
                 raw_ptr<NotificationAccessSetupOperation, CtnExperimental>>
      id_to_notification_operation_map_;
  base::flat_map<int, raw_ptr<CombinedAccessSetupOperation, CtnExperimental>>
      id_to_combined_operation_map_;
  base::flat_map<int, raw_ptr<FeatureSetupConnectionOperation, CtnExperimental>>
      id_to_connection_operation_map_;
  base::ObserverList<Observer> observer_list_;
  base::WeakPtrFactory<MultideviceFeatureAccessManager> weak_ptr_factory_{this};
};

std::ostream& operator<<(std::ostream& stream,
                         MultideviceFeatureAccessManager::AccessStatus status);
std::ostream& operator<<(
    std::ostream& stream,
    MultideviceFeatureAccessManager::AccessProhibitedReason reason);
std::ostream& operator<<(
    std::ostream& stream,
    std::pair<MultideviceFeatureAccessManager::AccessStatus,
              MultideviceFeatureAccessManager::AccessProhibitedReason>
        status_reason);

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_MULTIDEVICE_FEATURE_ACCESS_MANAGER_H_
