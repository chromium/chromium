// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/fake_onboarding_ui_tracker.h"

namespace ash {
namespace phonehub {

FakeOnboardingUiTracker::FakeOnboardingUiTracker() = default;

FakeOnboardingUiTracker::~FakeOnboardingUiTracker() = default;

void FakeOnboardingUiTracker::SetShouldShowOnboardingUi(
    bool should_show_onboarding_ui) {
  if (should_show_onboarding_ui_ == should_show_onboarding_ui)
    return;

  should_show_onboarding_ui_ = should_show_onboarding_ui;
  NotifyShouldShowOnboardingUiChanged();
}

bool FakeOnboardingUiTracker::ShouldShowOnboardingUi() const {
  return should_show_onboarding_ui_;
}

void FakeOnboardingUiTracker::DismissSetupUi() {
  SetShouldShowOnboardingUi(false);
}

void FakeOnboardingUiTracker::HandleGetStarted() {
  ++handle_get_started_call_count_;
}

}  // namespace phonehub
}  // namespace ash
