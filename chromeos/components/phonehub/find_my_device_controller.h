// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_FIND_MY_DEVICE_CONTROLLER_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_FIND_MY_DEVICE_CONTROLLER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace chromeos {
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

  FindMyDeviceController(const FindMyDeviceController&) = delete;
  FindMyDeviceController& operator=(const FindMyDeviceController&) = delete;
  virtual ~FindMyDeviceController();

  // Returns whether the phone is ringing as a result of Find My Device
  // functionality. Note that this function does not return true if the phone is
  // ringing for another reason (e.g., a normal phone call).
  virtual bool IsPhoneRinging() const = 0;

  // Note: Ringing the phone via Find My Device is not a synchronous operation,
  // since it requires sending a message to the connected phone. Use the
  // observer interface to be notified of when the state changes.
  virtual void RequestNewPhoneRingingState(bool ringing) = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  friend class PhoneStatusProcessor;

  FindMyDeviceController();

  // This only sets the internal state of the whether the phone is ringin
  // and does not send a request to start ringing the the remote phone device.
  virtual void SetIsPhoneRingingInternal(bool is_phone_ringing) = 0;
  void NotifyPhoneRingingStateChanged();

 private:
  base::ObserverList<Observer> observer_list_;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_FIND_MY_DEVICE_CONTROLLER_H_
