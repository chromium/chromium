// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/scoped_autofill_managers_observation.h"

#include "base/check_op.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

ScopedAutofillManagersObservation::ScopedAutofillManagersObservation(
    AutofillManager::Observer* observer)
    : autofill_manager_observations_{observer} {}

ScopedAutofillManagersObservation::~ScopedAutofillManagersObservation() {
  Reset();
}

void ScopedAutofillManagersObservation::Observe(
    ContentAutofillDriverFactory* factory,
    InitializationPolicy initialization_policy) {
  factory_observation_.Observe(factory);
  switch (initialization_policy) {
    case InitializationPolicy::kExpectNoPreexistingManagers:
      CHECK_EQ(factory->num_drivers(), 0u);
      break;
    case InitializationPolicy::kObservePreexistingManagers:
      for (ContentAutofillDriver* driver : factory->GetExistingDrivers({})) {
        autofill_manager_observations_.AddObservation(
            &driver->GetAutofillManager());
      }
      break;
  }
}

void ScopedAutofillManagersObservation::Observe(
    content::WebContents* contents,
    InitializationPolicy initialization_policy) {
  Observe(ContentAutofillDriverFactory::FromWebContents(contents),
          initialization_policy);
}

void ScopedAutofillManagersObservation::Reset() {
  factory_observation_.Reset();
  autofill_manager_observations_.RemoveAllObservations();
}

content::WebContents* ScopedAutofillManagersObservation::web_contents() {
  ContentAutofillDriverFactory* factory = factory_observation_.GetSource();
  return factory ? factory->web_contents() : nullptr;
}

void ScopedAutofillManagersObservation::OnContentAutofillDriverFactoryDestroyed(
    ContentAutofillDriverFactory& factory) {
  Reset();
}

void ScopedAutofillManagersObservation::OnContentAutofillDriverCreated(
    ContentAutofillDriverFactory& factory,
    ContentAutofillDriver& driver) {
  autofill_manager_observations_.AddObservation(&driver.GetAutofillManager());
}

void ScopedAutofillManagersObservation::OnContentAutofillDriverStateChanged(
    ContentAutofillDriverFactory& factory,
    ContentAutofillDriver& driver,
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
