// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_metrics.h"

#include <limits.h>

#include <string_view>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace signin_metrics {

namespace {

std::string_view GetPromoActionHistogramSuffix(PromoAction promo_action) {
  switch (promo_action) {
    case PromoAction::PROMO_ACTION_WITH_DEFAULT:
      return ".WithDefault";
    case PromoAction::PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT:
      return ".NewAccountNoExistingAccount";
    case PromoAction::PROMO_ACTION_NEW_ACCOUNT_EXISTING_ACCOUNT:
    case PromoAction::PROMO_ACTION_NOT_DEFAULT:
      NOTIMPLEMENTED()
          << "Those promo actions have no histogram equivalent yet, you need "
             "to implement the histogram and return the correct histogram "
             "suffix.";
      return "NOT IMPLEMENTED";
    case PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO:
      NOTREACHED() << "No signin promo should not record metrics.";
  }
}

#if BUILDFLAG(IS_IOS)
std::string_view ReauthFlowEventToHistogramSuffix(ReauthFlowEvent event) {
  switch (event) {
    case ReauthFlowEvent::kStarted:
      return ".Started";
    case ReauthFlowEvent::kCompleted:
      return ".Completed";
    case ReauthFlowEvent::kError:
      return ".Error";
    case ReauthFlowEvent::kCancelled:
      return ".Cancelled";
    case ReauthFlowEvent::kInterrupted:
      return ".Interrupted";
  }
}
#endif  // BUILDFLAG(IS_IOS)

}  // namespace

// These intermediate macros are necessary when we may emit to different
// histograms from the same logical place in the code. The base histogram macros
// expand in a way that can only work for a single histogram name, so these
// allow a single place in the code to fan out for multiple names.
#define INVESTIGATOR_HISTOGRAM_CUSTOM_COUNTS(name, type, sample, min, max, \
                                             bucket_count)                 \
  switch (type) {                                                          \
    case ReportingType::PERIODIC:                                          \
      UMA_HISTOGRAM_CUSTOM_COUNTS(name "_Periodic", sample, min, max,      \
                                  bucket_count);                           \
      break;                                                               \
    case ReportingType::ON_CHANGE:                                         \
      UMA_HISTOGRAM_CUSTOM_COUNTS(name "_OnChange", sample, min, max,      \
                                  bucket_count);                           \
      break;                                                               \
  }

#define INVESTIGATOR_HISTOGRAM_BOOLEAN(name, type, sample) \
  switch (type) {                                          \
    case ReportingType::PERIODIC:                          \
      UMA_HISTOGRAM_BOOLEAN(name "_Periodic", sample);     \
      break;                                               \
    case ReportingType::ON_CHANGE:                         \
      UMA_HISTOGRAM_BOOLEAN(name "_OnChange", sample);     \
      break;                                               \
  }

#define INVESTIGATOR_HISTOGRAM_ENUMERATION(name, type, sample, boundary_value) \
  switch (type) {                                                              \
    case ReportingType::PERIODIC:                                              \
      UMA_HISTOGRAM_ENUMERATION(name "_Periodic", sample, boundary_value);     \
      break;                                                                   \
    case ReportingType::ON_CHANGE:                                             \
      UMA_HISTOGRAM_ENUMERATION(name "_OnChange", sample, boundary_value);     \
      break;                                                                   \
  }

void LogSigninAccessPointStarted(AccessPoint access_point,
                                 PromoAction promo_action) {
  UMA_HISTOGRAM_ENUMERATION("Signin.SigninStartedAccessPoint", access_point);
  switch (promo_action) {
    case PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO:
      break;
    case PromoAction::PROMO_ACTION_WITH_DEFAULT:
      UMA_HISTOGRAM_ENUMERATION("Signin.SigninStartedAccessPoint.WithDefault",
                                access_point);
      break;
    case PromoAction::PROMO_ACTION_NOT_DEFAULT:
      UMA_HISTOGRAM_ENUMERATION("Signin.SigninStartedAccessPoint.NotDefault",
                                access_point);
      break;
    case PromoAction::PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT:
      UMA_HISTOGRAM_ENUMERATION(
          "Signin.SigninStartedAccessPoint.NewAccountNoExistingAccount",
          access_point);
      break;
    case PromoAction::PROMO_ACTION_NEW_ACCOUNT_EXISTING_ACCOUNT:
      UMA_HISTOGRAM_ENUMERATION(
          "Signin.SigninStartedAccessPoint.NewAccountExistingAccount",
          access_point);
      break;
  }
}

void LogSigninAccessPointCompleted(AccessPoint access_point,
                                   PromoAction promo_action) {
  UMA_HISTOGRAM_ENUMERATION("Signin.SigninCompletedAccessPoint", access_point);
  switch (promo_action) {
    case PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO:
      break;
    case PromoAction::PROMO_ACTION_WITH_DEFAULT:
      UMA_HISTOGRAM_ENUMERATION("Signin.SigninCompletedAccessPoint.WithDefault",
                                access_point);
      break;
    case PromoAction::PROMO_ACTION_NOT_DEFAULT:
      UMA_HISTOGRAM_ENUMERATION("Signin.SigninCompletedAccessPoint.NotDefault",
                                access_point);
      break;
    case PromoAction::PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT:
      UMA_HISTOGRAM_ENUMERATION(
          "Signin.SigninCompletedAccessPoint.NewAccountNoExistingAccount",
          access_point);
      break;
    case PromoAction::PROMO_ACTION_NEW_ACCOUNT_EXISTING_ACCOUNT:
      UMA_HISTOGRAM_ENUMERATION(
          "Signin.SigninCompletedAccessPoint.NewAccountExistingAccount",
          access_point);
      break;
  }
}

void LogSignInOffered(AccessPoint access_point, PromoAction promo_action) {
  static constexpr char signin_offered_base_histogram[] =
      "Signin.SignIn.Offered";

  // Log the generic offered histogram.
  base::UmaHistogramEnumeration(signin_offered_base_histogram, access_point);

  // Do not record the histogram with a promo suffix if this is invoked/recorded
  // from a non-promo context.
  if (promo_action == PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO) {
    return;
  }

  // Log the specific offered histogram based on the `promo_action`.
  base::UmaHistogramEnumeration(
      base::StrCat({signin_offered_base_histogram,
                    GetPromoActionHistogramSuffix(promo_action)}),
      access_point);
}

void LogSignInStarted(AccessPoint access_point) {
  base::UmaHistogramEnumeration("Signin.SignIn.Started", access_point);
}

void LogSigninPendingOffered(AccessPoint access_point) {
  base::UmaHistogramEnumeration("Signin.SigninPending.Offered", access_point);
}

#if BUILDFLAG(IS_IOS)
void LogSigninWithAccountType(SigninAccountType account_type) {
  base::UmaHistogramEnumeration("Signin.AccountType.SigninConsent",
                                account_type);
}
#endif  // BUILDFLAG(IS_IOS)

void LogSyncOptInStarted(AccessPoint access_point) {
  base::UmaHistogramEnumeration("Signin.SyncOptIn.Started", access_point);
}

void LogSyncOptInOffered(AccessPoint access_point) {
  base::UmaHistogramEnumeration("Signin.SyncOptIn.Offered", access_point);
}

void LogHistorySyncOptInOffered(AccessPoint access_point) {
  base::UmaHistogramEnumeration("Signin.HistorySyncOptIn.Offered",
                                access_point);
}

void LogSyncSettingsOpened(AccessPoint access_point) {
  base::UmaHistogramEnumeration("Signin.SyncOptIn.OpenedSyncSettings",
                                access_point);
}

void RecordAccountsPerProfile(int total_number_accounts) {
  UMA_HISTOGRAM_COUNTS_100("Profile.NumberOfAccountsPerProfile",
                           total_number_accounts);
}

void LogSigninAccountReconciliationDuration(base::TimeDelta duration,
                                            bool successful) {
  if (successful) {
    UMA_HISTOGRAM_CUSTOM_TIMES("Signin.Reconciler.Duration.UpTo3mins.Success",
                               duration, base::Milliseconds(1),
                               base::Minutes(3), 100);
  } else {
    UMA_HISTOGRAM_CUSTOM_TIMES("Signin.Reconciler.Duration.UpTo3mins.Failure",
                               duration, base::Milliseconds(1),
                               base::Minutes(3), 100);
  }
}

void LogSignout(ProfileSignout source_metric) {
  base::UmaHistogramEnumeration("Signin.SignoutProfile", source_metric);
}

void LogExternalCcResultFetches(
    bool fetches_completed,
    const base::TimeDelta& time_to_check_connections) {
  UMA_HISTOGRAM_BOOLEAN("Signin.Reconciler.AllExternalCcResultCompleted",
                        fetches_completed);
  if (fetches_completed) {
    UMA_HISTOGRAM_TIMES("Signin.Reconciler.ExternalCcResultTime.Completed",
                        time_to_check_connections);
  } else {
    UMA_HISTOGRAM_TIMES("Signin.Reconciler.ExternalCcResultTime.NotCompleted",
                        time_to_check_connections);
  }
}

void LogAuthError(const GoogleServiceAuthError& auth_error) {
  UMA_HISTOGRAM_ENUMERATION("Signin.AuthError", auth_error.state(),
                            GoogleServiceAuthError::State::NUM_STATES);
  if (auth_error.state() == GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS) {
    UMA_HISTOGRAM_ENUMERATION(
        "Signin.InvalidGaiaCredentialsReason",
        auth_error.GetInvalidGaiaCredentialsReason(),
        GoogleServiceAuthError::InvalidGaiaCredentialsReason::NUM_REASONS);
  }
}

void LogAccountReconcilorStateOnGaiaResponse(AccountReconcilorState state) {
  UMA_HISTOGRAM_ENUMERATION("Signin.AccountReconcilorState.OnGaiaResponse",
                            state);
}

void LogCookieJarStableAge(const base::TimeDelta stable_age,
                           const ReportingType type) {
  INVESTIGATOR_HISTOGRAM_CUSTOM_COUNTS(
      "Signin.CookieJar.StableAge", type,
      base::saturated_cast<int>(stable_age.InSeconds()), 1,
      base::saturated_cast<int>(base::Days(365).InSeconds()), 100);
}

void LogCookieJarCounts(const int signed_in,
                        const int signed_out,
                        const int total,
                        const ReportingType type) {
  INVESTIGATOR_HISTOGRAM_CUSTOM_COUNTS("Signin.CookieJar.SignedInCount", type,
                                       signed_in, 1, 10, 10);
  INVESTIGATOR_HISTOGRAM_CUSTOM_COUNTS("Signin.CookieJar.SignedOutCount", type,
                                       signed_out, 1, 10, 10);
  INVESTIGATOR_HISTOGRAM_CUSTOM_COUNTS("Signin.CookieJar.TotalCount", type,
                                       total, 1, 10, 10);
}

void LogAccountRelation(const AccountRelation relation,
                        const ReportingType type) {
  INVESTIGATOR_HISTOGRAM_ENUMERATION(
      "Signin.CookieJar.ChromeAccountRelation2", type,
      static_cast<int>(relation),
      static_cast<int>(AccountRelation::HISTOGRAM_COUNT));
}

void LogIsShared(const bool is_shared, const ReportingType type) {
  INVESTIGATOR_HISTOGRAM_BOOLEAN("Signin.IsShared", type, is_shared);
}

void LogSignedInCookiesCountsPerPrimaryAccountType(int signed_in_accounts_count,
                                                   bool primary_syncing,
                                                   bool primary_managed) {
  constexpr int kMaxBucket = 10;
  if (primary_syncing) {
    if (primary_managed) {
      base::UmaHistogramExactLinear(
          "Signin.CookieJar.SignedInCountWithPrimary.SyncEnterprise",
          signed_in_accounts_count, kMaxBucket);
    } else {
      base::UmaHistogramExactLinear(
          "Signin.CookieJar.SignedInCountWithPrimary.SyncConsumer",
          signed_in_accounts_count, kMaxBucket);
    }
  } else {
    if (primary_managed) {
      base::UmaHistogramExactLinear(
          "Signin.CookieJar.SignedInCountWithPrimary.NoSyncEnterprise",
          signed_in_accounts_count, kMaxBucket);
    } else {
      base::UmaHistogramExactLinear(
          "Signin.CookieJar.SignedInCountWithPrimary.NoSyncConsumer",
          signed_in_accounts_count, kMaxBucket);
    }
  }
}

void RecordRefreshTokenUpdatedFromSource(
    bool refresh_token_is_valid,
    SourceForRefreshTokenOperation source) {
  if (refresh_token_is_valid) {
    UMA_HISTOGRAM_ENUMERATION("Signin.RefreshTokenUpdated.ToValidToken.Source",
                              source);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "Signin.RefreshTokenUpdated.ToInvalidToken.Source", source);
  }
}

void RecordRefreshTokenRevokedFromSource(
    SourceForRefreshTokenOperation source) {
  UMA_HISTOGRAM_ENUMERATION("Signin.RefreshTokenRevoked.Source", source);
}

#if BUILDFLAG(IS_IOS)
void RecordSignoutConfirmationFromDataLossAlert(
    SignoutDataLossAlertReason reason,
    bool signout_confirmed) {
  const char* histogram;
  switch (reason) {
    case SignoutDataLossAlertReason::kSignoutWithUnsyncedData:
      histogram = "Sync.SignoutWithUnsyncedData";
      break;
    case SignoutDataLossAlertReason::kSignoutWithClearDataForManagedUser:
      histogram = "Signin.SignoutAndClearDataFromManagedAccount";
      break;
  }
  base::UmaHistogramBoolean(histogram, signout_confirmed);
}

void RecordReauthFlowEventInSigninFlow(signin_metrics::AccessPoint access_point,
                                       ReauthFlowEvent event) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Signin.Reauth.InSigninFlow",
                    ReauthFlowEventToHistogramSuffix(event)}),
      access_point);
}

void RecordReauthFlowEventInExplicitFlow(ReauthAccessPoint access_point,
                                         ReauthFlowEvent event) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Signin.Reauth.InExplicitFlow",
                    ReauthFlowEventToHistogramSuffix(event)}),
      access_point);
}
#endif  // BUILDFLAG(IS_IOS)

void RecordOpenTabCountOnSignin(signin::ConsentLevel consent_level,
                                size_t tabs_count) {
  std::string_view consent_level_token =
      consent_level == signin::ConsentLevel::kSignin ? ".OnSignin" : ".OnSync";
  base::UmaHistogramCounts1000(
      base::StrCat({"Signin.OpenTabsCount", consent_level_token}), tabs_count);
}

void RecordHistoryOptInStateOnSignin(signin_metrics::AccessPoint access_point,
                                     signin::ConsentLevel consent_level,
                                     bool opted_in) {
  std::string_view consent_level_token =
      consent_level == signin::ConsentLevel::kSignin ? ".OnSignin" : ".OnSync";
  base::UmaHistogramBoolean(
      base::StrCat({"Signin.HistoryOptInState", consent_level_token}),
      opted_in);

  if (opted_in) {
    base::UmaHistogramEnumeration(
        base::StrCat(
            {"Signin.HistoryAlreadyOptedInAccessPoint", consent_level_token}),
        access_point);
  }
}

// --------------------------------------------------------------
// User actions
// --------------------------------------------------------------

void RecordSigninUserActionForAccessPoint(AccessPoint access_point) {
  switch (access_point) {
    case AccessPoint::kStartPage:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromStartPage"));
      break;
    case AccessPoint::kNtpLink:
      base::RecordAction(base::UserMetricsAction("Signin_Signin_FromNTP"));
      break;
    case AccessPoint::kMenu:
      base::RecordAction(base::UserMetricsAction("Signin_Signin_FromMenu"));
      break;
    case AccessPoint::kSettings:
      base::RecordAction(base::UserMetricsAction("Signin_Signin_FromSettings"));
      break;
    case AccessPoint::kSettingsYourSavedInfo:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromYourSavedInfo"));
      break;
    case AccessPoint::kSupervisedUser:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromSupervisedUser"));
      break;
    case AccessPoint::kExtensionInstallBubble:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromExtensionInstallBubble"));
      break;
    case AccessPoint::kExtensions:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromExtensions"));
      break;
    case AccessPoint::kBookmarkBubble:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromBookmarkBubble"));
      break;
    case AccessPoint::kBookmarkManager:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromBookmarkManager"));
      break;
    case AccessPoint::kAvatarBubbleSignIn:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromAvatarBubbleSignin"));
      break;
    case AccessPoint::kUserManager:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromUserManager"));
      break;
    case AccessPoint::kDevicesPage:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromDevicesPage"));
      break;
    case AccessPoint::kFullscreenSigninPromo:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromSigninPromo"));
      break;
    case AccessPoint::kRecentTabs:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromRecentTabs"));
      break;
    case AccessPoint::kUnknown:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromUnknownAccessPoint"));
      break;
    case AccessPoint::kPasswordBubble:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromPasswordBubble"));
      break;
    case AccessPoint::kAutofillDropdown:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromAutofillDropdown"));
      break;
    case AccessPoint::kResigninInfobar:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromReSigninInfobar"));
      break;
    case AccessPoint::kTabSwitcher:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromTabSwitcher"));
      break;
    case AccessPoint::kMachineLogon:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromMachineLogon"));
      break;
    case AccessPoint::kGoogleServicesSettings:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromGoogleServicesSettings"));
      break;
    case AccessPoint::kEnterpriseSignoutCoordinator:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromEnterpriseSignoutSheet"));
      break;
    case AccessPoint::kSigninInterceptFirstRunExperience:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Signin_FromSigninInterceptFirstRunExperience"));
      break;
    case AccessPoint::kNtpFeedTopPromo:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromNTPFeedTopPromo"));
      break;
    case AccessPoint::kKaleidoscope:
      NOTREACHED() << "Access point " << static_cast<int>(access_point)
                   << " is only used to trigger non-sync sign-in and this"
                   << " action should only be triggered for sync-enabled"
                   << " sign-ins.";
    case AccessPoint::kSyncErrorCard:
    case AccessPoint::kForcedSignin:
    case AccessPoint::kAccountRenamed:
    case AccessPoint::kWebSignin:
    case AccessPoint::kSaveToDriveIos:
    case AccessPoint::kSaveToPhotosIos:
    case AccessPoint::kSettingsSyncOffRow:
    case AccessPoint::kPostDeviceRestoreBackgroundSignin:
    case AccessPoint::kRestorePrimaryAccountOnProfileLoad:
    case AccessPoint::kDesktopSigninManager:
    case AccessPoint::kSigninChoiceRemembered:
    case AccessPoint::kProfileMenuSignoutConfirmationPrompt:
    case AccessPoint::kSettingsSignoutConfirmationPrompt:
    case AccessPoint::kWebauthnModalDialog:
    case AccessPoint::kCctAccountMismatchNotification:
    case AccessPoint::kDriveFilePickerIos:
    case AccessPoint::kHistoryPage:
    case AccessPoint::kWidget:
    case AccessPoint::kHistorySyncEducationalTip:
    case AccessPoint::kManagedProfileAutoSigninIos:
      NOTREACHED() << "Access point " << static_cast<int>(access_point)
                   << " is not supposed to log signin user actions.";
    case AccessPoint::kCollaborationShareTabGroup:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Signin_FromCollaborationShareTabGroup"));
      break;
    case AccessPoint::kCollaborationJoinTabGroup:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Signin_FromCollaborationJoinTabGroup"));
      break;
    case AccessPoint::kCollaborationLeaveOrDeleteTabGroup:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Signin_FromCollaborationLeaveOrDeleteTabGroup"));
      break;
    case AccessPoint::kSafetyCheck:
      VLOG(1) << "Signin_Signin_From* user action is not recorded "
              << "for access point " << static_cast<int>(access_point);
      break;
    case AccessPoint::kSendTabToSelfPromo:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromSendTabToSelfPromo"));
      break;
    case AccessPoint::kPostDeviceRestoreSigninPromo:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Signin_FromPostDeviceRestoreSigninPromo"));
      break;
    case AccessPoint::kNtpSignedOutIcon:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromNTPSignedOutIcon"));
      break;
    case AccessPoint::kNtpFeedCardMenuPromo:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Signin_FromNTPFeedCardMenuSigninPromo"));
      break;
    case AccessPoint::kNtpFeedBottomPromo:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Signin_FromNTPFeedBottomSigninPromo"));
      break;
    case AccessPoint::kForYouFre:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromForYouFre"));
      break;
    case AccessPoint::kCreatorFeedFollow:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromCreatorFeedFollow"));
      break;
    case AccessPoint::kReadingList:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromReadingList"));
      break;
    case signin_metrics::AccessPoint::kReauthInfoBar:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromReauthInfoBar"));
      break;
    case signin_metrics::AccessPoint::kAccountConsistencyService:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Signin_FromAccountConsistencyService"));
      break;
    case AccessPoint::kSearchCompanion:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromSearchCompanion"));
      break;
    case AccessPoint::kSetUpList:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromSetUpList"));
      break;
    case AccessPoint::kChromeSigninInterceptBubble:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Signin_FromChromeSigninInterceptBubble"));
      break;
    case AccessPoint::kTabOrganization:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromTabOrganization"));
      break;
    case AccessPoint::kTipsNotification:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromTipsNotification"));
      break;
    case AccessPoint::kNotificationsOptInScreenContentToggle:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Signin_FromNotificationsOptInScreenContentToggle"));
      break;
    case AccessPoint::kNtpIdentityDisc:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromNtpIdentityDisc"));
      break;
    case AccessPoint::kOidcRedirectionInterception:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Signin_FromOidcRedirectionInterception"));
      break;
    case AccessPoint::kAvatarBubbleSignInWithSyncPromo:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Signin_FromAvatarBubbleSigninWithSyncPromo"));
      break;
    case AccessPoint::kAccountMenuSwitchAccount:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromAccountMenu"));
      break;
    case AccessPoint::kProductSpecifications:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromProductSpecifications"));
      break;
    case AccessPoint::kAccountMenuSwitchAccountFailed:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromAccountMenuFailedSwitch"));
      break;
    case AccessPoint::kAddressBubble:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromAddressBubble"));
      break;
    case AccessPoint::kGlicLaunchButton:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromGlicLaunchButton"));
      break;
    case AccessPoint::kHistorySyncOptinExpansionPillOnStartup:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Signin_FromHistorySyncOptinExpansionPillOnStartup"));
      break;
    case AccessPoint::kNonModalSigninPasswordPromo:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Signin_FromNonModalSigninPasswordPromo"));
      break;
    case AccessPoint::kNonModalSigninBookmarkPromo:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Signin_FromNonModalSigninBookmarkPromo"));
      break;
    case AccessPoint::kUserManagerWithPrefilledEmail:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Signin_FromUserManagerWithPrefilledEmail"));
      break;
    case AccessPoint::kEnterpriseManagementDisclaimerAtStartup:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Signin_FromEnterpriseManagementDisclaimerAtStartup"));
      break;
    case AccessPoint::kEnterpriseManagementDisclaimerAfterBrowserFocus:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Signin_FromEnterpriseManagementDisclaimerAfterBrowserFocus"));
      break;
    case AccessPoint::kEnterpriseManagementDisclaimerAfterSignin:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Signin_FromEnterpriseManagementDisclaimerAfterSignin"));
      break;
    case AccessPoint::kNtpFeaturePromo:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromNtpFeaturePromo"));
      break;
    case AccessPoint::kEnterpriseDialogAfterSigninInterception:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Signin_FromEnterpriseDialogAfterSigninInterception"));
      break;
  }
}

void RecordSigninImpressionUserActionForAccessPoint(AccessPoint access_point) {
  switch (access_point) {
    case AccessPoint::kStartPage:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromStartPage"));
      break;
    case AccessPoint::kNtpLink:
      base::RecordAction(base::UserMetricsAction("Signin_Impression_FromNTP"));
      break;
    case AccessPoint::kMenu:
      base::RecordAction(base::UserMetricsAction("Signin_Impression_FromMenu"));
      break;
    case AccessPoint::kSettings:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromSettings"));
      break;
    case AccessPoint::kSettingsYourSavedInfo:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromYourSavedInfo"));
      break;
    case AccessPoint::kExtensionInstallBubble:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Impression_FromExtensionInstallBubble"));
      break;
    case AccessPoint::kBookmarkBubble:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromBookmarkBubble"));
      break;
    case AccessPoint::kBookmarkManager:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromBookmarkManager"));
      break;
    case AccessPoint::kAvatarBubbleSignIn:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromAvatarBubbleSignin"));
      break;
    case AccessPoint::kDevicesPage:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromDevicesPage"));
      break;
    case AccessPoint::kFullscreenSigninPromo:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromSigninPromo"));
      break;
    case AccessPoint::kRecentTabs:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromRecentTabs"));
      break;
    case AccessPoint::kPasswordBubble:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromPasswordBubble"));
      break;
    case AccessPoint::kAutofillDropdown:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromAutofillDropdown"));
      break;
    case AccessPoint::kResigninInfobar:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromReSigninInfobar"));
      break;
    case AccessPoint::kTabSwitcher:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromTabSwitcher"));
      break;
    case AccessPoint::kGoogleServicesSettings:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Impression_FromGoogleServicesSettings"));
      break;
    case AccessPoint::kKaleidoscope:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromKaleidoscope"));
      break;
    case AccessPoint::kUserManager:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromUserManager"));
      break;
    case AccessPoint::kSendTabToSelfPromo:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromSendTabToSelfPromo"));
      break;
    case AccessPoint::kNtpFeedTopPromo:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromNTPFeedTopPromo"));
      break;
    case AccessPoint::kPostDeviceRestoreSigninPromo:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Impression_FromPostDeviceRestoreSigninPromo"));
      break;
    case AccessPoint::kNtpFeedCardMenuPromo:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Impression_FromNTPFeedCardMenuSigninPromo"));
      break;
    case AccessPoint::kNtpFeedBottomPromo:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Impression_FromNTPFeedBottomSigninPromo"));
      break;
    case AccessPoint::kCreatorFeedFollow:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromCreatorFeedFollow"));
      break;
    case AccessPoint::kReadingList:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromReadingList"));
      break;
    case AccessPoint::kSearchCompanion:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromSearchCompanion"));
      break;
    case AccessPoint::kSetUpList:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromSetUpList"));
      break;
    case AccessPoint::kChromeSigninInterceptBubble:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Impression_FromChromeSigninInterceptBubble"));
      break;
    case AccessPoint::kTipsNotification:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromTipsNotification"));
      break;
    case AccessPoint::kNotificationsOptInScreenContentToggle:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Impression_FromNotificationsOptInScreenContentToggle"));
      break;
    case AccessPoint::kProductSpecifications:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Impression_FromProductSpecifications"));
      break;
    case AccessPoint::kAddressBubble:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromAddressBubble"));
      break;
    case AccessPoint::kUserManagerWithPrefilledEmail:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Impression_FromUserManagerWithPrefilledEmail"));
      break;
    case AccessPoint::kEnterpriseDialogAfterSigninInterception:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Impression_FromEnterpriseDialogAfterSigninInterception"));
      break;
    case AccessPoint::kEnterpriseSignoutCoordinator:
    case AccessPoint::kExtensions:
    case AccessPoint::kSupervisedUser:
    case AccessPoint::kUnknown:
    case AccessPoint::kMachineLogon:
    case AccessPoint::kSyncErrorCard:
    case AccessPoint::kForcedSignin:
    case AccessPoint::kAccountRenamed:
    case AccessPoint::kWebSignin:
    case AccessPoint::kSigninChoiceRemembered:
    case AccessPoint::kSafetyCheck:
    case AccessPoint::kSigninInterceptFirstRunExperience:
    case AccessPoint::kSettingsSyncOffRow:
    case AccessPoint::kPostDeviceRestoreBackgroundSignin:
    case AccessPoint::kNtpSignedOutIcon:
    case AccessPoint::kDesktopSigninManager:
    case AccessPoint::kForYouFre:
    case AccessPoint::kSaveToDriveIos:
    case AccessPoint::kSaveToPhotosIos:
    case AccessPoint::kReauthInfoBar:
    case AccessPoint::kAccountConsistencyService:
    case AccessPoint::kRestorePrimaryAccountOnProfileLoad:
    case AccessPoint::kTabOrganization:
    case AccessPoint::kProfileMenuSignoutConfirmationPrompt:
    case AccessPoint::kSettingsSignoutConfirmationPrompt:
    case AccessPoint::kNtpIdentityDisc:
    case AccessPoint::kOidcRedirectionInterception:
    case AccessPoint::kWebauthnModalDialog:
    case AccessPoint::kAvatarBubbleSignInWithSyncPromo:
    case AccessPoint::kAccountMenuSwitchAccount:
    case AccessPoint::kAccountMenuSwitchAccountFailed:
    case AccessPoint::kCctAccountMismatchNotification:
    case AccessPoint::kDriveFilePickerIos:
    case AccessPoint::kCollaborationShareTabGroup:
    case AccessPoint::kGlicLaunchButton:
    case AccessPoint::kHistoryPage:
    case AccessPoint::kCollaborationJoinTabGroup:
    case AccessPoint::kHistorySyncOptinExpansionPillOnStartup:
    case AccessPoint::kWidget:
    case AccessPoint::kCollaborationLeaveOrDeleteTabGroup:
    case AccessPoint::kHistorySyncEducationalTip:
    case AccessPoint::kManagedProfileAutoSigninIos:
    case AccessPoint::kNonModalSigninPasswordPromo:
    case AccessPoint::kNonModalSigninBookmarkPromo:
    case AccessPoint::kEnterpriseManagementDisclaimerAtStartup:
    case AccessPoint::kEnterpriseManagementDisclaimerAfterBrowserFocus:
    case AccessPoint::kEnterpriseManagementDisclaimerAfterSignin:
    case AccessPoint::kNtpFeaturePromo:
      NOTREACHED() << "Signin_Impression_From* user actions are not recorded "
                      "for access point "
                   << static_cast<int>(access_point);
  }
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
void RecordConsistencyPromoUserAction(AccountConsistencyPromoAction action,
                                      AccessPoint access_point) {
  base::UmaHistogramEnumeration("Signin.AccountConsistencyPromoAction", action);

  // Log to the appropriate histogram given the action.
  std::string histogram;
  switch (action) {
    case AccountConsistencyPromoAction::SUPPRESSED_NO_ACCOUNTS:
      histogram = "Signin.AccountConsistencyPromoAction.SuppressedNoAccounts";
      break;
    case AccountConsistencyPromoAction::DISMISSED_BACK:
      histogram = "Signin.AccountConsistencyPromoAction.DismissedBack";
      break;
    case AccountConsistencyPromoAction::DISMISSED_BUTTON:
      histogram = "Signin.AccountConsistencyPromoAction.DismissedButton";
      break;
    case AccountConsistencyPromoAction::DISMISSED_SCRIM:
      histogram = "Signin.AccountConsistencyPromoAction.DismissedScrim";
      break;
    case AccountConsistencyPromoAction::DISMISSED_SWIPE_DOWN:
      histogram = "Signin.AccountConsistencyPromoAction.DismissedSwipeDown";
      break;
    case AccountConsistencyPromoAction::DISMISSED_OTHER:
      histogram = "Signin.AccountConsistencyPromoAction.DismissedOther";
      break;
    case AccountConsistencyPromoAction::ADD_ACCOUNT_STARTED:
      histogram = "Signin.AccountConsistencyPromoAction.AddAccountStarted";
      break;
    case AccountConsistencyPromoAction::SIGNED_IN_WITH_DEFAULT_ACCOUNT:
      histogram =
          "Signin.AccountConsistencyPromoAction.SignedInWithDefaultAccount";
      break;
    case AccountConsistencyPromoAction::SIGNED_IN_WITH_NON_DEFAULT_ACCOUNT:
      histogram =
          "Signin.AccountConsistencyPromoAction.SignedInWithNonDefaultAccount";
      break;
    case AccountConsistencyPromoAction::SIGNED_IN_WITH_NO_DEVICE_ACCOUNT:
      histogram =
          "Signin.AccountConsistencyPromoAction.SignedInWithNoDeviceAccount";
      break;
    case AccountConsistencyPromoAction::SHOWN:
      histogram = "Signin.AccountConsistencyPromoAction.Shown";
      break;
    case AccountConsistencyPromoAction::SHOWN_WITH_NO_DEVICE_ACCOUNT:
      histogram =
          "Signin.AccountConsistencyPromoAction.ShownWithNoDeviceAccount";
      break;
    case AccountConsistencyPromoAction::SUPPRESSED_SIGNIN_NOT_ALLOWED:
      histogram =
          "Signin.AccountConsistencyPromoAction.SuppressedSigninNotAllowed";
      break;
    case AccountConsistencyPromoAction::SIGNED_IN_WITH_ADDED_ACCOUNT:
      histogram =
          "Signin.AccountConsistencyPromoAction.SignedInWithAddedAccount";
      break;
    case AccountConsistencyPromoAction::AUTH_ERROR_SHOWN:
      histogram = "Signin.AccountConsistencyPromoAction.AuthErrorShown";
      break;
    case AccountConsistencyPromoAction::GENERIC_ERROR_SHOWN:
      histogram = "Signin.AccountConsistencyPromoAction.GenericErrorShown";
      break;
    case AccountConsistencyPromoAction::ADD_ACCOUNT_COMPLETED:
      histogram = "Signin.AccountConsistencyPromoAction.AddAccountCompleted";
      break;
    case AccountConsistencyPromoAction::
        ADD_ACCOUNT_COMPLETED_WITH_NO_DEVICE_ACCOUNT:
      histogram =
          "Signin.AccountConsistencyPromoAction."
          "AddAccountCompletedWithNoDeviceAccount";
      break;
    case AccountConsistencyPromoAction::SUPPRESSED_CONSECUTIVE_DISMISSALS:
      histogram =
          "Signin.AccountConsistencyPromoAction."
          "SuppressedConsecutiveDismissals";
      break;
    case AccountConsistencyPromoAction::TIMEOUT_ERROR_SHOWN:
      histogram = "Signin.AccountConsistencyPromoAction.TimeoutErrorShown";
      break;
    case AccountConsistencyPromoAction::SUPPRESSED_ALREADY_SIGNED_IN:
      histogram =
          "Signin.AccountConsistencyPromoAction.SuppressedAlreadySignedIn";
      break;
    case AccountConsistencyPromoAction::IOS_AUTH_FLOW_CANCELLED_OR_FAILED:
      histogram = "Signin.AccountConsistencyPromoAction.SignInFailed";
      break;
    case AccountConsistencyPromoAction::
        ADD_ACCOUNT_STARTED_WITH_NO_DEVICE_ACCOUNT:
      histogram =
          "Signin.AccountConsistencyPromoAction."
          "AddAccountStartedWithNoDeviceAccount";
      break;
    case AccountConsistencyPromoAction::CONFIRM_MANAGEMENT_SHOWN:
      histogram = "Signin.AccountConsistencyPromoAction.ConfirmManagementShown";
      break;
    case AccountConsistencyPromoAction::CONFIRM_MANAGEMENT_ACCEPTED:
      histogram =
          "Signin.AccountConsistencyPromoAction.ConfirmManagementAccepted";
      break;
  }

  base::UmaHistogramEnumeration(histogram, access_point);
}
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

}  // namespace signin_metrics
