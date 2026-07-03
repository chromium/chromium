// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/feature_showcase/feature_showcase_eligibility_tracker.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/profiles/feature_showcase/feature_showcase_step_eligibility_checker.h"

namespace {

constexpr size_t kMaxFeatureShowcaseSteps = 3;
constexpr base::TimeDelta kEvaluationTimeout = base::Seconds(2);

}  // namespace

FeatureShowcaseEligibilityTracker::FeatureShowcaseEligibilityTracker(
    std::vector<std::unique_ptr<FeatureShowcaseStepEligibilityChecker>>
        checkers)
    : checkers_(std::move(checkers)) {}

FeatureShowcaseEligibilityTracker::~FeatureShowcaseEligibilityTracker() =
    default;

void FeatureShowcaseEligibilityTracker::EvaluateEligibleSteps(
    Profile& profile,
    base::OnceCallback<void(const std::vector<std::string>&)> callback) {
  if (checkers_.empty()) {
    // Return asynchronously to match the behavior of the async path below.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::vector<std::string>()));
    return;
  }

  // Cancel any pending callbacks.
  weak_ptr_factory_.InvalidateWeakPtrs();

  results_.clear();
  results_.resize(checkers_.size());
  completed_checkers_ = 0;

  if (on_eligibility_evaluated_callback_) {
    // Return an empty result for the previous call to avoid silently dropping
    // the callback.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(on_eligibility_evaluated_callback_),
                                  std::vector<std::string>()));
  }

  on_eligibility_evaluated_callback_ = std::move(callback);

  timeout_timer_.Start(
      FROM_HERE, kEvaluationTimeout,
      base::BindOnce(&FeatureShowcaseEligibilityTracker::FinishEvaluation,
                     weak_ptr_factory_.GetWeakPtr()));

  for (size_t i = 0; i < checkers_.size(); ++i) {
    checkers_[i]->CheckEligibility(
        profile,
        base::BindOnce(
            &FeatureShowcaseEligibilityTracker::OnStepEligibilityDetermined,
            weak_ptr_factory_.GetWeakPtr(), i));
  }
}

void FeatureShowcaseEligibilityTracker::OnStepEligibilityDetermined(
    size_t index,
    bool is_eligible) {
  ++completed_checkers_;
  results_[index] = is_eligible;

  if (completed_checkers_ == checkers_.size()) {
    FinishEvaluation();
  }
}

void FeatureShowcaseEligibilityTracker::FinishEvaluation() {
  if (!on_eligibility_evaluated_callback_) {
    return;
  }

  // Take ownership of the callback to prevent a recursive re-entry.
  auto completion_callback = std::move(on_eligibility_evaluated_callback_);

  timeout_timer_.Stop();

  // Cancel any pending callbacks.
  weak_ptr_factory_.InvalidateWeakPtrs();

  std::vector<std::string> eligible_steps;
  for (size_t i = 0; i < checkers_.size(); ++i) {
    if (!results_[i].has_value()) {
      results_[i] = checkers_[i]->OnTimeout();
    }

    if (results_[i].value()) {
      eligible_steps.push_back(checkers_[i]->GetStepIdentifier());
    }
  }

  if (eligible_steps.size() > kMaxFeatureShowcaseSteps) {
    eligible_steps.resize(kMaxFeatureShowcaseSteps);
  }

  // `PostTask` to prevent a potential use-after-free. If checks complete
  // synchronously, invoking the callback could synchronously destroy this
  // tracker while `EvaluateEligibleSteps` is still iterating over `checkers_`.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(completion_callback),
                                std::move(eligible_steps)));
}
