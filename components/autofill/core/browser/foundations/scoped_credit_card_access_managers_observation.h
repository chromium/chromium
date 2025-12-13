// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_SCOPED_CREDIT_CARD_ACCESS_MANAGERS_OBSERVATION_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_SCOPED_CREDIT_CARD_ACCESS_MANAGERS_OBSERVATION_H_

#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "components/autofill/core/browser/foundations/autofill_driver_factory.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"

namespace autofill {

// `ScopedCreditCardAccessManagersObservation` allows observing all
// `CreditCardAccessManager`s of a tab, similar to what
// `ScopedAutofillManagersObservation` does for `AutofillManager`s.
class ScopedCreditCardAccessManagersObservation final
    : public AutofillDriverFactory::Observer,
      public CreditCardAccessManager::Observer {
 public:
  explicit ScopedCreditCardAccessManagersObservation(
      CreditCardAccessManager::Observer*);
  ~ScopedCreditCardAccessManagersObservation() override;

  void Observe(AutofillClient* client);
  void Observe(AutofillDriverFactory* factory);

  // Resets all observations.
  void Reset();

  // Returns whether there is an ongoing `AutofillDriverFactory` observation.
  bool IsObserving() const { return factory_observation_.IsObserving(); }

 private:
  void AddObservation(CreditCardAccessManager* credit_card_access_manager);

  void RemoveObservation(CreditCardAccessManager* credit_card_access_manager);

  [[nodiscard]] bool IsObserving(
      CreditCardAccessManager& credit_card_access_manager) const;

  // AutofillDriverFactory::Observer:
  void OnAutofillDriverFactoryDestroyed(
      AutofillDriverFactory& factory) override;
  void OnAutofillDriverStateChanged(
      AutofillDriverFactory& factory,
      AutofillDriver& driver,
      AutofillDriver::LifecycleState old_state,
      AutofillDriver::LifecycleState new_state) override;

  // CreditCardAccessManager::Observer
  void OnCreditCardAccessManagerDestroyed(
      CreditCardAccessManager& credit_card_access_manager) override;

  // Used to track `CreditCardAccessManager` creation.
  base::ScopedObservation<AutofillDriverFactory,
                          AutofillDriverFactory::Observer>
      factory_observation_{this};
  // Used to track `CreditCardAccessManager` destruction.
  base::ScopedMultiSourceObservation<CreditCardAccessManager,
                                     CreditCardAccessManager::Observer>
      credit_card_access_manager_observation_{this};
  // Used to forward events to the observer.
  base::ScopedMultiSourceObservation<CreditCardAccessManager,
                                     CreditCardAccessManager::Observer>
      forwarded_credit_card_access_manager_observation_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_SCOPED_CREDIT_CARD_ACCESS_MANAGERS_OBSERVATION_H_
