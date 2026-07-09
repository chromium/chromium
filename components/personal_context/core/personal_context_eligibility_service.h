// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_ELIGIBILITY_SERVICE_H_
#define COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_ELIGIBILITY_SERVICE_H_

#include "base/observer_list_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/personal_context/core/personal_context_types.h"

namespace personal_context {

// Service that manages the eligibility state of the Personal Context
// feature. It checks eligibility, and broadcasts state changes to observers.
//
// This is a Profile-keyed service (one instance per Profile). It is only
// available for the original (non-incognito) profile. For Incognito or Guest
// profiles, the service is not created, reflecting that Personal Context
// features are generally disabled in private browsing modes.
class PersonalContextEligibilityService : public KeyedService {
 public:
  // Observable interface for consuming features, notifies when the conditions
  // change.
  class Observer : public base::CheckedObserver {
   public:
    // Called whenever the global state changes. Can be used to track the
    // eligibility status changes and show/hide the entrypoint. Notifies
    // observers of changes to the value returned by GetEligibilityState().
    virtual void OnEligibilityStateChanged(
        PersonalContextEligibilityState new_state) = 0;
  };

  ~PersonalContextEligibilityService() override = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Sync getter for the current eligibility state. Checks whether the profile
  // is eligible to use Personal Context.
  virtual PersonalContextEligibilityState GetEligibilityState() = 0;
};

}  // namespace personal_context

#endif  // COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_ELIGIBILITY_SERVICE_H_
