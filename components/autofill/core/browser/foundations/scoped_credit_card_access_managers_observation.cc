// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/foundations/scoped_credit_card_access_managers_observation.h"

#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "components/autofill/core/browser/foundations/autofill_driver_factory.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"

namespace autofill {

ScopedCreditCardAccessManagersObservation::
    ScopedCreditCardAccessManagersObservation(
        CreditCardAccessManager::Observer* observer)
    : forwarded_credit_card_access_manager_observation_{observer} {}

ScopedCreditCardAccessManagersObservation::
    ~ScopedCreditCardAccessManagersObservation() = default;

void ScopedCreditCardAccessManagersObservation::Observe(
    AutofillClient* client) {
  return Observe(&client->GetAutofillDriverFactory());
}

void ScopedCreditCardAccessManagersObservation::Observe(
    AutofillDriverFactory* factory) {
  factory_observation_.Observe(factory);
  for (AutofillDriver* driver : factory->GetExistingDrivers()) {
    if (CreditCardAccessManager* ccam =
            driver->GetAutofillManager().GetCreditCardAccessManager()) {
      AddObservation(ccam);
    }
  }
}

void ScopedCreditCardAccessManagersObservation::Reset() {
  factory_observation_.Reset();
  credit_card_access_manager_observation_.RemoveAllObservations();
  forwarded_credit_card_access_manager_observation_.RemoveAllObservations();
}

void ScopedCreditCardAccessManagersObservation::AddObservation(
    CreditCardAccessManager* credit_card_access_manager) {
  // The order in which observers are added is relevant: Since
  // `OnCreditCardAccessManagerDestroyed` removes the observations, we need to
  // add the forward observations first. Otherwise, they will not receive the
  // `OnCreditCardAccessManagerDestroyed` event.
  // Note that this relies on the assumption that `base::ObserverList` notifies
  // observers in the order in which they were added.
  forwarded_credit_card_access_manager_observation_.AddObservation(
      credit_card_access_manager);
  credit_card_access_manager_observation_.AddObservation(
      credit_card_access_manager);
}

void ScopedCreditCardAccessManagersObservation::RemoveObservation(
    CreditCardAccessManager* credit_card_access_manager) {
  credit_card_access_manager_observation_.RemoveObservation(
      credit_card_access_manager);
  forwarded_credit_card_access_manager_observation_.RemoveObservation(
      credit_card_access_manager);
}

bool ScopedCreditCardAccessManagersObservation::IsObserving(
    CreditCardAccessManager& credit_card_access_manager) const {
  return credit_card_access_manager_observation_.IsObservingSource(
      &credit_card_access_manager);
}

void ScopedCreditCardAccessManagersObservation::
    OnAutofillDriverFactoryDestroyed(AutofillDriverFactory& factory) {
  Reset();
}

void ScopedCreditCardAccessManagersObservation::OnAutofillDriverStateChanged(
    AutofillDriverFactory& factory,
    AutofillDriver& driver,
    AutofillDriver::LifecycleState old_state,
    AutofillDriver::LifecycleState new_state) {
  switch (new_state) {
    case AutofillDriver::LifecycleState::kInactive:
    case AutofillDriver::LifecycleState::kPendingDeletion:
    case AutofillDriver::LifecycleState::kPendingReset:
      break;
    case AutofillDriver::LifecycleState::kActive: {
      CreditCardAccessManager* ccam =
          driver.GetAutofillManager().GetCreditCardAccessManager();
      if (ccam && !IsObserving(*ccam)) {
        AddObservation(ccam);
      }
      break;
    }
  }
}

void ScopedCreditCardAccessManagersObservation::
    OnCreditCardAccessManagerDestroyed(
        CreditCardAccessManager& credit_card_access_manager) {
  if (IsObserving(credit_card_access_manager)) {
    RemoveObservation(&credit_card_access_manager);
  }
}

}  // namespace autofill
