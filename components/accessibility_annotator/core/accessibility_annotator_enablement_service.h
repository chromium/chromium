// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_ENABLEMENT_SERVICE_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_ENABLEMENT_SERVICE_H_

#include "base/observer_list_types.h"
#include "components/accessibility_annotator/core/accessibility_annotator_types.h"
#include "components/keyed_service/core/keyed_service.h"

namespace accessibility_annotator {

// Service that manages the enablement state of the Accessibility Annotator
// feature. It checks eligibility, listens to profile preferences, and
// broadcasts state changes to observers.
class AccessibilityAnnotatorEnablementService : public KeyedService {
 public:
  // Observable interface for consuming features, notifies when the conditions
  // change.
  class Observer : public base::CheckedObserver {
   public:
    // Called whenever the global state changes. Can be used to track the
    // enablement status changes and show/hide the entrypoint. Notifies
    // observers of changes to the value returned by GetEnablementState().
    virtual void OnEnablementStateChanged(
        RemoteAnnotatorEnablementState new_state) = 0;
  };

  ~AccessibilityAnnotatorEnablementService() override = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Sync getter for the current enablement state. Checks whether the profile
  // is enabled to use Remote annotator. Includes feature check, eligibility
  // check, info acknowledgement OR setup completion.
  virtual RemoteAnnotatorEnablementState GetEnablementState() = 0;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_ENABLEMENT_SERVICE_H_
