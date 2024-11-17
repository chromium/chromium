// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/trigger.h"

#include "base/time/default_tick_clock.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/organization/tab_data.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service_factory.h"
#include "chrome/browser/ui/tabs/organization/trigger_policies.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "tab_sensitivity_cache.h"

namespace {

// Just counts the number of tabs in the browser.
float ScoringFunction(float sensitivity_threshold, TabStripModel* const model) {
  // Feature may be disabled in tests, in which case GetForProfile will CHECK.
  const TabOrganizationService* const service =
      base::FeatureList::IsEnabled(features::kTabOrganization)
          ? TabOrganizationServiceFactory::GetForProfile(model->profile())
          : nullptr;

  int num_eligible_tabs = 0;
  for (int i = 0; i < model->count(); i++) {
    const TabData tab = TabData(model->GetTabAtIndex(i));
    if (service) {
      const std::optional<float> score =
          service->tab_sensitivity_cache()->GetScore(tab.original_url());
      if (score && score.value() > sensitivity_threshold) {
        continue;
      }
    }
    if (!tab.IsValidForOrganizing()) {
      continue;
    }
    num_eligible_tabs++;
  }
  return num_eligible_tabs;
}
}  // namespace

TabOrganizationTrigger::TabOrganizationTrigger(
    TriggerScoringFunction scoring_function,
    float score_threshold,
    std::unique_ptr<TriggerPolicy> policy)
    : scoring_function_(scoring_function),
      score_threshold_(score_threshold),
      policy_(std::move(policy)) {}

TabOrganizationTrigger::~TabOrganizationTrigger() = default;

bool TabOrganizationTrigger::ShouldTrigger(
    TabStripModel* const tab_strip_model) const {
  const float score = scoring_function_.Run(tab_strip_model);
  if (score < score_threshold_) {
    return false;
  }

  return policy_->ShouldTrigger(score);
}

TriggerScoringFunction GetTriggerScoringFunction() {
  return base::BindRepeating(&ScoringFunction, GetSensitivityThreshold());
}

float GetTriggerScoreThreshold() {
  return features::kTabOrganizationTriggerThreshold.Get();
}

float GetSensitivityThreshold() {
  return features::kTabOrganizationTriggerSensitivityThreshold.Get();
}

std::unique_ptr<TriggerPolicy> GetTriggerPolicy(
    BackoffLevelProvider* backoff_level_provider,
    Profile* profile) {
  if (features::KTabOrganizationTriggerDemoMode.Get()) {
    return std::make_unique<DemoTriggerPolicy>();
  }

  auto* management_service =
      policy::ManagementServiceFactory::GetForProfile(profile);
  if (!base::FeatureList::IsEnabled(
          features::kTabOrganizationEnableNudgeForEnterprise) &&
      policy::ManagementServiceFactory::GetForPlatform()->IsManaged() &&
      management_service && management_service->IsManaged()) {
    return std::make_unique<NeverTriggerPolicy>();
  }

  return std::make_unique<TargetFrequencyTriggerPolicy>(
      std::make_unique<UsageTickClock>(base::DefaultTickClock::GetInstance()),
      features::kTabOrganizationTriggerPeriod.Get(),
      features::kTabOrganizationTriggerBackoffBase.Get(),
      std::move(backoff_level_provider));
}

std::unique_ptr<TabOrganizationTrigger> MakeTrigger(
    BackoffLevelProvider* backoff_level_provider,
    Profile* profile) {
  return std::make_unique<TabOrganizationTrigger>(
      GetTriggerScoringFunction(), GetTriggerScoreThreshold(),
      GetTriggerPolicy(backoff_level_provider, profile));
}
