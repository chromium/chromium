// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/first_run_service.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/channel.h"

#if !BUILDFLAG(ENABLE_DICE_SUPPORT)
#error "Unsupported platform"
#endif

namespace {
// The field trial name.
const char kTrialName[] = "ForYouFreStudy";

// Group names for the trial.
const char kEnabledGroup[] = "ClientSideEnabled";
const char kDisabledGroup[] = "ClientSideDisabled";
const char kDefaultGroup[] = "Default";

// Probabilities for all field trial groups add up to kTotalProbability.
const base::FieldTrial::Probability kTotalProbability = 100;

std::string PickTrialGroupWithoutActivation(base::FieldTrial& trial,
                                            version_info::Channel channel) {
  int enabled_percent;
  int disabled_percent;
  int default_percent;
  switch (channel) {
    case version_info::Channel::UNKNOWN:
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
    case version_info::Channel::BETA:
      enabled_percent = 50;
      disabled_percent = 50;
      default_percent = 0;
      break;
    case version_info::Channel::STABLE:
      // Disabled on stable pending approval. http://launch/4200918
      enabled_percent = 0;
      disabled_percent = 0;
      default_percent = 100;
      break;
  }
  DCHECK_EQ(kTotalProbability,
            enabled_percent + disabled_percent + default_percent);

  trial.AppendGroup(kEnabledGroup, enabled_percent);
  trial.AppendGroup(kDisabledGroup, disabled_percent);
  trial.AppendGroup(kDefaultGroup, default_percent);

  return trial.GetGroupNameWithoutActivation();
}

}  // namespace

// static
void FirstRunService::SetUpClientSideFieldTrialIfNeeded(
    const base::FieldTrial::EntropyProvider& entropy_provider,
    base::FeatureList* feature_list) {
  if (!first_run::IsChromeFirstRun()) {
    // We should not check these features outside of the first run.
    DVLOG(1) << "Not setting up client-side trial for the FRE, this is not the "
                "first run.";
    return;
  }

  // Make sure that Finch, fieldtrial_testing_config and command line flags take
  // precedence over features defined here. In particular, not detecting
  // fieldtrial_testing_config triggers a DCHECK.

  if (base::FieldTrialList::Find(kTrialName)) {
    DVLOG(1) << "Not setting up client-side trial for the FRE, trial "
                "already registered";
    return;
  }

  bool is_behaviour_feature_associated =
      feature_list->HasAssociatedFieldTrialByFeatureName(kForYouFre.name);
  bool is_measurement_feature_associated =
      feature_list->HasAssociatedFieldTrialByFeatureName(kForYouFre.name);
  if (is_behaviour_feature_associated || is_measurement_feature_associated) {
    LOG(WARNING) << "Not setting up client-side trial for the FRE, feature(s) "
                    "already overridden:"
                 << (is_behaviour_feature_associated ? " ForYouFre" : "")
                 << (is_measurement_feature_associated
                         ? " ForYouFreSyntheticTrialRegistration"
                         : "");
    return;
  }

  // Proceed with actually setting up the field trial.
  SetUpClientSideFieldTrial(entropy_provider, feature_list,
                            chrome::GetChannel());
}

// static
void FirstRunService::SetUpClientSideFieldTrial(
    const base::FieldTrial::EntropyProvider& entropy_provider,
    base::FeatureList* feature_list,
    version_info::Channel channel) {
  // Set up the trial and determine the group for the current client.
  scoped_refptr<base::FieldTrial> trial =
      base::FieldTrialList::FactoryGetFieldTrial(
          kTrialName, kTotalProbability, kDefaultGroup, entropy_provider);
  std::string group_name = PickTrialGroupWithoutActivation(*trial, channel);

  // Set up the state of the features based on the obtained group.
  base::FeatureList::OverrideState behaviour_feature_state =
      group_name == kEnabledGroup ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_DISABLE_FEATURE;
  base::FeatureList::OverrideState measurement_feature_state =
      group_name != kDefaultGroup ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_DISABLE_FEATURE;

  if (measurement_feature_state == base::FeatureList::OVERRIDE_ENABLE_FEATURE) {
    variations::AssociateVariationParams(
        kTrialName, group_name, {{kForYouFreStudyGroup.name, group_name}});
  }

  feature_list->RegisterFieldTrialOverride(
      kForYouFre.name, behaviour_feature_state, trial.get());
  feature_list->RegisterFieldTrialOverride(
      kForYouFreSyntheticTrialRegistration.name, measurement_feature_state,
      trial.get());

  // Activate only after the overrides are completed.
  trial->Activate();
}

// static
void FirstRunService::EnsureStickToFirstRunCohort() {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state) {
    return;  // Can be null in unit tests;
  }

  if (!local_state->GetBoolean(prefs::kFirstRunFinished)) {
    // This user did not see the FRE. Their first run either happened before the
    // feature was enabled, or it's happening right now. In the former case we
    // don't enroll them, and in the latter, they will be enrolled right before
    // starting the FRE.
    return;
  }

  auto enrolled_study_group =
      local_state->GetString(prefs::kFirstRunStudyGroup);
  if (enrolled_study_group.empty()) {
    // The user was not enrolled or exited the study at some point.
    return;
  }

  RegisterSyntheticFieldTrial(enrolled_study_group);
}

// static
void FirstRunService::JoinFirstRunCohort() {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state) {
    return;  // Can be null in unit tests;
  }

  // The First Run experience depends on experiment groups (see params
  // associated with the `kForYouFre` feature). To measure the long terms impact
  // of this one-shot experience, we save an associated group name to prefs so
  // we can report it as a synthetic trial for understanding the effects for
  // each specific configuration, disambiguating it from other clients who had
  // a different experience (or did not see the FRE for some reason).
  std::string active_study_group = kForYouFreStudyGroup.Get();
  if (active_study_group.empty()) {
    return;  // No active study, no need to sign up.
  }

  local_state->SetString(prefs::kFirstRunStudyGroup, active_study_group);
  RegisterSyntheticFieldTrial(active_study_group);
}

// static
void FirstRunService::RegisterSyntheticFieldTrial(
    const std::string& group_name) {
  DCHECK(!group_name.empty());

  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      "ForYouFreSynthetic", group_name,
      variations::SyntheticTrialAnnotationMode::kCurrentLog);
}
