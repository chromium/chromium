// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_FEATURES_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_FEATURES_H_

#import "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace password_manager::features {

// Features that are exclusive to ios and doesn't touch any cross-platform
// module go here.

// Enables automatic submission of password forms when the user selects a
// suggestion that fills all the required fields.
BASE_DECLARE_FEATURE(kIOSPasswordAutoSubmission);

// Enum to control the auto-submission flow variant.
enum class AutoSubmissionType {
  // Dismisses the bottom sheet first, then injects a native `\n` keystroke
  // to trigger submission.
  kDismissThenSubmit = 0,

  // Injects a native `\n` keystroke to trigger submission first, then
  // dismisses the bottom sheet.
  kSubmitThenDismiss = 1,

  // Dismisses the bottom sheet, raises a transparent shield to block user
  // interaction, and then injects a native `\n` keystroke to trigger
  // submission.
  kDismissThenBlockThenSubmit = 2,

  // Relies on a JavaScript `form.submit()` call executed in the renderer
  // instead of injecting a native keystroke.
  kScriptSubmit = 3,
};

// Feature param to control the auto-submission flow variant.
extern const base::FeatureParam<AutoSubmissionType> kAutoSubmissionTypeParam;
extern const base::FeatureParam<AutoSubmissionType>::Option
    kAutoSubmissionTypeOptions[];

// Feature param to control whether to wait some period of time between the
// moment the field is focused and the moment the submission is triggered.
// A value of 0 means no wait period.
extern const base::FeatureParam<int> kAutoSubmissionWaitPeriodMsParam;

}  // namespace password_manager::features

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_FEATURES_H_
