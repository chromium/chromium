// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_ENABLEMENT_SERVICE_H_
#define COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_ENABLEMENT_SERVICE_H_

#include "base/observer_list_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/personal_context/core/personal_context_types.h"

namespace personal_context {

// Service that manages the enablement state of the Personal Context
// feature. It checks eligibility, listens to profile preferences, and
// broadcasts state changes to observers.
//
// This is a Profile-keyed service (one instance per Profile). It is only
// available for the original (non-incognito) profile. For Incognito or Guest
// profiles, the service is not created, reflecting that Personal Context
// features are generally disabled in private browsing modes.
class PersonalContextEnablementService : public KeyedService {
 public:
  // Observable interface for consuming features, notifies when the conditions
  // change.
  class Observer : public base::CheckedObserver {
   public:
    // Called whenever the global state changes. Can be used to track the
    // enablement status changes and show/hide the entrypoint. Notifies
    // observers of changes to the value returned by GetEnablementState().
    virtual void OnEnablementStateChanged(
        PersonalContextEnablementState new_state) = 0;
  };

  ~PersonalContextEnablementService() override = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Sync getter for the current enablement state. Checks whether the profile
  // is enabled to use Personal Context. Includes feature check, eligibility
  // check, info acknowledgement OR setup completion.
  virtual PersonalContextEnablementState GetEnablementState() = 0;
};

}  // namespace personal_context

#endif  // COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_ENABLEMENT_SERVICE_H_
