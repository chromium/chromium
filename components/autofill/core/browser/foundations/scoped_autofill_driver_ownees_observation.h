// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_SCOPED_AUTOFILL_DRIVER_OWNEES_OBSERVATION_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_SCOPED_AUTOFILL_DRIVER_OWNEES_OBSERVATION_H_

#include <concepts>
#include <functional>
#include <utility>

#include "base/check.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_driver_factory.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"

namespace autofill {

// `ScopedAutofillDriverOwneesObservation` is a base class for building
// `ScopedObservation`-like objects that allow safe observation of objects whose
// lifetime is tied to that of an `AutofillDriver` without having to worry about
// frame creation and destruction.
//
// You most likely do not want to instantiate
// `ScopedAutofillDriverOwneesObservation` directly, but use classes such as
// `ScopedAutofillManagersObservation` instead.
template <typename Observee,
          typename ExternalObserver,
          typename OwneeFromDriver>
  requires(std::invocable<OwneeFromDriver, AutofillDriver&> &&
           std::same_as<std::invoke_result_t<OwneeFromDriver, AutofillDriver&>,
                        Observee&>)
class ScopedAutofillDriverOwneesObservation
    : public AutofillDriverFactory::Observer {
 public:
  explicit ScopedAutofillDriverOwneesObservation(
      ExternalObserver* observer,
      OwneeFromDriver ownee_from_driver = {})
      : driverlike_observations_(observer),
        ownee_from_driver_(std::move(ownee_from_driver)) {}
  ~ScopedAutofillDriverOwneesObservation() override { Reset(); }

  // Starts observing objects whose lifetimes are tied to that of an
  // `AutofillDriver`.
  //
  // The `initialization_policy` defines how to handle objects that already
  // exist at the time the observation starts.
  // - If the policy is `kExpectNoPreexistingObjects`, then `this` CHECKS that
  //   no nobjects exist at the start of the observation.
  // - If the policy is `kObservePreexistingObjects`, then `this` also starts
  //   observing those objects.
  enum class InitializationPolicy {
    kExpectNoPreexistingObjects,
    kObservePreexistingObjects,
  };
  void Observe(AutofillClient* client,
               InitializationPolicy initialization_policy =
                   InitializationPolicy::kExpectNoPreexistingObjects) {
    Observe(&client->GetAutofillDriverFactory(), initialization_policy);
  }
  void Observe(AutofillDriverFactory* factory,
               InitializationPolicy initialization_policy =
                   InitializationPolicy::kExpectNoPreexistingObjects) {
    factory_observation_.Observe(factory);
    switch (initialization_policy) {
      case InitializationPolicy::kExpectNoPreexistingObjects:
        CHECK_EQ(factory->GetExistingDrivers().size(), 0u);
        break;
      case InitializationPolicy::kObservePreexistingObjects:
        for (AutofillDriver* driver : factory->GetExistingDrivers()) {
          driverlike_observations_.AddObservation(&GetOwnee(*driver));
        }
        break;
    }
  }

  // Resets all observations.
  void Reset() {
    factory_observation_.Reset();
    driverlike_observations_.RemoveAllObservations();
  }

  AutofillDriverFactory* autofill_driver_factory() {
    return factory_observation_.GetSource();
  }

 private:
  // AutofillDriverFactory::Observer:
  void OnAutofillDriverFactoryDestroyed(
      AutofillDriverFactory& factory) override {
    Reset();
  }
  void OnAutofillDriverCreated(AutofillDriverFactory& factory,
                               AutofillDriver& driver) override {
    driverlike_observations_.AddObservation(&GetOwnee(driver));
  }
  void OnAutofillDriverStateChanged(
      AutofillDriverFactory& factory,
      AutofillDriver& driver,
      AutofillDriver::LifecycleState old_state,
      AutofillDriver::LifecycleState new_state) override {
    switch (new_state) {
      case AutofillDriver::LifecycleState::kInactive:
      case AutofillDriver::LifecycleState::kActive:
      case AutofillDriver::LifecycleState::kPendingReset:
        break;
      case AutofillDriver::LifecycleState::kPendingDeletion: {
        if constexpr (std::same_as<ExternalObserver,
                                   AutofillManager::Observer>) {
          // AutofillDriverFactory::SetLifecycleStateAndNotifyObservers()
          // notifies `AutofillDriver` before it notifies `AutofillManager` -
          // that is, it calls `OnAutofillDriverStateChanged()` (this function)
          // before `OnAutofillManagerStateChanged()`.
          //
          // Our removal preempts the `OnAutofillManagerStateChanged()`
          // notification. Therefore, we explicitly notify
          // `OnAutofillManagerStateChanged()` before the removal.
          driverlike_observations_.observer()->OnAutofillManagerStateChanged(
              GetOwnee(driver), old_state, new_state);
        }
        driverlike_observations_.RemoveObservation(&GetOwnee(driver));
        break;
      }
    }
  }

  Observee& GetOwnee(AutofillDriver& driver) {
    return std::invoke(ownee_from_driver_, driver);
  }

  // The observation used to track driver creation and destruction.
  base::ScopedObservation<AutofillDriverFactory,
                          AutofillDriverFactory::Observer>
      factory_observation_{this};
  // The observation used to forward events to `observer_`.
  base::ScopedMultiSourceObservation<Observee, ExternalObserver>
      driverlike_observations_;
  const OwneeFromDriver ownee_from_driver_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_SCOPED_AUTOFILL_DRIVER_OWNEES_OBSERVATION_H_
