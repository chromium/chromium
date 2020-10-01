// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_DO_NOT_DISTURB_CONTROLLER_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_DO_NOT_DISTURB_CONTROLLER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace chromeos {
namespace phonehub {

// Provides DND (Do Not Disturb) functionality for the connected phone. Clients
// can check whether DND is enabled and observe when that state has changed;
// additionally, this class provides an API for setting the DND state.
class DoNotDisturbController {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    virtual void OnDndStateChanged() = 0;
  };

  DoNotDisturbController(const DoNotDisturbController&) = delete;
  DoNotDisturbController& operator=(const DoNotDisturbController&) = delete;
  virtual ~DoNotDisturbController();

  virtual bool IsDndEnabled() const = 0;

  // Note: Setting DND state is not a synchronous operation, since it requires
  // sending a message to the connected phone. Use the observer interface to be
  // notified of when the state changes.
  virtual void RequestNewDoNotDisturbState(bool enabled) = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  friend class PhoneStatusProcessor;

  DoNotDisturbController();

  // This only sets the internal state of the DoNotDisturb mode and does not
  // send a request to set the state of the remote phone device.
  virtual void SetDoNotDisturbStateInternal(bool is_dnd_enabled) = 0;

  void NotifyDndStateChanged();

 private:
  base::ObserverList<Observer> observer_list_;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_DO_NOT_DISTURB_CONTROLLER_H_
