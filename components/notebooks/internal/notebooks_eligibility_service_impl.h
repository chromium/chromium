// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NOTEBOOKS_INTERNAL_NOTEBOOKS_ELIGIBILITY_SERVICE_IMPL_H_
#define COMPONENTS_NOTEBOOKS_INTERNAL_NOTEBOOKS_ELIGIBILITY_SERVICE_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/notebooks/public/notebooks_eligibility_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace notebooks {

// The internal implementation of NotebooksEligibilityService.
class NotebooksEligibilityServiceImpl
    : public NotebooksEligibilityService,
      public signin::IdentityManager::Observer {
 public:
  // `is_profile_eligible` indicates whether the profile meets static
  // prerequisites (e.g. regular non-incognito profile).
  NotebooksEligibilityServiceImpl(bool is_profile_eligible,
                                  signin::IdentityManager* identity_manager);
  ~NotebooksEligibilityServiceImpl() override;

  NotebooksEligibilityServiceImpl(const NotebooksEligibilityServiceImpl&) =
      delete;
  NotebooksEligibilityServiceImpl& operator=(
      const NotebooksEligibilityServiceImpl&) = delete;

  // NotebooksEligibilityService:
  void AddObserver(NotebooksEligibilityService::Observer* observer) override;
  void RemoveObserver(NotebooksEligibilityService::Observer* observer) override;
  bool IsEligible() const override;
  bool IsEligibilityLoading() const override;

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

 private:
  // Evaluates current conditions to determine eligibility.
  bool ComputeEligibility() const;

  const bool is_profile_eligible_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  bool is_eligible_ = false;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
  base::ObserverList<NotebooksEligibilityService::Observer> observers_;
};

}  // namespace notebooks

#endif  // COMPONENTS_NOTEBOOKS_INTERNAL_NOTEBOOKS_ELIGIBILITY_SERVICE_IMPL_H_
