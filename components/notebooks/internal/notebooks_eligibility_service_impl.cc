// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notebooks/internal/notebooks_eligibility_service_impl.h"

#include "base/feature_list.h"
#include "components/notebooks/public/features.h"

namespace notebooks {

NotebooksEligibilityServiceImpl::NotebooksEligibilityServiceImpl(
    bool is_profile_eligible)
    : is_profile_eligible_(is_profile_eligible) {}

NotebooksEligibilityServiceImpl::~NotebooksEligibilityServiceImpl() = default;

void NotebooksEligibilityServiceImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void NotebooksEligibilityServiceImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool NotebooksEligibilityServiceImpl::IsEligible() const {
  if (!is_profile_eligible_) {
    return false;
  }
  if (!base::FeatureList::IsEnabled(features::kNotebooks)) {
    return false;
  }
  return true;
}

bool NotebooksEligibilityServiceImpl::IsEligibilityLoading() const {
  return false;
}

}  // namespace notebooks
