// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_FACTORY_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_FACTORY_H_

#include "base/observer_list.h"
#include "components/autofill/core/browser/autofill_driver.h"

namespace autofill {

// The common interface for platform-dependent AutofillDriver factories:
// - ContentAutofillDriverFactory
// - AutofillDriverIOSFactory
class AutofillDriverFactory {
 public:
  using LifecycleState = AutofillDriver::LifecycleState;

  // Observer of AutofillDriverFactory events.
  class Observer : public base::CheckedObserver {
   public:
    // Called during destruction of the AutofillDriverFactory. It can,
    // e.g., be used to reset `ScopedObservation`s.
    virtual void OnAutofillDriverFactoryDestroyed(
        AutofillDriverFactory& factory) {}

    // Called right after the driver has been created.
    // At the time of this event, the `driver` object is already fully alive and
    // `factory.DriverForFrame(driver.render_frame_host()) == &driver` (or
    // similarly for iOS) holds. The driver's manager is still in its
    // `kInactive` state at the time.
    virtual void OnAutofillDriverCreated(AutofillDriverFactory& factory,
                                         AutofillDriver& driver) {}

    // Called right after the driver's state has changed.
    // See AutofillDriver::LifecycleState for details.
    // At the time of this event, the `driver` object is fully alive and
    // `factory.DriverForFrame(driver.render_frame_host()) == &driver` (or
    // similarly for iOS) holds.
    virtual void OnAutofillDriverStateChanged(AutofillDriverFactory& factory,
                                              AutofillDriver& driver,
                                              LifecycleState old_state,
                                              LifecycleState new_state) {}
  };

  AutofillDriverFactory();
  AutofillDriverFactory(AutofillDriverFactory&) = delete;
  AutofillDriverFactory& operator=(const AutofillDriverFactory&) = delete;
  virtual ~AutofillDriverFactory();

  void AddObserver(Observer* observer) { observers_.AddObserver(observer); }

  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

 protected:
  friend class AutofillDriverFactoryTestApi;

  void SetLifecycleStateAndNotifyObservers(AutofillDriver& driver,
                                           LifecycleState new_state);

  const base::ObserverList<Observer>& observers() { return observers_; }

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_FACTORY_H_
