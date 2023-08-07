// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_ONBOARDING_UI_TRACKER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_ONBOARDING_UI_TRACKER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace ash {
namespace phonehub {

// Tracks whether Phone Hub should show a UI to ask users to opt into the multi-
// device feature suite. Note that this UI is not the setup flow itself; rather,
// it serves as an additional entry point to that flow for users who may be
// interested in Phone Hub.
//
// This UI should be shown when a user is eligible for the Phone Hub feature but
// has not yet completed setup for it. If the current user has already set up
// Phone Hub on this device, the setup UI should not be shown. The UI also
// provides an option to dismiss the UI, in which case it should not be shown
// again.
class OnboardingUiTracker {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    virtual void OnShouldShowOnboardingUiChanged() = 0;
  };

  OnboardingUiTracker(const OnboardingUiTracker&) = delete;
  OnboardingUiTracker& operator=(const OnboardingUiTracker&) = delete;
  virtual ~OnboardingUiTracker();

  // Whether the setup UI should be shown.
  virtual bool ShouldShowOnboardingUi() const = 0;

  // Disables the ability to show the setup UI; once this is called,
  // ShouldShowOnboardingUi() will no longer return true for this user on this
  // device.
  virtual void DismissSetupUi() = 0;

  // Handle for when the user clicks the get started button.
  virtual void HandleGetStarted(bool is_icon_clicked_when_nudge_visible) = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  OnboardingUiTracker();

  void NotifyShouldShowOnboardingUiChanged();

 private:
  base::ObserverList<Observer> observer_list_;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_ONBOARDING_UI_TRACKER_H_
