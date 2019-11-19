// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_ONBOARDING_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_ONBOARDING_H_

#include "base/util/type_safety/strong_alias.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"

class PrefService;

namespace password_manager {

using PasswordUpdateBool = util::StrongAlias<class PasswordUpdateBoolTag, bool>;
using BlacklistedBool = util::StrongAlias<class BlacklistedBoolTag, bool>;

// The onboarding won't be shown if there are this many
// saved credentials or more.
constexpr int kOnboardingCredentialsThreshold = 3;

// This utility class is responsible for updating the
// |kPasswordManagerOnboardingState| pref, for later use in the triggering logic
// for the onboarding.
// Important note: The object will delete itself once it
// receives the results from the password store, thus it should not be allocated
// on the stack. Having a non-public destructor enforces this.
class OnboardingStateUpdate : public password_manager::PasswordStoreConsumer {
 public:
  OnboardingStateUpdate(scoped_refptr<password_manager::PasswordStore> store,
                        PrefService* prefs);

  // Requests all autofillable credentials from PasswordStore.
  void Start();

 private:
  ~OnboardingStateUpdate() override;

  // Update the |kPasswordManagerOnboardingState| pref to represent the right
  // state.
  //     - |kDoNotShow|  -> |kShouldShow| (if credentials count < threshold)
  //     - |kShouldShow| -> |kDoNotShow|  (if credentials count >= threshold)
  void UpdateState(
      std::vector<std::unique_ptr<autofill::PasswordForm>> credentials);

  // PasswordStoreConsumer:
  // When the results are obtained updates the pref and deletes
  // itself.
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override;

  scoped_refptr<password_manager::PasswordStore> store_;

  // |prefs_| is not an owning pointer.
  // It is used to update the |kPasswordManagerOnboardingState| pref.
  PrefService* prefs_;

  DISALLOW_COPY_AND_ASSIGN(OnboardingStateUpdate);
};

// This function updates the |kPasswordManagerOnboardingState| pref on
// a separate thread after a given time delay.
// Runs if the state is not |kShown|.
void UpdateOnboardingState(scoped_refptr<password_manager::PasswordStore> store,
                           PrefService* prefs,
                           base::TimeDelta delay);

// Return true if the password manager onboarding experience should be shown to
// the user. Conditions (all must apply):
//      1. The user is syncing passwords. This is a temporary cutoff to allow
//         us to experiment with different strings talking about features
//         available when syncing.
//      2. The set of credentials is not blacklisted.
//      3. We are dealing with a new set of credentials.
//      4. |kPasswordManagerOnboardingState| is |kShouldShow|.
//      5. The PasswordManagerOnboardingAndroid feature is enabled.
bool ShouldShowOnboarding(PrefService* prefs,
                          PasswordUpdateBool is_password_update,
                          BlacklistedBool is_blacklisted,
                          SyncState sync_state);

// This utility class is responsible for recording the result of the
// saving flow into the following metrics:
//     - "PasswordManager.Onboarding.ResultOfSavingFlow"
//     - "PasswordManager.Onboarding.ResultOfSavingFlowAfterOnboarding"
// They are recorded upon deletion of this class.
// The second metric is only recorded if the onboarding was shown
// during the flow, which is indicated by |onboarding_shown_|.
class SavingFlowMetricsRecorder {
 public:
  SavingFlowMetricsRecorder();

  // Records the metrics before destruction.
  ~SavingFlowMetricsRecorder();

  // This is called to indicate that the onboarding was shown.
  void SetOnboardingShown();

  // Functions called to store the outcome of the saving flow.
  // Accepts the following reasons for dismissing the infobar:
  // NO_DIRECT_INTERACTION, CLICKED_SAVE, CLICKED_CANCEL and CLICKED_NEVER.
  void SetFlowResult(metrics_util::UIDismissalReason reason);
  // Accepts the following reasons for dismissing the onboarding dialog:
  // kRejected and kDismissed.
  void SetFlowResult(metrics_util::OnboardingUIDismissalReason reason);

 private:
  // Boolean indicating if the onboarding was shown in the flow.
  bool onboarding_shown_ = false;
  // Enum indicating the outcome of the password saving flow.
  metrics_util::OnboardingResultOfSavingFlow flow_result_ =
      metrics_util::OnboardingResultOfSavingFlow::kInfobarNoDirectInteraction;

  DISALLOW_COPY_AND_ASSIGN(SavingFlowMetricsRecorder);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_ONBOARDING_H_
