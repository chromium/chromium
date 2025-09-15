// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/foundations/scoped_autofill_managers_observation.h"

#include "base/check_op.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "components/autofill/core/browser/foundations/autofill_driver_factory.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"

namespace autofill {

ScopedAutofillManagersObservation::ScopedAutofillManagersObservation(
    AutofillManager::Observer* observer)
    : autofill_manager_observations_{observer} {}

ScopedAutofillManagersObservation::~ScopedAutofillManagersObservation() {
  Reset();
}

void ScopedAutofillManagersObservation::Observe(
    AutofillClient* client,
    InitializationPolicy initialization_policy) {
  return Observe(&client->GetAutofillDriverFactory(), initialization_policy);
}

void ScopedAutofillManagersObservation::Observe(
    AutofillDriverFactory* factory,
    InitializationPolicy initialization_policy) {
  factory_observation_.Observe(factory);
  switch (initialization_policy) {
    case InitializationPolicy::kExpectNoPreexistingManagers:
      CHECK_EQ(factory->GetExistingDrivers().size(), 0u);
      break;
    case InitializationPolicy::kObservePreexistingManagers:
      for (AutofillDriver* driver : factory->GetExistingDrivers()) {
        autofill_manager_observations_.AddObservation(
            &driver->GetAutofillManager());
      }
      break;
  }
}

void ScopedAutofillManagersObservation::Reset() {
  factory_observation_.Reset();
  autofill_manager_observations_.RemoveAllObservations();
}

AutofillDriverFactory*
ScopedAutofillManagersObservation::autofill_driver_factory() {
  return factory_observation_.GetSource();
}

void ScopedAutofillManagersObservation::OnAutofillDriverFactoryDestroyed(
    AutofillDriverFactory& factory) {
  Reset();
}

void ScopedAutofillManagersObservation::OnAutofillDriverCreated(
    AutofillDriverFactory& factory,
    AutofillDriver& driver) {
  autofill_manager_observations_.AddObservation(&driver.GetAutofillManager());
}

void ScopedAutofillManagersObservation::OnAutofillDriverStateChanged(
    AutofillDriverFactory& factory,
    AutofillDriver& driver,
    AutofillDriver::LifecycleState old_state,
    AutofillDriver::LifecycleState new_state) {
  switch (new_state) {
    case AutofillDriver::LifecycleState::kInactive:
    case AutofillDriver::LifecycleState::kActive:
    case AutofillDriver::LifecycleState::kPendingReset:
      break;
    case AutofillDriver::LifecycleState::kPendingDeletion:
      // `AutofillDriverFactory::SetLifecycleStateAndNotifyObservers` first
      // calls this method to remove the `AutofillManager` observer. Since
      // `SetLifecycleStateAndNotifyObservers` would only then notify the
      // `AutofillManager` about the state change, the observer needs to be
      // notified now, before its removal.
      autofill_manager_observations_.observer()->OnAutofillManagerStateChanged(
          driver.GetAutofillManager(), old_state, new_state);
      autofill_manager_observations_.RemoveObservation(
          &driver.GetAutofillManager());
      break;
  }
}

}  // namespace autofill
