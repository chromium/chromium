// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/lacros_first_run_signed_in_flow_controller.h"

#include "base/logging.h"
#include "base/notreached.h"

LacrosFirstRunSignedInFlowController::LacrosFirstRunSignedInFlowController(
    ProfilePickerWebContentsHost* host,
    Profile* profile,
    std::unique_ptr<content::WebContents> contents,
    absl::optional<SkColor> profile_color,
    OnboardingFinishedCallback onboarding_finished_callback)
    : ProfilePickerSignedInFlowController(host,
                                          profile,
                                          std::move(contents),
                                          profile_color),
      onboarding_finished_callback_(std::move(onboarding_finished_callback)) {}

LacrosFirstRunSignedInFlowController::~LacrosFirstRunSignedInFlowController() {
  // Call the callback if not called yet (unless the flow has been canceled).
  if (onboarding_finished_callback_)
    std::move(onboarding_finished_callback_).Run(BrowserOpenedCallback());
}

void LacrosFirstRunSignedInFlowController::Cancel() {
  // If the flow gets canceled in the first (welcome) screen, it is not
  // considered as finished (and thus the callback should not get called).
  onboarding_finished_callback_.Reset();
}

void LacrosFirstRunSignedInFlowController::FinishAndOpenBrowser(
    BrowserOpenedCallback callback) {
  // TODO(crbug.com/1300109): Ask crosapi for extended account info on
  // construction and renaming the profile here.
  std::move(onboarding_finished_callback_).Run(std::move(callback));
}
