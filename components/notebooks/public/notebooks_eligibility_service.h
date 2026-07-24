// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NOTEBOOKS_PUBLIC_NOTEBOOKS_ELIGIBILITY_SERVICE_H_
#define COMPONENTS_NOTEBOOKS_PUBLIC_NOTEBOOKS_ELIGIBILITY_SERVICE_H_

#include "base/observer_list_types.h"
#include "components/keyed_service/core/keyed_service.h"

namespace notebooks {

// Manages and reports the user's eligibility for the Notebooks feature.
class NotebooksEligibilityService : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when the user's eligibility status changes or finishes loading.
    virtual void OnNotebooksEligibilityChanged(bool eligible) = 0;
  };

  NotebooksEligibilityService() = default;
  ~NotebooksEligibilityService() override = default;

  NotebooksEligibilityService(const NotebooksEligibilityService&) = delete;
  NotebooksEligibilityService& operator=(const NotebooksEligibilityService&) =
      delete;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns true if the user is eligible for Notebooks.
  // Returns false if ineligible OR if eligibility is still loading.
  virtual bool IsEligible() const = 0;

  // Returns true while the initial eligibility check is still in progress.
  virtual bool IsEligibilityLoading() const = 0;
};

}  // namespace notebooks

#endif  // COMPONENTS_NOTEBOOKS_PUBLIC_NOTEBOOKS_ELIGIBILITY_SERVICE_H_
