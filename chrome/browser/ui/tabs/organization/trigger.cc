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
#include "tab_sensitivity_cache.h"

namespace {

constexpr float kTriggerThreshold = 7.0f;
constexpr float kSensitivityThreshold = 0.5f;
constexpr base::TimeDelta kTriggerPeriod = base::Hours(6);
constexpr double kTriggerBackoffBase = 2.0;

// Just counts the number of tabs in the browser.
float ScoringFunction(float sensitivity_threshold, TabStripModel* const model) {
  // Feature may be disabled in tests, in which case GetForProfile will CHECK.
  const TabOrganizationService* const service =
      TabOrganizationServiceFactory::GetForProfile(model->profile());

  int num_eligible_tabs = 0;
  for (tabs::TabInterface* tab_model : *model) {
    const TabData tab = TabData(tab_model);
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
  return kTriggerThreshold;
}

float GetSensitivityThreshold() {
  return kSensitivityThreshold;
}

std::unique_ptr<TriggerPolicy> GetTriggerPolicy(
    BackoffLevelProvider* backoff_level_provider,
    Profile* profile) {
  auto* management_service =
      policy::ManagementServiceFactory::GetForProfile(profile);
  if (policy::ManagementServiceFactory::GetForPlatform()->IsManaged() &&
      management_service && management_service->IsManaged()) {
    return std::make_unique<NeverTriggerPolicy>();
  }

  return std::make_unique<TargetFrequencyTriggerPolicy>(
      std::make_unique<UsageTickClock>(base::DefaultTickClock::GetInstance()),
      kTriggerPeriod, kTriggerBackoffBase, std::move(backoff_level_provider));
}

std::unique_ptr<TabOrganizationTrigger> MakeTrigger(
    BackoffLevelProvider* backoff_level_provider,
    Profile* profile) {
  return std::make_unique<TabOrganizationTrigger>(
      GetTriggerScoringFunction(), GetTriggerScoreThreshold(),
      GetTriggerPolicy(backoff_level_provider, profile));
}
