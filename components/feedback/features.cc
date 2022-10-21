// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/features.h"

namespace feedback::features {

// Alphabetical:

// Enables feedback tool to save feedback report to local disk.
// This flag is only for e2e tast test purpose.
BASE_FEATURE(kOsFeedbackSaveReportToLocalForE2ETesting,
             "OsFeedbackSaveReportToLocalForE2ETesting",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsOsFeedbackSaveReportToLocalForE2ETestingEnabled() {
  return base::FeatureList::IsEnabled(
      kOsFeedbackSaveReportToLocalForE2ETesting);
}

// Disables dispatching reports in Tast tests.
BASE_FEATURE(kSkipSendingFeedbackReportInTastTests,
             "SkipSendingFeedbackReportInTastTests",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsSkipSendingFeedbackReportInTastTestsEnabled() {
  return base::FeatureList::IsEnabled(kSkipSendingFeedbackReportInTastTests);
}

}  // namespace feedback::features
