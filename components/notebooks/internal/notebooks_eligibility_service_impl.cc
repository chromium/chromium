// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notebooks/internal/notebooks_eligibility_service_impl.h"

#include "base/feature_list.h"
#include "components/notebooks/public/features.h"

namespace notebooks {

NotebooksEligibilityServiceImpl::NotebooksEligibilityServiceImpl(
    bool is_profile_eligible,
    signin::IdentityManager* identity_manager)
    : is_profile_eligible_(is_profile_eligible),
      identity_manager_(identity_manager) {
  is_eligible_ = ComputeEligibility();
  if (identity_manager_) {
    identity_manager_observation_.Observe(identity_manager_.get());
  }
}

NotebooksEligibilityServiceImpl::~NotebooksEligibilityServiceImpl() = default;

void NotebooksEligibilityServiceImpl::AddObserver(
    NotebooksEligibilityService::Observer* observer) {
  observers_.AddObserver(observer);
}

void NotebooksEligibilityServiceImpl::RemoveObserver(
    NotebooksEligibilityService::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool NotebooksEligibilityServiceImpl::IsEligible() const {
  return is_eligible_;
}

bool NotebooksEligibilityServiceImpl::IsEligibilityLoading() const {
  return false;
}

bool NotebooksEligibilityServiceImpl::ComputeEligibility() const {
  if (!is_profile_eligible_) {
    return false;
  }
  if (!base::FeatureList::IsEnabled(features::kNotebooks)) {
    return false;
  }
  if (!identity_manager_ ||
      !identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return false;
  }
  return true;
}

void NotebooksEligibilityServiceImpl::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  const bool current_is_eligible = ComputeEligibility();
  if (current_is_eligible != is_eligible_) {
    is_eligible_ = current_is_eligible;
    for (NotebooksEligibilityService::Observer& observer : observers_) {
      observer.OnNotebooksEligibilityChanged(is_eligible_);
    }
  }
}

void NotebooksEligibilityServiceImpl::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  identity_manager_observation_.Reset();
  identity_manager_ = nullptr;
  if (is_eligible_) {
    is_eligible_ = false;
    for (NotebooksEligibilityService::Observer& observer : observers_) {
      observer.OnNotebooksEligibilityChanged(false);
    }
  }
}

}  // namespace notebooks
