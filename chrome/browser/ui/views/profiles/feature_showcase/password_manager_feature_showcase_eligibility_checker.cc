// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/feature_showcase/password_manager_feature_showcase_eligibility_checker.h"

#include <utility>

#include "base/check_deref.h"
#include "base/functional/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

PasswordManagerFeatureShowcaseEligibilityChecker::
    PasswordManagerFeatureShowcaseEligibilityChecker() = default;

PasswordManagerFeatureShowcaseEligibilityChecker::
    ~PasswordManagerFeatureShowcaseEligibilityChecker() = default;

void PasswordManagerFeatureShowcaseEligibilityChecker::CheckEligibility(
    Profile& profile,
    base::OnceCallback<void(bool)> callback) {
  if (!profile.GetPrefs()->GetBoolean(
          password_manager::prefs::kCredentialsEnableService)) {
    std::move(callback).Run(false);
    return;
  }

  PinnedToolbarActionsModel& pinned_actions_model =
      CHECK_DEREF(PinnedToolbarActionsModel::Get(&profile));
  std::move(callback).Run(
      !pinned_actions_model.Contains(kActionShowPasswordsBubbleOrPage));
}

std::string
PasswordManagerFeatureShowcaseEligibilityChecker::GetStepIdentifier() const {
  return "password-manager";
}
