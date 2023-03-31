// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service_util.h"

#include "base/metrics/histogram_functions.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_notification_result.h"

namespace safe_browsing {

const char kTailoredSecurityDisabledDialogShown[] =
    "SafeBrowsing.AccountIntegration.DisabledDialog.Shown";
const char kTailoredSecurityDisabledDialogOkButtonClicked[] =
    "SafeBrowsing.AccountIntegration.DisabledDialog.OkButtonClicked";
const char kTailoredSecurityDisabledDialogSettingsButtonClicked[] =
    "SafeBrowsing.AccountIntegration.DisabledDialog.SettingsButtonClicked";
const char kTailoredSecurityDisabledDialogDismissed[] =
    "SafeBrowsing.AccountIntegration.DisabledDialog.Dismissed";
const char kTailoredSecurityEnabledDialogShown[] =
    "SafeBrowsing.AccountIntegration.EnabledDialog.Shown";
const char kTailoredSecurityEnabledDialogOkButtonClicked[] =
    "SafeBrowsing.AccountIntegration.EnabledDialog.OkButtonClicked";
const char kTailoredSecurityEnabledDialogSettingsButtonClicked[] =
    "SafeBrowsing.AccountIntegration.EnabledDialog.SettingsButtonClicked";
const char kTailoredSecurityEnabledDialogDismissed[] =
    "SafeBrowsing.AccountIntegration.EnabledDialog.Dismissed";

const char* GetUserActionString(TailoredSecurityOutcome outcome, bool enable) {
  switch (outcome) {
    case TailoredSecurityOutcome::kShown:
      return enable ? kTailoredSecurityEnabledDialogShown
                    : kTailoredSecurityDisabledDialogShown;
    case TailoredSecurityOutcome::kAccepted:
      return enable ? kTailoredSecurityEnabledDialogOkButtonClicked
                    : kTailoredSecurityDisabledDialogOkButtonClicked;

    case TailoredSecurityOutcome::kDismissed:
      return enable ? kTailoredSecurityEnabledDialogDismissed
                    : kTailoredSecurityDisabledDialogDismissed;

    case TailoredSecurityOutcome::kSettings:
      return enable ? kTailoredSecurityEnabledDialogSettingsButtonClicked
                    : kTailoredSecurityDisabledDialogSettingsButtonClicked;
    default:
      CHECK(false) << "Attempted to get a user action string for an "
                      "unsupported outcome: "
                   << static_cast<int>(outcome);
      return "";
  }
}

void RecordEnabledNotificationResult(
    TailoredSecurityNotificationResult result) {
  base::UmaHistogramEnumeration(
      "SafeBrowsing.TailoredSecurity.SyncPromptEnabledNotificationResult2",
      result);
}

}  // namespace safe_browsing
