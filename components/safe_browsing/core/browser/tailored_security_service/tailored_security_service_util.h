// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_TAILORED_SECURITY_SERVICE_TAILORED_SECURITY_SERVICE_UTIL_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_TAILORED_SECURITY_SERVICE_TAILORED_SECURITY_SERVICE_UTIL_H_

#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_outcome.h"

enum class TailoredSecurityNotificationResult;

namespace safe_browsing {

extern const char kTailoredSecurityDisabledDialogShown[];
extern const char kTailoredSecurityDisabledDialogOkButtonClicked[];
extern const char kTailoredSecurityDisabledDialogSettingsButtonClicked[];
extern const char kTailoredSecurityDisabledDialogDismissed[];
extern const char kTailoredSecurityEnabledDialogShown[];
extern const char kTailoredSecurityEnabledDialogOkButtonClicked[];
extern const char kTailoredSecurityEnabledDialogSettingsButtonClicked[];
extern const char kTailoredSecurityEnabledDialogDismissed[];

// Returns a User Action string that corresponds to the provided outcome and
// enable value.
const char* GetUserActionString(TailoredSecurityOutcome outcome, bool enable);

// Records an UMA Histogram value to count the result of trying to notify a sync
// user about enhanced protection for the enable case.
void RecordEnabledNotificationResult(TailoredSecurityNotificationResult result);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_TAILORED_SECURITY_SERVICE_TAILORED_SECURITY_SERVICE_UTIL_H_
