// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_metrics.h"

#include <limits.h>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace signin_metrics {

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
  UMA_HISTOGRAM_ENUMERATION("Signin.SigninStartedAccessPoint",
                            static_cast<int>(access_point),
                            static_cast<int>(AccessPoint::ACCESS_POINT_MAX));
  switch (promo_action) {
    case PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO:
      break;
    case PromoAction::PROMO_ACTION_WITH_DEFAULT:
      UMA_HISTOGRAM_ENUMERATION(
          "Signin.SigninStartedAccessPoint.WithDefault",
          static_cast<int>(access_point),
          static_cast<int>(AccessPoint::ACCESS_POINT_MAX));
      break;
    case PromoAction::PROMO_ACTION_NOT_DEFAULT:
      UMA_HISTOGRAM_ENUMERATION(
          "Signin.SigninStartedAccessPoint.NotDefault",
          static_cast<int>(access_point),
          static_cast<int>(AccessPoint::ACCESS_POINT_MAX));
      break;
    case PromoAction::PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT:
      UMA_HISTOGRAM_ENUMERATION(
          "Signin.SigninStartedAccessPoint.NewAccountNoExistingAccount",
          static_cast<int>(access_point),
          static_cast<int>(AccessPoint::ACCESS_POINT_MAX));
      break;
    case PromoAction::PROMO_ACTION_NEW_ACCOUNT_EXISTING_ACCOUNT:
      UMA_HISTOGRAM_ENUMERATION(
          "Signin.SigninStartedAccessPoint.NewAccountExistingAccount",
          static_cast<int>(access_point),
          static_cast<int>(AccessPoint::ACCESS_POINT_MAX));
      break;
  }
}

void LogSigninAccessPointCompleted(AccessPoint access_point,
                                   PromoAction promo_action) {
  UMA_HISTOGRAM_ENUMERATION("Signin.SigninCompletedAccessPoint",
                            static_cast<int>(access_point),
                            static_cast<int>(AccessPoint::ACCESS_POINT_MAX));
  switch (promo_action) {
    case PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO:
      break;
    case PromoAction::PROMO_ACTION_WITH_DEFAULT:
      UMA_HISTOGRAM_ENUMERATION(
          "Signin.SigninCompletedAccessPoint.WithDefault",
          static_cast<int>(access_point),
          static_cast<int>(AccessPoint::ACCESS_POINT_MAX));
      break;
    case PromoAction::PROMO_ACTION_NOT_DEFAULT:
      UMA_HISTOGRAM_ENUMERATION(
          "Signin.SigninCompletedAccessPoint.NotDefault",
          static_cast<int>(access_point),
          static_cast<int>(AccessPoint::ACCESS_POINT_MAX));
      break;
    case PromoAction::PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT:
      UMA_HISTOGRAM_ENUMERATION(
          "Signin.SigninCompletedAccessPoint.NewAccountNoExistingAccount",
          static_cast<int>(access_point),
          static_cast<int>(AccessPoint::ACCESS_POINT_MAX));
      break;
    case PromoAction::PROMO_ACTION_NEW_ACCOUNT_EXISTING_ACCOUNT:
      UMA_HISTOGRAM_ENUMERATION(
          "Signin.SigninCompletedAccessPoint.NewAccountExistingAccount",
          static_cast<int>(access_point),
          static_cast<int>(AccessPoint::ACCESS_POINT_MAX));
      break;
  }
}

void LogSignInOffered(AccessPoint access_point) {
  base::UmaHistogramEnumeration("Signin.SignIn.Offered", access_point,
                                AccessPoint::ACCESS_POINT_MAX);
}

void LogSignInStarted(AccessPoint access_point) {
  base::UmaHistogramEnumeration("Signin.SignIn.Started", access_point,
                                AccessPoint::ACCESS_POINT_MAX);
}

void LogSyncOptInStarted(AccessPoint access_point) {
  base::UmaHistogramEnumeration("Signin.SyncOptIn.Started", access_point,
                                AccessPoint::ACCESS_POINT_MAX);
}

void LogSyncSettingsOpened(AccessPoint access_point) {
  base::UmaHistogramEnumeration("Signin.SyncOptIn.OpenedSyncSettings",
                                access_point, AccessPoint::ACCESS_POINT_MAX);
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
      "Signin.CookieJar.ChromeAccountRelation", type,
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

void RecordSignoutForceClearDataChoice(bool force_clear_data) {
  base::UmaHistogramBoolean("Signin.UserRequestedWipeDataOnSignout",
                            force_clear_data);
}
#endif  // BUILDFLAG(IS_IOS)

void RecordOpenTabCountOnSignin(signin_metrics::AccessPoint access_point,
                                signin::ConsentLevel consent_level,
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
        access_point, AccessPoint::ACCESS_POINT_MAX);
  }
}

// --------------------------------------------------------------
// User actions
// --------------------------------------------------------------

void RecordSigninUserActionForAccessPoint(AccessPoint access_point) {
  switch (access_point) {
    case AccessPoint::ACCESS_POINT_START_PAGE:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromStartPage"));
      break;
    case AccessPoint::ACCESS_POINT_NTP_LINK:
      base::RecordAction(base::UserMetricsAction("Signin_Signin_FromNTP"));
      break;
    case AccessPoint::ACCESS_POINT_MENU:
      base::RecordAction(base::UserMetricsAction("Signin_Signin_FromMenu"));
      break;
    case AccessPoint::ACCESS_POINT_SETTINGS:
      base::RecordAction(base::UserMetricsAction("Signin_Signin_FromSettings"));
      break;
    case AccessPoint::ACCESS_POINT_SUPERVISED_USER:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromSupervisedUser"));
      break;
    case AccessPoint::ACCESS_POINT_EXTENSION_INSTALL_BUBBLE:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromExtensionInstallBubble"));
      break;
    case AccessPoint::ACCESS_POINT_EXTENSIONS:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromExtensions"));
      break;
    case AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromBookmarkBubble"));
      break;
    case AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromBookmarkManager"));
      break;
    case AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromAvatarBubbleSignin"));
      break;
    case AccessPoint::ACCESS_POINT_USER_MANAGER:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromUserManager"));
      break;
    case AccessPoint::ACCESS_POINT_DEVICES_PAGE:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromDevicesPage"));
      break;
    case AccessPoint::ACCESS_POINT_SIGNIN_PROMO:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromSigninPromo"));
      break;
    case AccessPoint::ACCESS_POINT_RECENT_TABS:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromRecentTabs"));
      break;
    case AccessPoint::ACCESS_POINT_UNKNOWN:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromUnknownAccessPoint"));
      break;
    case AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromPasswordBubble"));
      break;
    case AccessPoint::ACCESS_POINT_AUTOFILL_DROPDOWN:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromAutofillDropdown"));
      break;
    case AccessPoint::ACCESS_POINT_RESIGNIN_INFOBAR:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromReSigninInfobar"));
      break;
    case AccessPoint::ACCESS_POINT_TAB_SWITCHER:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromTabSwitcher"));
      break;
    case AccessPoint::ACCESS_POINT_MACHINE_LOGON:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromMachineLogon"));
      break;
    case AccessPoint::ACCESS_POINT_GOOGLE_SERVICES_SETTINGS:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromGoogleServicesSettings"));
      break;
    case AccessPoint::ACCESS_POINT_ENTERPRISE_SIGNOUT_COORDINATOR:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromEnterpriseSignoutSheet"));
      break;
    case AccessPoint::ACCESS_POINT_SIGNIN_INTERCEPT_FIRST_RUN_EXPERIENCE:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Signin_FromSigninInterceptFirstRunExperience"));
      break;
    case AccessPoint::ACCESS_POINT_NTP_FEED_TOP_PROMO:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromNTPFeedTopPromo"));
      break;
    case AccessPoint::ACCESS_POINT_KALEIDOSCOPE:
      NOTREACHED_IN_MIGRATION()
          << "Access point " << static_cast<int>(access_point)
          << " is only used to trigger non-sync sign-in and this"
          << " action should only be triggered for sync-enabled"
          << " sign-ins.";
      break;
    case AccessPoint::ACCESS_POINT_SYNC_ERROR_CARD:
    case AccessPoint::ACCESS_POINT_FORCED_SIGNIN:
    case AccessPoint::ACCESS_POINT_ACCOUNT_RENAMED:
    case AccessPoint::ACCESS_POINT_WEB_SIGNIN:
    case AccessPoint::ACCESS_POINT_SAVE_TO_DRIVE_IOS:
    case AccessPoint::ACCESS_POINT_SAVE_TO_PHOTOS_IOS:
    case AccessPoint::ACCESS_POINT_SETTINGS_SYNC_OFF_ROW:
    case AccessPoint::ACCESS_POINT_POST_DEVICE_RESTORE_BACKGROUND_SIGNIN:
    case AccessPoint::ACCESS_POINT_RESTORE_PRIMARY_ACCOUNT_ON_PROFILE_LOAD:
    case AccessPoint::ACCESS_POINT_DESKTOP_SIGNIN_MANAGER:
    case AccessPoint::ACCESS_POINT_SIGNIN_CHOICE_REMEMBERED:
    case AccessPoint::ACCESS_POINT_PROFILE_MENU_SIGNOUT_CONFIRMATION_PROMPT:
    case AccessPoint::ACCESS_POINT_SETTINGS_SIGNOUT_CONFIRMATION_PROMPT:
    case AccessPoint::ACCESS_POINT_WEBAUTHN_MODAL_DIALOG:
    case AccessPoint::ACCESS_POINT_CCT_ACCOUNT_MISMATCH_NOTIFICATION:
      NOTREACHED_IN_MIGRATION()
          << "Access point " << static_cast<int>(access_point)
          << " is not supposed to log signin user actions.";
      break;
    case AccessPoint::ACCESS_POINT_SAFETY_CHECK:
      VLOG(1) << "Signin_Signin_From* user action is not recorded "
              << "for access point " << static_cast<int>(access_point);
      break;
    case AccessPoint::ACCESS_POINT_SEND_TAB_TO_SELF_PROMO:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromSendTabToSelfPromo"));
      break;
    case AccessPoint::ACCESS_POINT_POST_DEVICE_RESTORE_SIGNIN_PROMO:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Signin_FromPostDeviceRestoreSigninPromo"));
      break;
    case AccessPoint::ACCESS_POINT_NTP_SIGNED_OUT_ICON:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromNTPSignedOutIcon"));
      break;
    case AccessPoint::ACCESS_POINT_NTP_FEED_CARD_MENU_PROMO:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Signin_FromNTPFeedCardMenuSigninPromo"));
      break;
    case AccessPoint::ACCESS_POINT_NTP_FEED_BOTTOM_PROMO:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Signin_FromNTPFeedBottomSigninPromo"));
      break;
    case AccessPoint::ACCESS_POINT_FOR_YOU_FRE:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromForYouFre"));
      break;
    case AccessPoint::ACCESS_POINT_CREATOR_FEED_FOLLOW:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromCreatorFeedFollow"));
      break;
    case AccessPoint::ACCESS_POINT_READING_LIST:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromReadingList"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_REAUTH_INFO_BAR:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromReauthInfoBar"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_ACCOUNT_CONSISTENCY_SERVICE:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Signin_FromAccountConsistencyService"));
      break;
    case AccessPoint::ACCESS_POINT_SEARCH_COMPANION:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromSearchCompanion"));
      break;
    case AccessPoint::ACCESS_POINT_SET_UP_LIST:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromSetUpList"));
      break;
    case AccessPoint::ACCESS_POINT_PASSWORD_MIGRATION_WARNING_ANDROID:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Signin_FromPasswordMigrationWarningAndroid"));
      break;
    case AccessPoint::ACCESS_POINT_CHROME_SIGNIN_INTERCEPT_BUBBLE:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Signin_FromChromeSigninInterceptBubble"));
      break;
    case AccessPoint::ACCESS_POINT_TAB_ORGANIZATION:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromTabOrganization"));
      break;
    case AccessPoint::ACCESS_POINT_TIPS_NOTIFICATION:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromTipsNotification"));
      break;
    case AccessPoint::ACCESS_POINT_NOTIFICATIONS_OPT_IN_SCREEN_CONTENT_TOGGLE:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Signin_FromNotificationsOptInScreenContentToggle"));
      break;
    case AccessPoint::ACCESS_POINT_NTP_IDENTITY_DISC:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromNtpIdentityDisc"));
      break;
    case AccessPoint::ACCESS_POINT_OIDC_REDIRECTION_INTERCEPTION:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Signin_FromOidcRedirectionInterception"));
      break;
    case AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN_WITH_SYNC_PROMO:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Signin_FromAvatarBubbleSigninWithSyncPromo"));
      break;
    case AccessPoint::ACCESS_POINT_ACCOUNT_MENU:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromAccountMenu"));
      break;
    case AccessPoint::ACCESS_POINT_PRODUCT_SPECIFICATIONS:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromProductSpecifications"));
      break;
    case AccessPoint::ACCESS_POINT_ACCOUNT_MENU_FAILED_SWITCH:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromAccountMenuFailedSwitch"));
      break;
    case AccessPoint::ACCESS_POINT_ADDRESS_BUBBLE:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromAddressBubble"));
      break;
    case AccessPoint::ACCESS_POINT_MAX:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void RecordSignoutUserAction(bool force_clear_data) {
  if (force_clear_data) {
    base::RecordAction(base::UserMetricsAction("Signin_SignoutClearData"));
  } else {
    base::RecordAction(base::UserMetricsAction("Signin_Signout"));
  }
}

void RecordSigninImpressionUserActionForAccessPoint(AccessPoint access_point) {
  switch (access_point) {
    case AccessPoint::ACCESS_POINT_START_PAGE:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromStartPage"));
      break;
    case AccessPoint::ACCESS_POINT_NTP_LINK:
      base::RecordAction(base::UserMetricsAction("Signin_Impression_FromNTP"));
      break;
    case AccessPoint::ACCESS_POINT_MENU:
      base::RecordAction(base::UserMetricsAction("Signin_Impression_FromMenu"));
      break;
    case AccessPoint::ACCESS_POINT_SETTINGS:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromSettings"));
      break;
    case AccessPoint::ACCESS_POINT_EXTENSION_INSTALL_BUBBLE:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Impression_FromExtensionInstallBubble"));
      break;
    case AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromBookmarkBubble"));
      break;
    case AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromBookmarkManager"));
      break;
    case AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromAvatarBubbleSignin"));
      break;
    case AccessPoint::ACCESS_POINT_DEVICES_PAGE:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromDevicesPage"));
      break;
    case AccessPoint::ACCESS_POINT_SIGNIN_PROMO:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromSigninPromo"));
      break;
    case AccessPoint::ACCESS_POINT_RECENT_TABS:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromRecentTabs"));
      break;
    case AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromPasswordBubble"));
      break;
    case AccessPoint::ACCESS_POINT_AUTOFILL_DROPDOWN:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromAutofillDropdown"));
      break;
    case AccessPoint::ACCESS_POINT_RESIGNIN_INFOBAR:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromReSigninInfobar"));
      break;
    case AccessPoint::ACCESS_POINT_TAB_SWITCHER:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromTabSwitcher"));
      break;
    case AccessPoint::ACCESS_POINT_GOOGLE_SERVICES_SETTINGS:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Impression_FromGoogleServicesSettings"));
      break;
    case AccessPoint::ACCESS_POINT_KALEIDOSCOPE:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromKaleidoscope"));
      break;
    case AccessPoint::ACCESS_POINT_USER_MANAGER:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromUserManager"));
      break;
    case AccessPoint::ACCESS_POINT_SEND_TAB_TO_SELF_PROMO:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromSendTabToSelfPromo"));
      break;
    case AccessPoint::ACCESS_POINT_NTP_FEED_TOP_PROMO:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromNTPFeedTopPromo"));
      break;
    case AccessPoint::ACCESS_POINT_POST_DEVICE_RESTORE_SIGNIN_PROMO:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Impression_FromPostDeviceRestoreSigninPromo"));
      break;
    case AccessPoint::ACCESS_POINT_NTP_FEED_CARD_MENU_PROMO:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Impression_FromNTPFeedCardMenuSigninPromo"));
      break;
    case AccessPoint::ACCESS_POINT_NTP_FEED_BOTTOM_PROMO:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Impression_FromNTPFeedBottomSigninPromo"));
      break;
    case AccessPoint::ACCESS_POINT_CREATOR_FEED_FOLLOW:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromCreatorFeedFollow"));
      break;
    case AccessPoint::ACCESS_POINT_READING_LIST:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromReadingList"));
      break;
    case AccessPoint::ACCESS_POINT_SEARCH_COMPANION:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromSearchCompanion"));
      break;
    case AccessPoint::ACCESS_POINT_SET_UP_LIST:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromSetUpList"));
      break;
    case AccessPoint::ACCESS_POINT_CHROME_SIGNIN_INTERCEPT_BUBBLE:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Impression_FromChromeSigninInterceptBubble"));
      break;
    case AccessPoint::ACCESS_POINT_TIPS_NOTIFICATION:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromTipsNotification"));
      break;
    case AccessPoint::ACCESS_POINT_NOTIFICATIONS_OPT_IN_SCREEN_CONTENT_TOGGLE:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Impression_FromNotificationsOptInScreenContentToggle"));
      break;
    case AccessPoint::ACCESS_POINT_PRODUCT_SPECIFICATIONS:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Impression_FromProductSpecifications"));
      break;
    case AccessPoint::ACCESS_POINT_ADDRESS_BUBBLE:
      base::RecordAction(
          base::UserMetricsAction("Signin_Impression_FromAddressBubble"));
      break;
    case AccessPoint::ACCESS_POINT_ENTERPRISE_SIGNOUT_COORDINATOR:
    case AccessPoint::ACCESS_POINT_EXTENSIONS:
    case AccessPoint::ACCESS_POINT_SUPERVISED_USER:
    case AccessPoint::ACCESS_POINT_UNKNOWN:
    case AccessPoint::ACCESS_POINT_MACHINE_LOGON:
    case AccessPoint::ACCESS_POINT_SYNC_ERROR_CARD:
    case AccessPoint::ACCESS_POINT_FORCED_SIGNIN:
    case AccessPoint::ACCESS_POINT_ACCOUNT_RENAMED:
    case AccessPoint::ACCESS_POINT_WEB_SIGNIN:
    case AccessPoint::ACCESS_POINT_SIGNIN_CHOICE_REMEMBERED:
    case AccessPoint::ACCESS_POINT_SAFETY_CHECK:
    case AccessPoint::ACCESS_POINT_SIGNIN_INTERCEPT_FIRST_RUN_EXPERIENCE:
    case AccessPoint::ACCESS_POINT_SETTINGS_SYNC_OFF_ROW:
    case AccessPoint::ACCESS_POINT_POST_DEVICE_RESTORE_BACKGROUND_SIGNIN:
    case AccessPoint::ACCESS_POINT_NTP_SIGNED_OUT_ICON:
    case AccessPoint::ACCESS_POINT_DESKTOP_SIGNIN_MANAGER:
    case AccessPoint::ACCESS_POINT_FOR_YOU_FRE:
    case AccessPoint::ACCESS_POINT_SAVE_TO_DRIVE_IOS:
    case AccessPoint::ACCESS_POINT_SAVE_TO_PHOTOS_IOS:
    case AccessPoint::ACCESS_POINT_REAUTH_INFO_BAR:
    case AccessPoint::ACCESS_POINT_ACCOUNT_CONSISTENCY_SERVICE:
    case AccessPoint::ACCESS_POINT_PASSWORD_MIGRATION_WARNING_ANDROID:
    case AccessPoint::ACCESS_POINT_RESTORE_PRIMARY_ACCOUNT_ON_PROFILE_LOAD:
    case AccessPoint::ACCESS_POINT_TAB_ORGANIZATION:
    case AccessPoint::ACCESS_POINT_PROFILE_MENU_SIGNOUT_CONFIRMATION_PROMPT:
    case AccessPoint::ACCESS_POINT_SETTINGS_SIGNOUT_CONFIRMATION_PROMPT:
    case AccessPoint::ACCESS_POINT_NTP_IDENTITY_DISC:
    case AccessPoint::ACCESS_POINT_OIDC_REDIRECTION_INTERCEPTION:
    case AccessPoint::ACCESS_POINT_WEBAUTHN_MODAL_DIALOG:
    case AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN_WITH_SYNC_PROMO:
    case AccessPoint::ACCESS_POINT_ACCOUNT_MENU:
    case AccessPoint::ACCESS_POINT_ACCOUNT_MENU_FAILED_SWITCH:
    case AccessPoint::ACCESS_POINT_CCT_ACCOUNT_MISMATCH_NOTIFICATION:
    case AccessPoint::ACCESS_POINT_MAX:
      NOTREACHED_IN_MIGRATION() << "Signin_Impression_From* user actions"
                                << " are not recorded for access point "
                                << static_cast<int>(access_point);
      break;
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

  base::UmaHistogramEnumeration(histogram, access_point,
                                AccessPoint::ACCESS_POINT_MAX);
}
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

}  // namespace signin_metrics
