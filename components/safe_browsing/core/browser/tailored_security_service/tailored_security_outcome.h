// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_TAILORED_SECURITY_SERVICE_TAILORED_SECURITY_OUTCOME_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_TAILORED_SECURITY_SERVICE_TAILORED_SECURITY_OUTCOME_H_

// This represents the outcome of displaying a prompt to the user when the
// account tailored security bit changes.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class TailoredSecurityOutcome {
  kAccepted = 0,
  kDismissed = 1,
  kSettings = 2,
  kShown = 3,
  kRejected = 4,
  kClosedByAnotherDialog = 5,
  kMaxValue = kClosedByAnotherDialog,
};

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_TAILORED_SECURITY_SERVICE_TAILORED_SECURITY_OUTCOME_H_
