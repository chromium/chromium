// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ORGANIZATION_TRIGGER_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZATION_TRIGGER_H_

#include <memory>

#include "base/functional/callback.h"

class BackoffLevelProvider;
class TabStripModel;
class Profile;

// Decides whether to trigger the nudge UI. Can incorporate considerations such
// as rate limiting / backoff, historical score distribution, historical CTR for
// this user, etc.
class TriggerPolicy {
 public:
  virtual ~TriggerPolicy() = default;
  virtual bool ShouldTrigger(float score) = 0;
};

using TriggerScoringFunction = base::RepeatingCallback<float(TabStripModel*)>;

// Decides when to trigger the tab organization proactive nudge UI by scoring
// potential trigger moments and picking the best one based on a score threshold
// and trigger policy.
class TabOrganizationTrigger {
 public:
  // `scoring_function` calculates how good of a trigger moment this is.
  // `score_threshold` determines how good of a trigger moment is good enough.
  // If this trigger moment is good enough, `policy` decides whether we trigger
  // now or bide our time.
  TabOrganizationTrigger(TriggerScoringFunction scoring_function,
                         float score_threshold,
                         std::unique_ptr<TriggerPolicy> policy);
  ~TabOrganizationTrigger();

  bool ShouldTrigger(TabStripModel* inputs) const;

 private:
  TriggerScoringFunction scoring_function_;
  float score_threshold_;
  std::unique_ptr<TriggerPolicy> policy_;
};

TriggerScoringFunction GetTriggerScoringFunction();
float GetTriggerScoreThreshold();
float GetSensitivityThreshold();
std::unique_ptr<TriggerPolicy> GetTriggerPolicy(
    BackoffLevelProvider* backoff_level_provider,
    Profile* profile);

std::unique_ptr<TabOrganizationTrigger> MakeTrigger(
    BackoffLevelProvider* backoff_level_provider,
    Profile* profile);

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZATION_TRIGGER_H_
