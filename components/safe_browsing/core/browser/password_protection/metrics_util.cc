// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/password_protection/metrics_util.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "components/safe_browsing/core/common/utils.h"
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
const char kGmailSyncPasswordEntryRequestOutcomeHistogram[] =
    "PasswordProtection.RequestOutcome.GmailSyncPasswordEntry";
const char kGmailNonSyncPasswordEntryRequestOutcomeHistogram[] =
    "PasswordProtection.RequestOutcome.GmailNonSyncPasswordEntry";
const char kGSuiteSyncPasswordEntryRequestOutcomeHistogram[] =
    "PasswordProtection.RequestOutcome.GSuiteSyncPasswordEntry";
const char kGSuiteNonSyncPasswordEntryRequestOutcomeHistogram[] =
    "PasswordProtection.RequestOutcome.GSuiteNonSyncPasswordEntry";
const char kSavedPasswordEntryRequestOutcomeHistogram[] =
    "PasswordProtection.RequestOutcome.SavedPasswordEntry";
const char kUnknownPrimaryAccountPasswordEntryVerdictHistogram[] =
    "PasswordProtection.Verdict.UnknownPrimaryPasswordEntry";
const char kUnknownNonPrimaryAccountPasswordEntryVerdictHistogram[] =
    "PasswordProtection.Verdict.UnknownNonPrimaryPasswordEntry";
const char kGSuiteSyncPasswordEntryVerdictHistogram[] =
    "PasswordProtection.Verdict.GSuiteSyncPasswordEntry";
const char kGSuiteNonSyncPasswordEntryVerdictHistogram[] =
    "PasswordProtection.Verdict.GSuiteNonSyncPasswordEntry";
const char kGmailSyncPasswordEntryVerdictHistogram[] =
    "PasswordProtection.Verdict.GmailSyncPasswordEntry";
const char kGmailNonSyncPasswordEntryVerdictHistogram[] =
    "PasswordProtection.Verdict.GmailNonSyncPasswordEntry";
const char kSavedPasswordEntryVerdictHistogram[] =
    "PasswordProtection.Verdict.SavedPasswordEntry";
const char kGmailNonSyncPasswordInterstitialHistogram[] =
    "PasswordProtection.InterstitialAction.GmailNonSyncPasswordEntry";
const char kGmailSyncPasswordPageInfoHistogram[] =
    "PasswordProtection.PageInfoAction.GmailSyncPasswordEntry";
const char kGmailNonSyncPasswordPageInfoHistogram[] =
    "PasswordProtection.PageInfoAction.GmailNonSyncPasswordEntry";
const char kSavedPasswordPageInfoHistogram[] =
    "PasswordProtection.PageInfoAction.SavedPasswordEntry";
const char kGmailSyncPasswordWarningDialogHistogram[] =
    "PasswordProtection.ModalWarningDialogAction.GmailSyncPasswordEntry";
const char kGmailNonSyncPasswordWarningDialogHistogram[] =
    "PasswordProtection.ModalWarningDialogAction.GmailNonSyncPasswordEntry";
const char kNonSyncPasswordInterstitialHistogram[] =
    "PasswordProtection.InterstitialAction.NonSyncPasswordEntry";
const char kNonSyncPasswordPageInfoHistogram[] =
    "PasswordProtection.PageInfoAction.NonSyncPasswordEntry";
const char kGSuiteSyncPasswordInterstitialHistogram[] =
    "PasswordProtection.InterstitialAction.GSuiteSyncPasswordEntry";
const char kGSuiteNonSyncPasswordInterstitialHistogram[] =
    "PasswordProtection.InterstitialAction.GSuiteNonSyncPasswordEntry";
const char kGSuiteSyncPasswordPageInfoHistogram[] =
    "PasswordProtection.PageInfoAction.GSuiteSyncPasswordEntry";
const char kGSuiteNonSyncPasswordPageInfoHistogram[] =
    "PasswordProtection.PageInfoAction.GSuiteNonSyncPasswordEntry";
const char kGSuiteSyncPasswordWarningDialogHistogram[] =
    "PasswordProtection.ModalWarningDialogAction.GSuiteSyncPasswordEntry";
const char kGSuiteNonSyncPasswordWarningDialogHistogram[] =
    "PasswordProtection.ModalWarningDialogAction.GSuiteNonSyncPasswordEntry";
const char kSavedPasswordWarningDialogHistogram[] =
    "PasswordProtection.ModalWarningDialogAction.SavedPasswordEntry";
const char kNonSyncPasswordWarningDialogHistogram[] =
    "PasswordProtection.ModalWarningDialogAction.NonSyncPasswordEntry";
const char kPasswordOnFocusRequestOutcomeHistogram[] =
    "PasswordProtection.RequestOutcome.PasswordFieldOnFocus";
const char kPasswordOnFocusVerdictHistogram[] =
    "PasswordProtection.Verdict.PasswordFieldOnFocus";
const char kNonSyncPasswordEntryRequestOutcomeHistogram[] =
    "PasswordProtection.RequestOutcome.NonSyncPasswordEntry";
const char kSyncPasswordEntryRequestOutcomeHistogram[] =
    "PasswordProtection.RequestOutcome.SyncPasswordEntry";
const char kSyncPasswordEntryVerdictHistogram[] =
    "PasswordProtection.Verdict.SyncPasswordEntry";
const char kNonSyncPasswordEntryVerdictHistogram[] =
    "PasswordProtection.Verdict.NonSyncPasswordEntry";
const char kSyncPasswordInterstitialHistogram[] =
    "PasswordProtection.InterstitialAction.SyncPasswordEntry";
const char kSyncPasswordPageInfoHistogram[] =
    "PasswordProtection.PageInfoAction.SyncPasswordEntry";
const char kSyncPasswordWarningDialogHistogram[] =
    "PasswordProtection.ModalWarningDialogAction.SyncPasswordEntry";
const char kEnterprisePasswordAlertHistogram[] =
    "PasswordProtection.PasswordAlertModeOutcome."
    "NonGaiaEnterprisePasswordEntry";
const char kGsuiteSyncPasswordAlertHistogram[] =
    "PasswordProtection.PasswordAlertModeOutcome.GSuiteSyncPasswordEntry";
const char kGsuiteNonSyncPasswordAlertHistogram[] =
    "PasswordProtection.PasswordAlertModeOutcome.GSuiteNonSyncPasswordEntry";

const char kPasswordOnFocusRequestWithTokenHistogram[] =
    "PasswordProtection.RequestWithToken.PasswordFieldOnFocus";
const char kAnyPasswordEntryRequestWithTokenHistogram[] =
    "PasswordProtection.RequestWithToken.AnyPasswordEntry";

void LogPasswordProtectionRequestTokenHistogram(
    LoginReputationClientRequest::TriggerType trigger_type,
    bool has_access_token) {
  if (trigger_type == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE) {
    base::UmaHistogramBoolean(kPasswordOnFocusRequestWithTokenHistogram,
                              has_access_token);
  } else {
    base::UmaHistogramBoolean(kAnyPasswordEntryRequestWithTokenHistogram,
                              has_access_token);
  }
}

void LogPasswordEntryRequestOutcome(
    RequestOutcome outcome,
    ReusedPasswordAccountType password_account_type) {
  UMA_HISTOGRAM_ENUMERATION(kAnyPasswordEntryRequestOutcomeHistogram, outcome);

  bool is_gsuite_user =
      password_account_type.account_type() == ReusedPasswordAccountType::GSUITE;
  bool is_gmail_user =
      password_account_type.account_type() == ReusedPasswordAccountType::GMAIL;
  bool is_primary_account_password = password_account_type.is_account_syncing();
  if (is_primary_account_password) {
    base::UmaHistogramEnumeration(
        is_gsuite_user ? kGSuiteSyncPasswordEntryRequestOutcomeHistogram
                       : kGmailSyncPasswordEntryRequestOutcomeHistogram,
        outcome);
    UMA_HISTOGRAM_ENUMERATION(kSyncPasswordEntryRequestOutcomeHistogram,
                              outcome);
  } else if (password_account_type.account_type() ==
             ReusedPasswordAccountType::NON_GAIA_ENTERPRISE) {
    UMA_HISTOGRAM_ENUMERATION(kEnterprisePasswordEntryRequestOutcomeHistogram,
                              outcome);
  } else if (password_account_type.account_type() ==
             ReusedPasswordAccountType::SAVED_PASSWORD) {
    UMA_HISTOGRAM_ENUMERATION(kSavedPasswordEntryRequestOutcomeHistogram,
                              outcome);
  } else if (is_gsuite_user || is_gmail_user) {
    base::UmaHistogramEnumeration(
        is_gsuite_user ? kGSuiteNonSyncPasswordEntryRequestOutcomeHistogram
                       : kGmailNonSyncPasswordEntryRequestOutcomeHistogram,
        outcome);
    UMA_HISTOGRAM_ENUMERATION(kNonSyncPasswordEntryRequestOutcomeHistogram,
                              outcome);
  }
}

void LogPasswordOnFocusRequestOutcome(RequestOutcome outcome) {
  UMA_HISTOGRAM_ENUMERATION(kPasswordOnFocusRequestOutcomeHistogram, outcome);
}

void LogPasswordAlertModeOutcome(
    RequestOutcome outcome,
    ReusedPasswordAccountType password_account_type) {
  DCHECK_NE(ReusedPasswordAccountType::GMAIL,
            password_account_type.account_type());
  if (password_account_type.account_type() ==
      ReusedPasswordAccountType::NON_GAIA_ENTERPRISE) {
    UMA_HISTOGRAM_ENUMERATION(kEnterprisePasswordAlertHistogram, outcome);
  } else {
    if (password_account_type.is_account_syncing()) {
      UMA_HISTOGRAM_ENUMERATION(kGsuiteSyncPasswordAlertHistogram, outcome);
    } else {
      UMA_HISTOGRAM_ENUMERATION(kGsuiteNonSyncPasswordAlertHistogram, outcome);
    }
  }
}

void LogNoPingingReason(LoginReputationClientRequest::TriggerType trigger_type,
                        RequestOutcome reason,
                        ReusedPasswordAccountType password_account_type) {
  DCHECK(trigger_type == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE ||
         trigger_type == LoginReputationClientRequest::PASSWORD_REUSE_EVENT);

  if (trigger_type == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE) {
    UMA_HISTOGRAM_ENUMERATION(kPasswordOnFocusRequestOutcomeHistogram, reason);
  } else {
    LogPasswordEntryRequestOutcome(reason, password_account_type);
  }
}

void LogPasswordProtectionVerdict(
    LoginReputationClientRequest::TriggerType trigger_type,
    ReusedPasswordAccountType password_account_type,
    VerdictType verdict_type) {
  bool is_gsuite_user =
      password_account_type.account_type() == ReusedPasswordAccountType::GSUITE;
  bool is_gmail_user =
      password_account_type.account_type() == ReusedPasswordAccountType::GMAIL;
  bool is_account_syncing = password_account_type.is_account_syncing();

  switch (trigger_type) {
    case LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE:
      UMA_HISTOGRAM_ENUMERATION(
          kPasswordOnFocusVerdictHistogram, verdict_type,
          (LoginReputationClientResponse_VerdictType_VerdictType_MAX + 1));
      break;
    case LoginReputationClientRequest::PASSWORD_REUSE_EVENT:
      UMA_HISTOGRAM_ENUMERATION(
          kAnyPasswordEntryVerdictHistogram, verdict_type,
          (LoginReputationClientResponse_VerdictType_VerdictType_MAX + 1));
      if (is_account_syncing) {
        if (password_account_type.account_type() ==
            ReusedPasswordAccountType::UNKNOWN) {
          UMA_HISTOGRAM_ENUMERATION(
              kUnknownPrimaryAccountPasswordEntryVerdictHistogram, verdict_type,
              (LoginReputationClientResponse_VerdictType_VerdictType_MAX + 1));
        }
        UMA_HISTOGRAM_ENUMERATION(
            kSyncPasswordEntryVerdictHistogram, verdict_type,
            (LoginReputationClientResponse_VerdictType_VerdictType_MAX + 1));
      } else if (is_gsuite_user || is_gmail_user) {
        UMA_HISTOGRAM_ENUMERATION(
            kNonSyncPasswordEntryVerdictHistogram, verdict_type,
            (LoginReputationClientResponse_VerdictType_VerdictType_MAX + 1));
      }
      if (is_gsuite_user) {
        if (is_account_syncing) {
          UMA_HISTOGRAM_ENUMERATION(
              kGSuiteSyncPasswordEntryVerdictHistogram, verdict_type,
              (LoginReputationClientResponse_VerdictType_VerdictType_MAX + 1));
        } else {
          UMA_HISTOGRAM_ENUMERATION(
              kGSuiteNonSyncPasswordEntryVerdictHistogram, verdict_type,
              (LoginReputationClientResponse_VerdictType_VerdictType_MAX + 1));
        }
      } else if (is_gmail_user) {
        if (is_account_syncing) {
          UMA_HISTOGRAM_ENUMERATION(
              kGmailSyncPasswordEntryVerdictHistogram, verdict_type,
              (LoginReputationClientResponse_VerdictType_VerdictType_MAX + 1));
        } else {
          UMA_HISTOGRAM_ENUMERATION(
              kGmailNonSyncPasswordEntryVerdictHistogram, verdict_type,
              (LoginReputationClientResponse_VerdictType_VerdictType_MAX + 1));
        }
      } else if (password_account_type.account_type() ==
                 ReusedPasswordAccountType::NON_GAIA_ENTERPRISE) {
        UMA_HISTOGRAM_ENUMERATION(
            kEnterprisePasswordEntryVerdictHistogram, verdict_type,
            (LoginReputationClientResponse_VerdictType_VerdictType_MAX + 1));
      } else if (password_account_type.account_type() ==
                 ReusedPasswordAccountType::SAVED_PASSWORD) {
        UMA_HISTOGRAM_ENUMERATION(
            kSavedPasswordEntryVerdictHistogram, verdict_type,
            (LoginReputationClientResponse_VerdictType_VerdictType_MAX + 1));
      } else {
        UMA_HISTOGRAM_ENUMERATION(
            kUnknownNonPrimaryAccountPasswordEntryVerdictHistogram,
            verdict_type,
            (LoginReputationClientResponse_VerdictType_VerdictType_MAX + 1));
      }
      break;
    default:
      NOTREACHED_IN_MIGRATION();
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
    int net_error,
    const base::TimeTicks& request_start_time) {
  RecordHttpResponseOrErrorCode(
      "PasswordProtection.PasswordProtectionResponseOrErrorCode", net_error,
      response_code);
  if (response_code == net::HTTP_OK) {
    UMA_HISTOGRAM_TIMES("PasswordProtection.RequestNetworkDuration",
                        base::TimeTicks::Now() - request_start_time);
  }
}

void LogPasswordProtectionSampleReportSent() {
  base::UmaHistogramBoolean("PasswordProtection.SampleReportSent", true);
}

void LogWarningAction(WarningUIType ui_type,
                      WarningAction action,
                      ReusedPasswordAccountType password_account_type) {
  // |password_type| can be unknown if user directly navigates to
  // chrome://reset-password page. In this case, do not record user action.
  if (password_account_type.account_type() ==
          ReusedPasswordAccountType::UNKNOWN &&
      ui_type == WarningUIType::INTERSTITIAL) {
    return;
  }

  bool is_gsuite_user =
      password_account_type.account_type() == ReusedPasswordAccountType::GSUITE;
  bool is_primary_account_password = password_account_type.is_account_syncing();
  switch (ui_type) {
    case WarningUIType::PAGE_INFO:
      if (is_primary_account_password) {
        UMA_HISTOGRAM_ENUMERATION(kSyncPasswordPageInfoHistogram, action);
        if (is_gsuite_user) {
          UMA_HISTOGRAM_ENUMERATION(kGSuiteSyncPasswordPageInfoHistogram,
                                    action);
        } else {
          UMA_HISTOGRAM_ENUMERATION(kGmailSyncPasswordPageInfoHistogram,
                                    action);
        }
      } else if (password_account_type.account_type() ==
                 ReusedPasswordAccountType::NON_GAIA_ENTERPRISE) {
        UMA_HISTOGRAM_ENUMERATION(kEnterprisePasswordPageInfoHistogram, action);
      } else if (password_account_type.account_type() ==
                 ReusedPasswordAccountType::SAVED_PASSWORD) {
        UMA_HISTOGRAM_ENUMERATION(kSavedPasswordPageInfoHistogram, action);
      } else {
        UMA_HISTOGRAM_ENUMERATION(kNonSyncPasswordPageInfoHistogram, action);
        if (is_gsuite_user) {
          UMA_HISTOGRAM_ENUMERATION(kGSuiteNonSyncPasswordPageInfoHistogram,
                                    action);
        } else {
          UMA_HISTOGRAM_ENUMERATION(kGmailNonSyncPasswordPageInfoHistogram,
                                    action);
        }
      }
      break;
    case WarningUIType::MODAL_DIALOG:
      if (is_primary_account_password) {
        UMA_HISTOGRAM_ENUMERATION(kSyncPasswordWarningDialogHistogram, action);
        if (is_gsuite_user) {
          UMA_HISTOGRAM_ENUMERATION(kGSuiteSyncPasswordWarningDialogHistogram,
                                    action);
        } else {
          UMA_HISTOGRAM_ENUMERATION(kGmailSyncPasswordWarningDialogHistogram,
                                    action);
        }
      } else if (password_account_type.account_type() ==
                 ReusedPasswordAccountType::NON_GAIA_ENTERPRISE) {
        UMA_HISTOGRAM_ENUMERATION(kEnterprisePasswordWarningDialogHistogram,
                                  action);
      } else if (password_account_type.account_type() ==
                 ReusedPasswordAccountType::SAVED_PASSWORD) {
        UMA_HISTOGRAM_ENUMERATION(kSavedPasswordWarningDialogHistogram, action);
      } else {
        UMA_HISTOGRAM_ENUMERATION(kNonSyncPasswordWarningDialogHistogram,
                                  action);
        if (is_gsuite_user) {
          UMA_HISTOGRAM_ENUMERATION(
              kGSuiteNonSyncPasswordWarningDialogHistogram, action);
        } else {
          UMA_HISTOGRAM_ENUMERATION(kGmailNonSyncPasswordWarningDialogHistogram,
                                    action);
        }
      }
      break;
    case WarningUIType::INTERSTITIAL:
      if (is_primary_account_password) {
        UMA_HISTOGRAM_ENUMERATION(kSyncPasswordInterstitialHistogram, action);
        if (is_gsuite_user) {
          UMA_HISTOGRAM_ENUMERATION(kGSuiteSyncPasswordInterstitialHistogram,
                                    action);
        }
      } else if (password_account_type.account_type() ==
                 ReusedPasswordAccountType::NON_GAIA_ENTERPRISE) {
        UMA_HISTOGRAM_ENUMERATION(kEnterprisePasswordInterstitialHistogram,
                                  action);
      } else {
        UMA_HISTOGRAM_ENUMERATION(kNonSyncPasswordInterstitialHistogram,
                                  action);
        if (is_gsuite_user) {
          UMA_HISTOGRAM_ENUMERATION(kGSuiteNonSyncPasswordInterstitialHistogram,
                                    action);
        } else {
          UMA_HISTOGRAM_ENUMERATION(kGmailNonSyncPasswordInterstitialHistogram,
                                    action);
        }
      }
      break;
    case WarningUIType::NOT_USED:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void LogModalWarningDialogLifetime(
    base::TimeTicks modal_construction_start_time) {
  UMA_HISTOGRAM_MEDIUM_TIMES(
      "PasswordProtection.ModalWarningDialogLifetime",
      base::TimeTicks::Now() - modal_construction_start_time);
}

}  // namespace safe_browsing
