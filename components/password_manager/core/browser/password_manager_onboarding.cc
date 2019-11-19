// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_onboarding.h"

#include "base/feature_list.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

namespace password_manager {

namespace {
// Functions converting UIDismissalReasons for the save password
// infobar and the onboarding dialog into values of the
// |OnboardingResultOfSavingFlow| enum.
metrics_util::OnboardingResultOfSavingFlow FlowResultFromInfobarDismissalReason(
    const metrics_util::UIDismissalReason reason) {
  switch (reason) {
    case metrics_util::UIDismissalReason::NO_DIRECT_INTERACTION:
      return metrics_util::OnboardingResultOfSavingFlow::
          kInfobarNoDirectInteraction;
    case metrics_util::UIDismissalReason::CLICKED_SAVE:
      return metrics_util::OnboardingResultOfSavingFlow::kInfobarClickedSave;
    case metrics_util::UIDismissalReason::CLICKED_CANCEL:
      return metrics_util::OnboardingResultOfSavingFlow::kInfobarClickedCancel;
    case metrics_util::UIDismissalReason::CLICKED_NEVER:
      return metrics_util::OnboardingResultOfSavingFlow::kInfobarClickedNever;
    default:
      NOTREACHED();
      return metrics_util::OnboardingResultOfSavingFlow::
          kInfobarNoDirectInteraction;
  }
}

metrics_util::OnboardingResultOfSavingFlow
FlowResultFromOnboardingDismissalReason(
    const metrics_util::OnboardingUIDismissalReason reason) {
  switch (reason) {
    case metrics_util::OnboardingUIDismissalReason::kRejected:
      return metrics_util::OnboardingResultOfSavingFlow::kOnboardingRejected;
    case metrics_util::OnboardingUIDismissalReason::kDismissed:
      return metrics_util::OnboardingResultOfSavingFlow::kOnboardingDismissed;
    default:
      NOTREACHED();
      return metrics_util::OnboardingResultOfSavingFlow::kOnboardingRejected;
  }
}
}  // namespace

using OnboardingState = password_manager::metrics_util::OnboardingState;

OnboardingStateUpdate::OnboardingStateUpdate(
    scoped_refptr<password_manager::PasswordStore> store,
    PrefService* prefs)
    : store_(std::move(store)), prefs_(prefs) {}

void OnboardingStateUpdate::Start() {
  store_->GetAutofillableLogins(this);
}

OnboardingStateUpdate::~OnboardingStateUpdate() = default;

void OnboardingStateUpdate::UpdateState(
    std::vector<std::unique_ptr<autofill::PasswordForm>> credentials) {
  if (credentials.size() >= kOnboardingCredentialsThreshold) {
    if (prefs_->GetInteger(prefs::kPasswordManagerOnboardingState) ==
        static_cast<int>(OnboardingState::kShouldShow)) {
      prefs_->SetInteger(prefs::kPasswordManagerOnboardingState,
                         static_cast<int>(OnboardingState::kDoNotShow));
    }
    return;
  }
  if (prefs_->GetInteger(
          password_manager::prefs::kPasswordManagerOnboardingState) ==
      static_cast<int>(OnboardingState::kDoNotShow)) {
    prefs_->SetInteger(prefs::kPasswordManagerOnboardingState,
                       static_cast<int>(OnboardingState::kShouldShow));
  }
}

void OnboardingStateUpdate::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  UpdateState(std::move(results));
  delete this;
}

// Initializes and runs the OnboardingStateUpdate class, which is
// used to update the |kPasswordManagerOnboardingState| pref.
void StartOnboardingStateUpdate(
    scoped_refptr<password_manager::PasswordStore> store,
    PrefService* prefs) {
  (new OnboardingStateUpdate(store, prefs))->Start();
}

void UpdateOnboardingState(scoped_refptr<password_manager::PasswordStore> store,
                           PrefService* prefs,
                           base::TimeDelta delay) {
  if (prefs->GetInteger(prefs::kPasswordManagerOnboardingState) ==
      static_cast<int>(OnboardingState::kShown)) {
    return;
  }
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&StartOnboardingStateUpdate, store, prefs),
      delay);
}

bool ShouldShowOnboarding(PrefService* prefs,
                          PasswordUpdateBool is_password_update,
                          BlacklistedBool is_blacklisted,
                          SyncState sync_state) {
  if (is_blacklisted) {
    return false;
  }

  if (is_password_update) {
    return false;
  }

  bool was_feature_checked_before = prefs->GetBoolean(
      password_manager::prefs::kWasOnboardingFeatureCheckedBefore);

  if (was_feature_checked_before) {
    // This is a signal that the user was at some point eligible for onboarding.
    // The feature needs to be checked again, irrespective of onboarding status,
    // in order to ensure data completeness.
    ignore_result(base::FeatureList::IsEnabled(
        password_manager::features::kPasswordManagerOnboardingAndroid));
  }

  if (sync_state == NOT_SYNCING) {
    return false;
  }

  int pref_value = prefs->GetInteger(
      password_manager::prefs::kPasswordManagerOnboardingState);
  bool should_show =
      (pref_value == static_cast<int>(OnboardingState::kShouldShow));
  if (!should_show)
    return false;

  // It is very important that the feature is checked only for users who
  // are or were eligible for onboarding, otherwise the data will be diluted.
  // It's also important that the feature is checked in all eligible cases,
  // including past eligibiliy and users having already seen the onboarding
  // prompt, otherwise the data will be incomplete.
  prefs->SetBoolean(password_manager::prefs::kWasOnboardingFeatureCheckedBefore,
                    true);
  return base::FeatureList::IsEnabled(
      password_manager::features::kPasswordManagerOnboardingAndroid);
}

SavingFlowMetricsRecorder::SavingFlowMetricsRecorder() = default;

SavingFlowMetricsRecorder::~SavingFlowMetricsRecorder() {
  metrics_util::LogResultOfSavingFlow(flow_result_);
  if (onboarding_shown_) {
    metrics_util::LogResultOfOnboardingSavingFlow(flow_result_);
  }
}

void SavingFlowMetricsRecorder::SetOnboardingShown() {
  onboarding_shown_ = true;
}

void SavingFlowMetricsRecorder::SetFlowResult(
    metrics_util::UIDismissalReason reason) {
  flow_result_ = FlowResultFromInfobarDismissalReason(reason);
}

void SavingFlowMetricsRecorder::SetFlowResult(
    metrics_util::OnboardingUIDismissalReason reason) {
  flow_result_ = FlowResultFromOnboardingDismissalReason(reason);
}

}  // namespace password_manager
