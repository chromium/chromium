// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_ONBOARDING_UI_TRACKER_IMPL_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_ONBOARDING_UI_TRACKER_IMPL_H_

#include "chromeos/components/phonehub/onboarding_ui_tracker.h"

namespace chromeos {
namespace phonehub {

// TODO(https://crbug.com/1106937): Add real implementation.
class OnboardingUiTrackerImpl : public OnboardingUiTracker {
 public:
  OnboardingUiTrackerImpl();
  ~OnboardingUiTrackerImpl() override;

 private:
  // OnboardingUiTracker:
  bool ShouldShowOnboardingUi() const override;
  void DismissSetupUi() override;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_ONBOARDING_UI_TRACKER_IMPL_H_
