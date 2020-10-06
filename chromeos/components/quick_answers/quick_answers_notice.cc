// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/quick_answers_notice.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "chromeos/components/quick_answers/utils/quick_answers_metrics.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace quick_answers {

QuickAnswersNotice::QuickAnswersNotice(PrefService* prefs) : prefs_(prefs) {}

QuickAnswersNotice::~QuickAnswersNotice() = default;

void QuickAnswersNotice::StartNotice() {
  // Increments impression count.
  IncrementPrefCounter(prefs::kQuickAnswersNoticeImpressionCount, 1);

  // Logs how many times the user has seen the notice.
  RecordNoticeImpression(GetImpressionCount());

  start_time_ = base::TimeTicks::Now();
}

void QuickAnswersNotice::DismissNotice() {
  RecordImpressionDuration();
  // Logs notice dismissed with impression count and impression duration.
  RecordNoticeInteraction(NoticeInteractionType::kDismiss, GetImpressionCount(),
                          GetImpressionDuration());
}

void QuickAnswersNotice::AcceptNotice(NoticeInteractionType interaction) {
  RecordImpressionDuration();
  // Logs notice accepted with impression count and impression duration.
  RecordNoticeInteraction(interaction, GetImpressionCount(),
                          GetImpressionDuration());

  // Marks the notice as accepted.
  prefs_->SetBoolean(prefs::kQuickAnswersConsented, true);
}

bool QuickAnswersNotice::ShouldShowNotice() const {
  return !IsAccepted() && !HasReachedImpressionCap() &&
         !HasReachedDurationCap();
}

bool QuickAnswersNotice::IsAccepted() const {
  return prefs_->GetBoolean(prefs::kQuickAnswersConsented);
}

bool QuickAnswersNotice::HasReachedImpressionCap() const {
  return GetImpressionCount() + 1 > kNoticeImpressionCap;
}

bool QuickAnswersNotice::HasReachedDurationCap() const {
  int duration_secs =
      prefs_->GetInteger(prefs::kQuickAnswersNoticeImpressionDuration);
  return duration_secs >= kNoticeDurationCap;
}

void QuickAnswersNotice::IncrementPrefCounter(const std::string& path,
                                              int count) {
  prefs_->SetInteger(path, prefs_->GetInteger(path) + count);
}

void QuickAnswersNotice::RecordImpressionDuration() {
  // Records duration in pref.
  IncrementPrefCounter(prefs::kQuickAnswersNoticeImpressionDuration,
                       GetImpressionDuration().InSeconds());
}

int QuickAnswersNotice::GetImpressionCount() const {
  return prefs_->GetInteger(prefs::kQuickAnswersNoticeImpressionCount);
}

base::TimeDelta QuickAnswersNotice::GetImpressionDuration() const {
  DCHECK(!start_time_.is_null());
  return base::TimeTicks::Now() - start_time_;
}

}  // namespace quick_answers
}  // namespace chromeos
