// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_ONBOARDING_UI_TRACKER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_ONBOARDING_UI_TRACKER_H_

#include "chromeos/ash/components/phonehub/onboarding_ui_tracker.h"

namespace ash {
namespace phonehub {

class FakeOnboardingUiTracker : public OnboardingUiTracker {
 public:
  FakeOnboardingUiTracker();
  ~FakeOnboardingUiTracker() override;

  void SetShouldShowOnboardingUi(bool should_show_onboarding_ui);

  // OnboardingUiTracker:
  bool ShouldShowOnboardingUi() const override;

  size_t handle_get_started_call_count() {
    return handle_get_started_call_count_;
  }

  bool is_icon_clicked_when_nudge_visible() {
    return is_icon_clicked_when_nudge_visible_;
  }

 private:
  // OnboardingUiTracker:
  void DismissSetupUi() override;
  void HandleGetStarted(bool is_icon_clicked_when_nudge_visible) override;

  bool should_show_onboarding_ui_ = false;
  size_t handle_get_started_call_count_ = 0;
  bool is_icon_clicked_when_nudge_visible_ = false;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_ONBOARDING_UI_TRACKER_H_
