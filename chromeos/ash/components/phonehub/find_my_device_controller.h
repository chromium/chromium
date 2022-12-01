// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_FIND_MY_DEVICE_CONTROLLER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_FIND_MY_DEVICE_CONTROLLER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace ash {
namespace phonehub {

// Provides functionality for ringing the connected phone via the Find My Device
// feature.
class FindMyDeviceController {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    virtual void OnPhoneRingingStateChanged() = 0;
  };

  enum class Status {
    // Ringing is not available if the phone's DoNotDisturb mode is enabled.
    // To re-enable ringing, DoNotDisturb mode must be disabled.
    kRingingNotAvailable = 0,
    // The connected phone is not currently ringing.
    kRingingOff = 1,
    // The connected phone is currently ringing.
    kRingingOn = 2,
  };

  FindMyDeviceController(const FindMyDeviceController&) = delete;
  FindMyDeviceController& operator=(const FindMyDeviceController&) = delete;
  virtual ~FindMyDeviceController();

  // Note: Ringing the phone via Find My Device is not a synchronous operation,
  // since it requires sending a message to the connected phone. Use the
  // observer interface to be notified of when the state changes.
  virtual void RequestNewPhoneRingingState(bool ringing) = 0;

  // Returns the current ringing state of the connected phone. There are three
  // possible states (on, off, disabled). The status is a result of Find My
  // Device Functionality. Note that this function does not return true if the
  // phone is ringing for another reason (e.g., a normal phone call)
  virtual Status GetPhoneRingingStatus() = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  friend class PhoneStatusProcessor;

  FindMyDeviceController();

  // This only sets the internal state of the whether the phone is ringing
  // and does not send a request to start ringing the the remote phone device.
  virtual void SetPhoneRingingStatusInternal(Status status) = 0;
  void NotifyPhoneRingingStateChanged();

 private:
  base::ObserverList<Observer> observer_list_;
};

std::ostream& operator<<(std::ostream& stream,
                         FindMyDeviceController::Status status);

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_FIND_MY_DEVICE_CONTROLLER_H_
