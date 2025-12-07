// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_SCOPED_AUTOFILL_MANAGERS_OBSERVATION_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_SCOPED_AUTOFILL_MANAGERS_OBSERVATION_H_

#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "components/autofill/core/browser/foundations/autofill_driver_factory.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"

namespace autofill {

class AutofillDriver;

// `ScopedAutofillManagersObservation` is a helper to reduce boilerplate when
// observing all `AutofillManagers` of an `AutofillClient`. Instead of having to
// implement both `AutofillDriverFactory`'s and `AutofillManager`'s
// observer interfaces, the consumer now only needs to implement
// `AutofillManager::Observer` and add a member variable of this type.
//
// Example:
// class AutofillInformationConsumer : public AutofillManager::Observer {
//  public:
//   AutofillInformationConsumer(content::WebContents* contents) {
//     auto* client = ContentAutofillClient::FromWebContents(contents);
//     autofill_managers_observation.Observe(client);
//   }
//
//   // AutofillManager::Observer:
//   void OnAfterLanguageDetermined(AutofillManager& manager) {
//     // Do something.
//   }
//
//  private:
//   ScopedAutofillManagersObservation autofill_managers_observation_{this};
// };
class ScopedAutofillManagersObservation final
    : public AutofillDriverFactory::Observer {
 public:
  explicit ScopedAutofillManagersObservation(AutofillManager::Observer*);
  ~ScopedAutofillManagersObservation() override;

  // Starts observing a client's AutofillManagers.
  //
  // The `initialization_policy` defines how to handle `AutofillManager`s that
  // already exist at the time the observation starts.
  // - If the policy is `kExpectNoPreexistingManagers`, then `this` CHECKS that
  //   no managers exist at the start of the observation.
  // - If the policy is `kObservePreexistingManagers`, then `this` also starts
  //   observing those managers.
  //   WARNING: In this case, the usual contract for `AutofillManager::Observer`
  //   may no longer hold. Not every `OnAfterX` event will be preceded by an
  //   `OnBeforeX` event, since the observation may start after `OnBeforeX` has
  //   been emitted, but before `OnAfterX` is triggered.
  enum class InitializationPolicy {
    kExpectNoPreexistingManagers,
    kObservePreexistingManagers,
  };
  void Observe(AutofillClient* client,
               InitializationPolicy initialization_policy =
                   InitializationPolicy::kExpectNoPreexistingManagers);
  void Observe(AutofillDriverFactory* factory,
               InitializationPolicy initialization_policy =
                   InitializationPolicy::kExpectNoPreexistingManagers);

  // Resets all observations.
  void Reset();

  AutofillDriverFactory* autofill_driver_factory();

 private:
  // AutofillDriverFactory::Observer:
  void OnAutofillDriverFactoryDestroyed(
      AutofillDriverFactory& factory) override;
  void OnAutofillDriverCreated(AutofillDriverFactory& factory,
                               AutofillDriver& driver) override;
  void OnAutofillDriverStateChanged(
      AutofillDriverFactory& factory,
      AutofillDriver& driver,
      AutofillDriver::LifecycleState old_state,
      AutofillDriver::LifecycleState new_state) override;

  // The observation used to track driver creation and destruction.
  base::ScopedObservation<AutofillDriverFactory,
                          AutofillDriverFactory::Observer>
      factory_observation_{this};
  // The observation used to forward events to `observer_`.
  base::ScopedMultiSourceObservation<AutofillManager, AutofillManager::Observer>
      autofill_manager_observations_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_SCOPED_AUTOFILL_MANAGERS_OBSERVATION_H_
