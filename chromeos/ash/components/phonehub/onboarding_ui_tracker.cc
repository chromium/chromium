// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/onboarding_ui_tracker.h"

namespace ash {
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
}  // namespace ash
