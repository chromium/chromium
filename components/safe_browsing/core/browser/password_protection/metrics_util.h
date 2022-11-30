// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_PASSWORD_PROTECTION_METRICS_UTIL_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_PASSWORD_PROTECTION_METRICS_UTIL_H_

#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace base {
class TimeTicks;
}

namespace safe_browsing {

// UMA metrics
extern const char kAnyPasswordEntryRequestOutcomeHistogram[];
extern const char kAnyPasswordEntryVerdictHistogram[];
extern const char kEnterprisePasswordEntryRequestOutcomeHistogram[];
extern const char kEnterprisePasswordEntryVerdictHistogram[];
extern const char kEnterprisePasswordInterstitialHistogram[];
extern const char kEnterprisePasswordPageInfoHistogram[];
extern const char kEnterprisePasswordWarningDialogHistogram[];
extern const char kSavedPasswordPageInfoHistogram[];
extern const char kGmailNonSyncPasswordInterstitialHistogram[];
extern const char kGmailSyncPasswordPageInfoHistogram[];
extern const char kGmailNonSyncPasswordPageInfoHistogram[];
extern const char kGmailSyncPasswordWarningDialogHistogram[];
extern const char kGmailNonSyncPasswordWarningDialogHistogram[];
extern const char kNonSyncPasswordInterstitialHistogram[];
extern const char kNonSyncPasswordPageInfoHistogram[];
extern const char kGmailSyncPasswordEntryRequestOutcomeHistogram[];
extern const char kGmailNonSyncPasswordEntryRequestOutcomeHistogram[];
extern const char kGSuiteNonSyncPasswordEntryRequestOutcomeHistogram[];
extern const char kGSuiteSyncPasswordEntryVerdictHistogram[];

extern const char kGSuiteSyncPasswordEntryRequestOutcomeHistogram[];
extern const char kGSuiteNonSyncPasswordEntryVerdictHistogram[];
extern const char kGmailSyncPasswordEntryVerdictHistogram[];
extern const char kGmailNonSyncPasswordEntryVerdictHistogram[];
extern const char kGSuiteSyncPasswordInterstitialHistogram[];
extern const char kGSuiteNonSyncPasswordInterstitialHistogram[];
extern const char kGSuiteSyncPasswordPageInfoHistogram[];
extern const char kGSuiteNonSyncPasswordPageInfoHistogram[];
extern const char kGSuiteSyncPasswordWarningDialogHistogram[];
extern const char kGSuiteNonSyncPasswordWarningDialogHistogram[];
extern const char kPasswordOnFocusRequestOutcomeHistogram[];
extern const char kPasswordOnFocusVerdictHistogram[];
extern const char kNonSyncPasswordEntryRequestOutcomeHistogram[];
extern const char kNonSyncPasswordEntryVerdictHistogram[];
extern const char kSyncPasswordChromeSettingsHistogram[];
extern const char kSyncPasswordEntryRequestOutcomeHistogram[];
extern const char kSyncPasswordEntryVerdictHistogram[];
extern const char kSyncPasswordInterstitialHistogram[];
extern const char kSyncPasswordPageInfoHistogram[];
extern const char kSyncPasswordWarningDialogHistogram[];
extern const char kEnterprisePasswordAlertHistogram[];
extern const char kGsuiteSyncPasswordAlertHistogram[];
extern const char kGsuiteNonSyncPasswordAlertHistogram[];

extern const char kPasswordOnFocusRequestWithTokenHistogram[];
extern const char kAnyPasswordEntryRequestWithTokenHistogram[];

using ReusedPasswordAccountType =
    LoginReputationClientRequest::PasswordReuseEvent::ReusedPasswordAccountType;
using SyncAccountType =
    LoginReputationClientRequest::PasswordReuseEvent::SyncAccountType;
using VerdictType = LoginReputationClientResponse::VerdictType;

// The outcome of the request. These values are used for UMA.
// DO NOT CHANGE THE ORDERING OF THESE VALUES.
enum class RequestOutcome {
  // Request outcome unknown.
  UNKNOWN = 0,
  // Request successfully sent.
  SUCCEEDED = 1,
  // Request canceled.
  CANCELED = 2,
  // Request timeout.
  TIMEDOUT = 3,
  // No request sent because URL matches allowlist.
  MATCHED_ALLOWLIST = 4,
  // No request sent because response already cached.
  RESPONSE_ALREADY_CACHED = 5,
  DEPRECATED_NO_EXTENDED_REPORTING = 6,
  // No request sent because user is in incognito mode.
  DISABLED_DUE_TO_INCOGNITO = 7,
  // No request sent because request is malformed.
  REQUEST_MALFORMED = 8,
  // Net error.
  FETCH_FAILED = 9,
  // Response received but malformed.
  RESPONSE_MALFORMED = 10,
  // No request sent since password protection service is no longer available.
  SERVICE_DESTROYED = 11,
  // No request sent because pinging feature is disable.
  DISABLED_DUE_TO_FEATURE_DISABLED = 12,
  // No request sent because the user is not extended reporting or enhanced
  // protection user.
  DISABLED_DUE_TO_USER_POPULATION = 13,
  // No request sent because the reputation of the URL is not computable.
  URL_NOT_VALID_FOR_REPUTATION_COMPUTING = 14,
  // No request sent because URL matches enterprise allowlist.
  MATCHED_ENTERPRISE_ALLOWLIST = 15,
  // No request sent because URL matches enterprise change password URL.
  MATCHED_ENTERPRISE_CHANGE_PASSWORD_URL = 16,
  // No request sent because URL matches enterprise login URL.
  MATCHED_ENTERPRISE_LOGIN_URL = 17,
  // No request sent if the admin configures password protection to
  // warn on ALL password reuses (rather than just phishing sites).
  PASSWORD_ALERT_MODE = 18,
  // No request sent if the admin turns off password protection.
  TURNED_OFF_BY_ADMIN = 19,
  // No request sent because Safe Browsing is disabled.
  SAFE_BROWSING_DISABLED = 20,
  // No request sent because user is not signed-in.
  USER_NOT_SIGNED_IN = 21,
  // The country is excluded from sending pings.
  EXCLUDED_COUNTRY = 22,
  kMaxValue = EXCLUDED_COUNTRY,
};

// Enum values indicates if a password protection warning is shown or
// represents user's action on warnings. These values are used for UMA.
// DO NOT CHANGE THE ORDERING OF THESE VALUES.
enum class WarningAction {
  // Warning shows up.
  SHOWN = 0,

  // User clicks on "Change Password" button.
  CHANGE_PASSWORD = 1,

  // User clicks on "Ignore" button.
  IGNORE_WARNING = 2,

  // Dialog closed in reaction to change of user state.
  CLOSE = 3,

  // User explicitly mark the site as legitimate.
  MARK_AS_LEGITIMATE = 4,

  kMaxValue = MARK_AS_LEGITIMATE,
};

// Type of password protection warning UI.
enum class WarningUIType {
  NOT_USED = 0,
  // Page in bubble.
  PAGE_INFO = 1,
  // Modal warning dialog.
  MODAL_DIALOG = 2,
  // chrome://reset-password interstitial.
  INTERSTITIAL = 3,
};

// Logs whether an access_token was sent or not, for the appropriate
// |trigger_type| metric.
void LogPasswordProtectionRequestTokenHistogram(
    LoginReputationClientRequest::TriggerType trigger_type,
    bool has_access_token);

// Logs the |outcome| to several UMA metrics, depending on the value
// of |password_type| and |sync_account_type|.
void LogPasswordEntryRequestOutcome(
    RequestOutcome outcome,
    ReusedPasswordAccountType password_account_type);

// Logs the |outcome| to several UMA metrics for password on focus pings.
void LogPasswordOnFocusRequestOutcome(RequestOutcome outcome);

// Logs the |outcome| to several UMA metrics for password alert mode.
void LogPasswordAlertModeOutcome(
    RequestOutcome outcome,
    ReusedPasswordAccountType password_account_type);

// Logs password protection verdict based on |trigger_type|, |password_type|,
// and |sync_account_type|.
void LogPasswordProtectionVerdict(
    LoginReputationClientRequest::TriggerType trigger_type,
    ReusedPasswordAccountType password_account_type,
    VerdictType verdict_type);

// Logs |reason| for why there's no ping sent out.
void LogNoPingingReason(LoginReputationClientRequest::TriggerType trigger_type,
                        RequestOutcome reason,
                        ReusedPasswordAccountType password_account_type);

// Logs the type of sync account.
void LogSyncAccountType(SyncAccountType sync_account_type);

// Logs the network response and duration of a password protection ping.
void LogPasswordProtectionNetworkResponseAndDuration(
    int response_code,
    int net_error,
    const base::TimeTicks& request_start_time);

// Logs when a sample ping of allowlist URLs is sent to Safe Browsing.
void LogPasswordProtectionSampleReportSent();

// Records user action on warnings to corresponding UMA histograms.
void LogWarningAction(WarningUIType ui_type,
                      WarningAction action,
                      ReusedPasswordAccountType password_account_type);

// Logs the number of verdict migrated to the new caching structure.
void LogNumberOfVerdictMigrated(size_t verdicts_migrated);

// Logs the size of referrer chain by |verdict_type|.
void LogReferrerChainSize(
    LoginReputationClientResponse::VerdictType verdict_type,
    int referrer_chain_size);

// Logs the interval between when the modal warning is constructed and when it
// is destructed.
void LogModalWarningDialogLifetime(
    base::TimeTicks modal_construction_start_time);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_PASSWORD_PROTECTION_METRICS_UTIL_H_
