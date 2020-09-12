// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/onboarding_ui_tracker.h"

namespace chromeos {
namespace phonehub {

OnboardingUiTracker::OnboardingUiTracker() = default;

OnboardingUiTracker::~OnboardingUiTracker() = default;

void OnboardingUiTracker::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void OnboardingUiTracker::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void OnboardingUiTracker::NotifyShouldShowOnboardingUiChanged() {
  for (auto& observer : observer_list_)
    observer.OnShouldShowOnboardingUiChanged();
}

}  // namespace phonehub
}  // namespace chromeos
