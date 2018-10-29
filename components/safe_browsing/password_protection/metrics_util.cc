// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/password_protection/metrics_util.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "net/http/http_status_code.h"

namespace safe_browsing {

const char kAnyPasswordEntryRequestOutcomeHistogram[] =
    "PasswordProtection.RequestOutcome.AnyPasswordEntry";
const char kAnyPasswordEntryVerdictHistogram[] =
    "PasswordProtection.Verdict.AnyPasswordEntry";
const char kEnterprisePasswordEntryRequestOutcomeHistogram[] =
    "PasswordProtection.RequestOutcome.NonGaiaEnterprisePasswordEntry";
const char kEnterprisePasswordEntryVerdictHistogram[] =
    "PasswordProtection.Verdict.NonGaiaEnterprisePasswordEntry";
const char kEnterprisePasswordPageInfoHistogram[] =
    "PasswordProtection.PageInfoAction.NonGaiaEnterprisePasswordEntry";
const char kEnterprisePasswordInterstitialHistogram[] =
    "PasswordProtection.InterstitialAction.NonGaiaEnterprisePasswordEntry";
const char kEnterprisePasswordWarningDialogHistogram[] =
    "PasswordProtection.ModalWarningDialogAction."
    "NonGaiaEnterprisePasswordEntry";
const char kGSuiteSyncPasswordEntryRequestOutcomeHistogram[] =
    "PasswordProtection.RequestOutcome.GSuiteSyncPasswordEntry";
const char kGSuiteSyncPasswordEntryVerdictHistogram[] =
    "PasswordProtection.Verdict.GSuiteSyncPasswordEntry";
const char kGSuiteSyncPasswordInterstitialHistogram[] =
    "PasswordProtection.InterstitialAction.GSuiteSyncPasswordEntry";
const char kGSuiteSyncPasswordPageInfoHistogram[] =
    "PasswordProtection.PageInfoAction.GSuiteSyncPasswordEntry";
const char kGSuiteSyncPasswordWarningDialogHistogram[] =
    "PasswordProtection.ModalWarningDialogAction.GSuiteSyncPasswordEntry";
const char kInterstitialActionByUserNavigationHistogram[] =
    "PasswordProtection.InterstitialActionByUserNavigation";
const char kPasswordOnFocusRequestOutcomeHistogram[] =
    "PasswordProtection.RequestOutcome.PasswordFieldOnFocus";
const char kPasswordOnFocusVerdictHistogram[] =
    "PasswordProtection.Verdict.PasswordFieldOnFocus";
const char kProtectedPasswordEntryRequestOutcomeHistogram[] =
    "PasswordProtection.RequestOutcome.ProtectedPasswordEntry";
const char kProtectedPasswordEntryVerdictHistogram[] =
    "PasswordProtection.Verdict.ProtectedPasswordEntry";
const char kSyncPasswordEntryRequestOutcomeHistogram[] =
    "PasswordProtection.RequestOutcome.SyncPasswordEntry";
const char kSyncPasswordEntryVerdictHistogram[] =
    "PasswordProtection.Verdict.SyncPasswordEntry";
const char kSyncPasswordChromeSettingsHistogram[] =
    "PasswordProtection.ChromeSettingsAction.SyncPasswordEntry";
const char kSyncPasswordInterstitialHistogram[] =
    "PasswordProtection.InterstitialAction.SyncPasswordEntry";
const char kSyncPasswordPageInfoHistogram[] =
    "PasswordProtection.PageInfoAction.SyncPasswordEntry";
const char kSyncPasswordWarningDialogHistogram[] =
    "PasswordProtection.ModalWarningDialogAction.SyncPasswordEntry";

void LogPasswordEntryRequestOutcome(RequestOutcome outcome,
                                    ReusedPasswordType password_type,
                                    SyncAccountType sync_account_type) {
  UMA_HISTOGRAM_ENUMERATION(kAnyPasswordEntryRequestOutcomeHistogram, outcome);
  if (password_type == PasswordReuseEvent::SIGN_IN_PASSWORD) {
    if (sync_account_type == PasswordReuseEvent::GSUITE) {
      UMA_HISTOGRAM_ENUMERATION(kGSuiteSyncPasswordEntryRequestOutcomeHistogram,
                                outcome);
    }
    UMA_HISTOGRAM_ENUMERATION(kSyncPasswordEntryRequestOutcomeHistogram,
                              outcome);
  } else if (password_type == PasswordReuseEvent::ENTERPRISE_PASSWORD) {
    UMA_HISTOGRAM_ENUMERATION(kEnterprisePasswordEntryRequestOutcomeHistogram,
                              outcome);
  } else {
    UMA_HISTOGRAM_ENUMERATION(kProtectedPasswordEntryRequestOutcomeHistogram,
                              outcome);
  }
}

void LogPasswordOnFocusRequestOutcome(RequestOutcome outcome) {
  UMA_HISTOGRAM_ENUMERATION(kPasswordOnFocusRequestOutcomeHistogram, outcome);
}

void LogPasswordAlertModeOutcome(RequestOutcome outcome,
                                 ReusedPasswordType password_type) {
  DCHECK(password_type == PasswordReuseEvent::SIGN_IN_PASSWORD ||
         password_type == PasswordReuseEvent::ENTERPRISE_PASSWORD);
  if (password_type == PasswordReuseEvent::SIGN_IN_PASSWORD) {
    UMA_HISTOGRAM_ENUMERATION(
        "PasswordProtection.PasswordAlertModeOutcome.GSuiteSyncPasswordEntry",
        outcome);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "PasswordProtection.PasswordAlertModeOutcome."
        "NonGaiaEnterprisePasswordEntry",
        outcome);
  }
}

void LogNoPingingReason(LoginReputationClientRequest::TriggerType trigger_type,
                        RequestOutcome reason,
                        ReusedPasswordType password_type,
                        SyncAccountType sync_account_type) {
  DCHECK(trigger_type == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE ||
         trigger_type == LoginReputationClientRequest::PASSWORD_REUSE_EVENT);

  if (trigger_type == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE) {
    UMA_HISTOGRAM_ENUMERATION(kPasswordOnFocusRequestOutcomeHistogram, reason);
  } else {
    LogPasswordEntryRequestOutcome(reason, password_type, sync_account_type);
  }
}

void LogPasswordProtectionVerdict(
    LoginReputationClientRequest::TriggerType trigger_type,
    ReusedPasswordType password_type,
    SyncAccountType sync_account_type,
    VerdictType verdict_type) {
  switch (trigger_type) {
    case LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE:
      UMA_HISTOGRAM_ENUMERATION(
          kPasswordOnFocusVerdictHistogram, verdict_type,
          LoginReputationClientResponse_VerdictType_VerdictType_MAX + 1);
      break;
    case LoginReputationClientRequest::PASSWORD_REUSE_EVENT:
      UMA_HISTOGRAM_ENUMERATION(
          kAnyPasswordEntryVerdictHistogram, verdict_type,
          LoginReputationClientResponse_VerdictType_VerdictType_MAX + 1);
      if (password_type == PasswordReuseEvent::SIGN_IN_PASSWORD) {
        if (sync_account_type == PasswordReuseEvent::GSUITE) {
          UMA_HISTOGRAM_ENUMERATION(
              kGSuiteSyncPasswordEntryVerdictHistogram, verdict_type,
              LoginReputationClientResponse_VerdictType_VerdictType_MAX + 1);
        }
        UMA_HISTOGRAM_ENUMERATION(
            kSyncPasswordEntryVerdictHistogram, verdict_type,
            LoginReputationClientResponse_VerdictType_VerdictType_MAX + 1);
      } else if (password_type == PasswordReuseEvent::ENTERPRISE_PASSWORD) {
        UMA_HISTOGRAM_ENUMERATION(
            kEnterprisePasswordEntryVerdictHistogram, verdict_type,
            LoginReputationClientResponse_VerdictType_VerdictType_MAX + 1);
      } else {
        UMA_HISTOGRAM_ENUMERATION(
            kProtectedPasswordEntryVerdictHistogram, verdict_type,
            LoginReputationClientResponse_VerdictType_VerdictType_MAX + 1);
      }
      break;
    default:
      NOTREACHED();
  }
}

void LogSyncAccountType(SyncAccountType sync_account_type) {
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordProtection.PasswordReuseSyncAccountType", sync_account_type,
      LoginReputationClientRequest::PasswordReuseEvent::SyncAccountType_MAX +
          1);
}

void LogPasswordProtectionNetworkResponseAndDuration(
    int response_code,
    const base::TimeTicks& request_start_time) {
  base::UmaHistogramSparse(
      "PasswordProtection.PasswordProtectionResponseOrErrorCode",
      response_code);
  if (response_code == net::HTTP_OK) {
    UMA_HISTOGRAM_TIMES("PasswordProtection.RequestNetworkDuration",
                        base::TimeTicks::Now() - request_start_time);
  }
}

void LogWarningAction(WarningUIType ui_type,
                      WarningAction action,
                      ReusedPasswordType password_type,
                      SyncAccountType sync_account_type) {
  // |password_type| can be unknown if user directly navigates to
  // chrome://reset-password page. In this case, do not record user action.
  if (password_type == PasswordReuseEvent::REUSED_PASSWORD_TYPE_UNKNOWN &&
      ui_type == WarningUIType::INTERSTITIAL) {
    UMA_HISTOGRAM_ENUMERATION(
        "PasswordProtection.InterstitialActionByUserNavigation", action);
    return;
  }
  bool is_sign_in_password =
      password_type == PasswordReuseEvent::SIGN_IN_PASSWORD;
  bool is_gsuite_user = sync_account_type == PasswordReuseEvent::GSUITE;
  switch (ui_type) {
    case WarningUIType::PAGE_INFO:
      if (is_sign_in_password) {
        UMA_HISTOGRAM_ENUMERATION(kSyncPasswordPageInfoHistogram, action);
        if (is_gsuite_user) {
          UMA_HISTOGRAM_ENUMERATION(kGSuiteSyncPasswordPageInfoHistogram,
                                    action);
        }
      } else {
        UMA_HISTOGRAM_ENUMERATION(kEnterprisePasswordPageInfoHistogram, action);
      }
      break;
    case WarningUIType::MODAL_DIALOG:
      if (is_sign_in_password) {
        UMA_HISTOGRAM_ENUMERATION(kSyncPasswordWarningDialogHistogram, action);
        if (is_gsuite_user) {
          UMA_HISTOGRAM_ENUMERATION(kGSuiteSyncPasswordWarningDialogHistogram,
                                    action);
        }
      } else {
        UMA_HISTOGRAM_ENUMERATION(kEnterprisePasswordWarningDialogHistogram,
                                  action);
      }
      break;
    case WarningUIType::CHROME_SETTINGS:
      DCHECK(is_sign_in_password);
      UMA_HISTOGRAM_ENUMERATION(kSyncPasswordChromeSettingsHistogram, action);
      break;
    case WarningUIType::INTERSTITIAL:
      if (is_sign_in_password) {
        UMA_HISTOGRAM_ENUMERATION(kSyncPasswordInterstitialHistogram, action);
        if (is_gsuite_user) {
          UMA_HISTOGRAM_ENUMERATION(kGSuiteSyncPasswordInterstitialHistogram,
                                    action);
        }
      } else {
        UMA_HISTOGRAM_ENUMERATION(kEnterprisePasswordInterstitialHistogram,
                                  action);
      }
      break;
    case WarningUIType::NOT_USED:
      NOTREACHED();
      break;
  }
}

void LogNumberOfReuseBeforeSyncPasswordChange(size_t reuse_count) {
  UMA_HISTOGRAM_COUNTS_100(
      "PasswordProtection.GaiaPasswordReusesBeforeGaiaPasswordChanged",
      reuse_count);
}

void LogContentsSize(const gfx::Size& size) {
  if (size.width() <= 0 || size.height() <= 0)
    return;
  UMA_HISTOGRAM_COUNTS_10000("SafeBrowsing.ContentsSize.Width", size.width());
  UMA_HISTOGRAM_COUNTS_10000("SafeBrowsing.ContentsSize.Height", size.height());
}

}  // namespace safe_browsing
