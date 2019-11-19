// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision_metrics_recorder.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision_handler_utils.h"

// static
AddSupervisionMetricsRecorder* AddSupervisionMetricsRecorder::GetInstance() {
  static base::NoDestructor<AddSupervisionMetricsRecorder> instance_;
  return instance_.get();
}

void AddSupervisionMetricsRecorder::RecordAddSupervisionEnrollment(
    EnrollmentState action) {
  base::UmaHistogramEnumeration("AddSupervisionDialog.Enrollment", action);
  switch (action) {
    case EnrollmentState::kInitiated:
      DCHECK(!EnrollmentCompleted())
          << "The user should not be enrolled in supervision at the start of "
             "the Add Supervision process.";
      base::RecordAction(
          base::UserMetricsAction("AddSupervisionDialog_Launched"));
      start_time_ = clock_->NowTicks();
      break;
    case EnrollmentState::kCompleted:
      DCHECK(EnrollmentCompleted())
          << "Add Supervision enrollment should be completed before recording "
             "kCompleted UMA metric.";
      RecordUserTime("AddSupervisionDialog.EnrollmentCompletedUserTime");
      base::RecordAction(
          base::UserMetricsAction("AddSupervisionDialog_EnrollmentCompleted"));
      break;
    case EnrollmentState::kSignedOut:
      DCHECK(EnrollmentCompleted())
          << "There should be no way for the user to attempt sign out from "
             "within the Add Supervision dialog without completing supervision "
             "enrollment.";
      RecordUserTime("AddSupervisionDialog.SignoutCompletedUserTime");
      base::RecordAction(base::UserMetricsAction(
          "AddSupervisionDialog_AttemptedSignoutAfterEnrollment"));
      break;
    case EnrollmentState::kClosed:
      DCHECK(!EnrollmentCompleted())
          << "There should be no way to close the Add Supervision dialog "
             "without signing out after supervision enrollment has completed.";
      RecordUserTime("AddSupervisionDialog.EnrollmentNotCompletedUserTime");
      base::RecordAction(
          base::UserMetricsAction("AddSupervisionDialog_Closed"));
      break;
    case EnrollmentState::kSwitchedAccounts:
      DCHECK(!EnrollmentCompleted()) << "The only way for the user to switch "
                                        "accounts is before enrollment";
      RecordUserTime("AddSupervisionDialog.EnrollmentNotCompletedUserTime");
      base::RecordAction(
          base::UserMetricsAction("AddSupervisionDialog_SwitchedAccounts"));
      break;
  }
}

void AddSupervisionMetricsRecorder::SetClockForTesting(
    const base::TickClock* tick_clock) {
  clock_ = tick_clock;
}

AddSupervisionMetricsRecorder::AddSupervisionMetricsRecorder()
    : clock_(base::DefaultTickClock::GetInstance()) {}

void AddSupervisionMetricsRecorder::RecordUserTime(
    const char* metric_name) const {
  DCHECK(!start_time_.is_null()) << "start_time_ has not been initialized.";
  base::TimeDelta duration = clock_->NowTicks() - start_time_;
  base::UmaHistogramLongTimes(metric_name, duration);
}
