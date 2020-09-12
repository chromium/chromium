// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/onboarding_ui_tracker_impl.h"

namespace chromeos {
namespace phonehub {

OnboardingUiTrackerImpl::OnboardingUiTrackerImpl() = default;

OnboardingUiTrackerImpl::~OnboardingUiTrackerImpl() = default;

bool OnboardingUiTrackerImpl::ShouldShowOnboardingUi() const {
  // TODO(https://crbug.com/1106937): Return a real value.
  return false;
}

void OnboardingUiTrackerImpl::DismissSetupUi() {
  // TODO(https://crbug.com/1106937): Store this in a pref so that we do not
  // show this UI to the user again.
}

}  // namespace phonehub
}  // namespace chromeos
