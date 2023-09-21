// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/trigger.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

namespace {

// Just counts the number of tabs in the browser.
float MVPScoringFunction(const TabStripModel* const model) {
  int num_tabs_not_in_group = 0;
  for (int i = 0; i < model->count(); i++) {
    if (model->GetTabGroupForTab(i) == absl::nullopt) {
      num_tabs_not_in_group++;
    }
  }
  return num_tabs_not_in_group;
}

constexpr int kMinTabCount = 7;

// Trigger only first time a trigger moment occurs.
class GreedyTriggerPolicy final : public TriggerPolicy {
 public:
  bool ShouldTrigger(float score) override {
    if (has_triggered_) {
      return false;
    }
    has_triggered_ = true;
    return true;
  }

 private:
  bool has_triggered_ = false;
};
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
    const TabStripModel* const tab_strip_model) const {
  const float score = scoring_function_.Run(tab_strip_model);
  if (score < score_threshold_) {
    return false;
  }

  return policy_->ShouldTrigger(score);
}

std::unique_ptr<TabOrganizationTrigger> MakeMVPTrigger() {
  return std::make_unique<TabOrganizationTrigger>(
      base::BindRepeating(&MVPScoringFunction), kMinTabCount,
      std::make_unique<GreedyTriggerPolicy>());
}
