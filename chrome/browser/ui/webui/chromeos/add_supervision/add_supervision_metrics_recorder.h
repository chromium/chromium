// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_ADD_SUPERVISION_ADD_SUPERVISION_METRICS_RECORDER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_ADD_SUPERVISION_ADD_SUPERVISION_METRICS_RECORDER_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/time/time.h"

namespace base {
class TickClock;
}

// Records UMA metrics for users going through the Add Supervision process.
class AddSupervisionMetricsRecorder {
 public:
  // These enum values represent the state that the user has attained while
  // going through the Add Supervision dialog.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class EnrollmentState {
    // Recorded when user opens Add Supervision dialog.
    kInitiated = 0,
    // Recorded when user successfully enrolls in supervision.
    kCompleted = 1,
    // Recorded when user clicks "Sign out" after enrollment in the dialog.
    kSignedOut = 2,
    // Recorded when user closes the dialog without enrollment, excluding sign
    // out.
    kClosed = 3,
    // Recorded when user signs out to switch accounts.
    kSwitchedAccounts = 4,
    // Add future entries above this comment, in sync with enums.xml.
    // Update kMaxValue to the last value.
    kMaxValue = kSwitchedAccounts
  };

  static AddSupervisionMetricsRecorder* GetInstance();

  // Records UMA metrics for users going through the Add Supervision process.
  void RecordAddSupervisionEnrollment(EnrollmentState action);

  // Method intended for testing purposes only.
  // Set clock used for timing to enable manipulation during tests.
  void SetClockForTesting(const base::TickClock* tick_clock);

 private:
  friend class base::NoDestructor<AddSupervisionMetricsRecorder>;

  AddSupervisionMetricsRecorder();

  // Records UMA metric of how long the user spends in the Add Supervision
  // process in milliseconds.
  void RecordUserTime(const char* metric_name) const;

  // Points to the base::DefaultTickClock by default.
  const base::TickClock* clock_;

  // Records when the user initiates the Add Supervision process.
  base::TimeTicks start_time_;

  DISALLOW_COPY_AND_ASSIGN(AddSupervisionMetricsRecorder);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_ADD_SUPERVISION_ADD_SUPERVISION_METRICS_RECORDER_H_
