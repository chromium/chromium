// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/trigger.h"

#include "base/time/default_tick_clock.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/organization/tab_data.h"
#include "chrome/browser/ui/tabs/organization/trigger_policies.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"

namespace {

// Just counts the number of tabs in the browser.
float MVPScoringFunction(TabStripModel* const model) {
  int num_eligible_tabs = 0;
  for (int i = 0; i < model->count(); i++) {
    if (TabData(model, model->GetWebContentsAt(i)).IsValidForOrganizing()) {
      num_eligible_tabs++;
    }
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

TriggerScoringFunction GetDefaultTriggerScoringFunction() {
  return base::BindRepeating(&MVPScoringFunction);
}

float GetDefaultTriggerScoreThreshold() {
  return features::kTabOrganizationTriggerThreshold.Get();
}

std::unique_ptr<TriggerPolicy> GetDefaultTriggerPolicy(
    std::unique_ptr<BackoffLevelProvider> backoff_level_provider) {
  return std::make_unique<TargetFrequencyTriggerPolicy>(
      std::make_unique<UsageTickClock>(base::DefaultTickClock::GetInstance()),
      features::kTabOrganizationTriggerPeriod.Get(),
      features::kTabOrganizationTriggerBackoffBase.Get(),
      std::move(backoff_level_provider));
}

std::unique_ptr<TabOrganizationTrigger> MakeMVPTrigger(
    std::unique_ptr<BackoffLevelProvider> backoff_level_provider) {
  return std::make_unique<TabOrganizationTrigger>(
      GetDefaultTriggerScoringFunction(), GetDefaultTriggerScoreThreshold(),
      GetDefaultTriggerPolicy(std::move(backoff_level_provider)));
}
