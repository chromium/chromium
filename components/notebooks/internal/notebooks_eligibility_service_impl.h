// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NOTEBOOKS_INTERNAL_NOTEBOOKS_ELIGIBILITY_SERVICE_IMPL_H_
#define COMPONENTS_NOTEBOOKS_INTERNAL_NOTEBOOKS_ELIGIBILITY_SERVICE_IMPL_H_

#include "base/observer_list.h"
#include "components/notebooks/public/notebooks_eligibility_service.h"

namespace notebooks {

// The internal implementation of NotebooksEligibilityService.
class NotebooksEligibilityServiceImpl : public NotebooksEligibilityService {
 public:
  // `is_profile_eligible` indicates whether the profile meets static
  // prerequisites (e.g. regular non-incognito profile).
  explicit NotebooksEligibilityServiceImpl(bool is_profile_eligible);
  ~NotebooksEligibilityServiceImpl() override;

  NotebooksEligibilityServiceImpl(const NotebooksEligibilityServiceImpl&) =
      delete;
  NotebooksEligibilityServiceImpl& operator=(
      const NotebooksEligibilityServiceImpl&) = delete;

  // NotebooksEligibilityService:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool IsEligible() const override;
  bool IsEligibilityLoading() const override;

 private:
  // Indicates if the underlying profile is eligible based on profile type.
  const bool is_profile_eligible_;
  base::ObserverList<Observer> observers_;
};

}  // namespace notebooks

#endif  // COMPONENTS_NOTEBOOKS_INTERNAL_NOTEBOOKS_ELIGIBILITY_SERVICE_IMPL_H_
