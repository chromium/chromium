// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_METRICS_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_METRICS_H_

#include <limits.h>

#include "base/time/time.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace signin_metrics {

// Enum for the ways in which primary account detection is done.
enum DifferentPrimaryAccounts {
  // token and cookie had same primary accounts.
  ACCOUNTS_SAME = 0,
  // Deprecated. Indicates different primary accounts.
  UNUSED_ACCOUNTS_DIFFERENT,
  // No GAIA cookie present, so the primaries are considered different.
  NO_COOKIE_PRESENT,
  // There was at least one cookie and one token, and the primaries differed.
  COOKIE_AND_TOKEN_PRIMARIES_DIFFERENT,
  NUM_DIFFERENT_PRIMARY_ACCOUNT_METRICS,
};

// Track all the ways a profile can become signed out as a histogram.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.signin
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: SignoutReason
enum ProfileSignout {
  // The value used within unit tests
  SIGNOUT_TEST = 0,
  // The preference or policy controlling if signin is valid has changed.
  SIGNOUT_PREF_CHANGED = 0,
  // The valid pattern for signing in to the Google service changed.
  GOOGLE_SERVICE_NAME_PATTERN_CHANGED,
  // The preference or policy controlling if signin is valid changed during
  // the signin process.
  SIGNIN_PREF_CHANGED_DURING_SIGNIN,
  // User clicked to signout from the settings page.
  USER_CLICKED_SIGNOUT_SETTINGS,
  // The signin process was aborted, but signin had succeeded, so signout. This
  // may be due to a server response, policy definition or user action.
  ABORT_SIGNIN,
  // The sync server caused the profile to be signed out.
  SERVER_FORCED_DISABLE,
  // The credentials are being transfered to a new profile, so the old one is
  // signed out.
  TRANSFER_CREDENTIALS,
  // Signed out because credentials are invalid and force-sign-in is enabled.
  AUTHENTICATION_FAILED_WITH_FORCE_SIGNIN,
  // The user disables sync from the DICE UI.
  USER_TUNED_OFF_SYNC_FROM_DICE_UI,
  // Android specific. Signout forced because the account was removed from the
  // device.
  ACCOUNT_REMOVED_FROM_DEVICE,
  // Keep this as the last enum.
  NUM_PROFILE_SIGNOUT_METRICS,
};

// Enum values used for use with "AutoLogin.Reverse" histograms.
enum AccessPointAction {
  // The infobar was shown to the user.
  HISTOGRAM_SHOWN,
  // The user pressed the accept button to perform the suggested action.
  HISTOGRAM_ACCEPTED,
  // The user pressed the reject to turn off the feature.
  HISTOGRAM_REJECTED,
  // The user pressed the X button to dismiss the infobar this time.
  HISTOGRAM_DISMISSED,
  // The user completely ignored the infobar.  Either they navigated away, or
  // they used the page as is.
  HISTOGRAM_IGNORED,
  // The user clicked on the learn more link in the infobar.
  HISTOGRAM_LEARN_MORE,
  // The sync was started with default settings.
  HISTOGRAM_WITH_DEFAULTS,
  // The sync was started with advanced settings.
  HISTOGRAM_WITH_ADVANCED,
  // The sync was started through auto-accept with default settings.
  HISTOGRAM_AUTO_WITH_DEFAULTS,
  // The sync was started through auto-accept with advanced settings.
  HISTOGRAM_AUTO_WITH_ADVANCED,
  // The sync was aborted with an undo button.
  HISTOGRAM_UNDO,
  HISTOGRAM_MAX
};

// Enum values used with the "Signin.OneClickConfirmation" histogram, which
// tracks the actions used in the OneClickConfirmation bubble.
enum ConfirmationUsage {
  HISTOGRAM_CONFIRM_SHOWN,
  HISTOGRAM_CONFIRM_OK,
  HISTOGRAM_CONFIRM_RETURN,
  HISTOGRAM_CONFIRM_ADVANCED,
  HISTOGRAM_CONFIRM_CLOSE,
  HISTOGRAM_CONFIRM_ESCAPE,
  HISTOGRAM_CONFIRM_UNDO,
  HISTOGRAM_CONFIRM_LEARN_MORE,
  HISTOGRAM_CONFIRM_LEARN_MORE_OK,
  HISTOGRAM_CONFIRM_LEARN_MORE_RETURN,
  HISTOGRAM_CONFIRM_LEARN_MORE_ADVANCED,
  HISTOGRAM_CONFIRM_LEARN_MORE_CLOSE,
  HISTOGRAM_CONFIRM_LEARN_MORE_ESCAPE,
  HISTOGRAM_CONFIRM_LEARN_MORE_UNDO,
  HISTOGRAM_CONFIRM_MAX
};

// TODO(gogerald): right now, gaia server needs to distinguish the source from
// signin_metrics::SOURCE_START_PAGE, signin_metrics::SOURCE_SETTINGS and the
// others to show advanced sync setting, remove them after switching to Minute
// Maid sign in flow.
// This was previously used in Signin.SigninSource UMA histogram, but no longer
// used after having below AccessPoint and Reason related histograms.
enum Source {
  SOURCE_START_PAGE = 0,  // This must be first.
  SOURCE_SETTINGS = 3,
  SOURCE_OTHERS = 13,
};

// Enum values which enumerates all access points where sign in could be
// initiated. Not all of them exist on all platforms. They are used with
// "Signin.SigninStartedAccessPoint" and "Signin.SigninCompletedAccessPoint"
// histograms.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.signin
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: SigninAccessPoint
enum class AccessPoint : int {
  ACCESS_POINT_START_PAGE = 0,
  ACCESS_POINT_NTP_LINK,
  ACCESS_POINT_MENU,
  ACCESS_POINT_SETTINGS,
  ACCESS_POINT_SUPERVISED_USER,
  ACCESS_POINT_EXTENSION_INSTALL_BUBBLE,
  ACCESS_POINT_EXTENSIONS,
  ACCESS_POINT_APPS_PAGE_LINK,
  ACCESS_POINT_BOOKMARK_BUBBLE,
  ACCESS_POINT_BOOKMARK_MANAGER,
  ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN,
  ACCESS_POINT_USER_MANAGER,
  ACCESS_POINT_DEVICES_PAGE,
  ACCESS_POINT_CLOUD_PRINT,
  ACCESS_POINT_CONTENT_AREA,
  ACCESS_POINT_SIGNIN_PROMO,
  ACCESS_POINT_RECENT_TABS,
  ACCESS_POINT_UNKNOWN,  // This should never have been used to get signin URL.
  ACCESS_POINT_PASSWORD_BUBBLE,
  ACCESS_POINT_AUTOFILL_DROPDOWN,
  ACCESS_POINT_NTP_CONTENT_SUGGESTIONS,
  ACCESS_POINT_RESIGNIN_INFOBAR,
  ACCESS_POINT_TAB_SWITCHER,
  ACCESS_POINT_FORCE_SIGNIN_WARNING,
  ACCESS_POINT_SAVE_CARD_BUBBLE,
  ACCESS_POINT_MANAGE_CARDS_BUBBLE,
  ACCESS_POINT_MAX,  // This must be last.
};

// Enum values which enumerates all user actions on the sign-in promo.
enum class PromoAction : int {
  PROMO_ACTION_NO_SIGNIN_PROMO = 0,
  // The user selected the default account.
  PROMO_ACTION_WITH_DEFAULT,
  // On desktop, the user selected an account that is not the default. On
  // mobile, the user selected the generic "Use another account" button.
  PROMO_ACTION_NOT_DEFAULT,
  // Non-personalized promo, pre-dice on desktop.
  PROMO_ACTION_NEW_ACCOUNT_PRE_DICE,
  // Non personalized promo, when there is no account on the device.
  PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT,
  // The user clicked on the "Add account" button, when there are already
  // accounts on the device. (desktop only, the button does not exist on
  // mobile).
  PROMO_ACTION_NEW_ACCOUNT_EXISTING_ACCOUNT
};

// Enum values which enumerates all reasons to start sign in process.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.signin
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: SigninReason
enum class Reason : int {
  REASON_SIGNIN_PRIMARY_ACCOUNT = 0,
  REASON_ADD_SECONDARY_ACCOUNT,
  REASON_REAUTHENTICATION,
  REASON_UNLOCK,
  REASON_UNKNOWN_REASON,  // This should never have been used to get signin URL.
  REASON_FORCED_SIGNIN_PRIMARY_ACCOUNT,
  REASON_MAX,  // This must be last.
};

// Enum values used for use with the "Signin.Reauth" histogram.
enum AccountReauth {
  // The user gave the wrong email when doing a reauthentication.
  HISTOGRAM_ACCOUNT_MISSMATCH,
  // The user was shown a reauthentication login screen.
  HISTOGRAM_REAUTH_SHOWN,

  HISTOGRAM_REAUTH_MAX
};

// Enum values used for "Signin.XDevicePromo.Eligible" histogram, which tracks
// the reasons for which a profile is or is not eligible for the promo.
enum CrossDevicePromoEligibility {
  // The user is eligible for the promo.
  ELIGIBLE,
  // The profile has previously opted out of the promo.
  OPTED_OUT,
  // The profile is already signed in.
  SIGNED_IN,
  // The profile does not have a single, peristent GAIA cookie.
  NOT_SINGLE_GAIA_ACCOUNT,
  // Yet to determine how many devices the user has.
  UNKNOWN_COUNT_DEVICES,
  // An error was returned trying to determine the account's devices.
  ERROR_FETCHING_DEVICE_ACTIVITY,
  // The call to get device activity was throttled, and never executed.
  THROTTLED_FETCHING_DEVICE_ACTIVITY,
  // The user has no devices.
  ZERO_DEVICES,
  // The user has no device that was recently active.
  NO_ACTIVE_DEVICES,
  // Always last enumerated type.
  NUM_CROSS_DEVICE_PROMO_ELIGIBILITY_METRICS
};

// Enum reasons the CrossDevicePromo couldn't initialize, or that it succeeded.
enum CrossDevicePromoInitialized {
  // The promo was initialized successfully.
  INITIALIZED,
  // The profile is opted out, so the promo didn't initialize.
  UNINITIALIZED_OPTED_OUT,
  // Unable to read the variations configuration.
  NO_VARIATIONS_CONFIG,
  // Always the last enumerated type.
  NUM_CROSS_DEVICE_PROMO_INITIALIZED_METRICS
};

// Enum values used for "Signin.AccountReconcilorState.OnGaiaResponse"
// histogram, which records the state of the AccountReconcilor when GAIA returns
// a specific response.
enum AccountReconcilorState {
  // The AccountReconcilor has finished running and is up to date.
  ACCOUNT_RECONCILOR_OK,
  // The AccountReconcilor is running and gathering information.
  ACCOUNT_RECONCILOR_RUNNING,
  // The AccountReconcilor encountered an error and stopped.
  ACCOUNT_RECONCILOR_ERROR,
  // Always the last enumerated type.
  ACCOUNT_RECONCILOR_HISTOGRAM_COUNT,
};

// Values of histogram comparing account id and email.
enum class AccountEquality : int {
  // Expected case when the user is not switching accounts.
  BOTH_EQUAL = 0,
  // Expected case when the user is switching accounts.
  BOTH_DIFFERENT,
  // The user has changed at least two email account names. This is actually
  // a different account, even though the email matches.
  ONLY_SAME_EMAIL,
  // The user has changed the email of their account, but the account is
  // actually the same.
  ONLY_SAME_ID,
  // The last account id was not present, email equality was used. This should
  // happen once to all old clients. Does not differentiate between same and
  // different accounts.
  EMAIL_FALLBACK,
  // Always the last enumerated type.
  HISTOGRAM_COUNT,
};

// When the user is give a choice of deleting their profile or not when signing
// out, the |DELETED| or |KEEPING| metric should be used. If the user is not
// given any option, then use the |IGNORE_METRIC| value should be used.
enum class SignoutDelete : int {
  DELETED = 0,
  KEEPING,
  IGNORE_METRIC,
};

// This is the relationship between the account used to sign into chrome, and
// the account(s) used to sign into the content area/cookie jar. This enum
// gets messy because we're trying to capture quite a few things, if there was
// a match or not, how many accounts were in the cookie jar, and what state
// those cookie jar accounts were in (signed out vs signed in). Note that it's
// not possible to have the same account multiple times in the cookie jar.
enum class AccountRelation : int {
  // No signed in or out accounts in the content area. Cannot have a match.
  EMPTY_COOKIE_JAR = 0,
  // The cookie jar contains only a single signed out account that matches.
  NO_SIGNED_IN_SINGLE_SIGNED_OUT_MATCH,
  // The cookie jar contains only signed out accounts, one of which matches.
  NO_SIGNED_IN_ONE_OF_SIGNED_OUT_MATCH,
  // The cookie jar contains one or more signed out accounts, none match.
  NO_SIGNED_IN_WITH_SIGNED_OUT_NO_MATCH,
  // The cookie jar contains only a single signed in account that matches.
  SINGLE_SIGNED_IN_MATCH_NO_SIGNED_OUT,
  // There's only one signed in account which matches, and there are one or
  // more signed out accounts.
  SINGLE_SINGED_IN_MATCH_WITH_SIGNED_OUT,
  // There's more than one signed in account, one of which matches. There
  // could be any configuration of signed out accounts.
  ONE_OF_SIGNED_IN_MATCH_ANY_SIGNED_OUT,
  // There's one or more signed in accounts, none of which match. However
  // there is a match in the signed out accounts.
  WITH_SIGNED_IN_ONE_OF_SIGNED_OUT_MATCH,
  // There's one or more signed in accounts and any configuration of signed
  // out accounts. However, none of the accounts match.
  WITH_SIGNED_IN_NO_MATCH,
  // Always the last enumerated type.
  HISTOGRAM_COUNT,
};

// Different types of reporting. This is used as a histogram suffix.
enum class ReportingType { PERIODIC, ON_CHANGE };

// -----------------------------------------------------------------------------
// Histograms
// -----------------------------------------------------------------------------

// Tracks the access point of sign in.
void LogSigninAccessPointStarted(AccessPoint access_point,
                                 PromoAction promo_action);
void LogSigninAccessPointCompleted(AccessPoint access_point,
                                   PromoAction promo_action);

// Tracks the reason of sign in.
void LogSigninReason(Reason reason);

// Log to UMA histograms and UserCounts stats about a single execution of the
// AccountReconciler.
// |total_number_accounts| - How many accounts are in the browser for this
//                           profile.
// |count_added_to_cookie_jar| - How many accounts were in the browser but not
//                               in the cookie jar.
// |count_removed_from_cookie_jar| - How many accounts were in the cookie jar
//                                   but not in the browser.
// |primary_accounts_same| - False if the primary account for the cookie jar
//                           and the token service were different; else true.
// |is_first_reconcile| - True if these stats are from the first execution of
//                        the AccountReconcilor.
// |pre_count_gaia_cookies| - How many GAIA cookies were present before
//                            the AccountReconcilor began modifying the state.
void LogSigninAccountReconciliation(int total_number_accounts,
                                    int count_added_to_cookie_jar,
                                    int count_removed_from_cookie_jar,
                                    bool primary_accounts_same,
                                    bool is_first_reconcile,
                                    int pre_count_gaia_cookies);

// Logs to UMA histograms how many accounts are in the browser for this
// profile.
void RecordAccountsPerProfile(int total_number_accounts);

// Logs duration of a single execution of AccountReconciler to UMA histograms.
// |duration| - How long execution of AccountReconciler took.
// |successful| - True if AccountReconciler was successful.
void LogSigninAccountReconciliationDuration(base::TimeDelta duration,
                                            bool successful);

// Track a successful signin.
void LogSigninAddAccount();

// Track a successful signin of a profile.
void LogSigninProfile(bool is_first_run, base::Time install_date);

// Track a profile signout.
void LogSignout(ProfileSignout source_metric, SignoutDelete delete_metric);

// Tracks whether the external connection results were all fetched before
// the gaia cookie manager service tried to use them with merge session.
// |time_to_check_connections| is the time it took to complete.
void LogExternalCcResultFetches(
    bool fetches_completed,
    const base::TimeDelta& time_to_check_connections);

// Track when the current authentication error changed.
void LogAuthError(const GoogleServiceAuthError& auth_error);

void LogSigninConfirmHistogramValue(ConfirmationUsage action);

void LogXDevicePromoEligible(CrossDevicePromoEligibility metric);

void LogXDevicePromoInitialized(CrossDevicePromoInitialized metric);

void LogBrowsingSessionDuration(const base::Time& previous_activity_time);

// Records the AccountReconcilor |state| when GAIA returns a specific response.
// If |state| is different than ACCOUNT_RECONCILOR_OK it means the user will
// be shown a different set of accounts in the content-area and the settings UI.
void LogAccountReconcilorStateOnGaiaResponse(AccountReconcilorState state);

// Records the AccountEquality metric when an investigator compares the current
// and previous id/emails during a signin.
void LogAccountEquality(AccountEquality equality);

// Records the amount of time since the cookie jar was last changed.
void LogCookieJarStableAge(const base::TimeDelta stable_age,
                           const ReportingType type);

// Records three counts for the number of accounts in the cookei jar.
void LogCookieJarCounts(const int signed_in,
                        const int signed_out,
                        const int total,
                        const ReportingType type);

// Records the relation between the account signed into Chrome, and the
// account(s) present in the cookie jar.
void LogAccountRelation(const AccountRelation relation,
                        const ReportingType type);

// Records if the best guess is that this profile is currently shared or not
// between multiple users.
void LogIsShared(const bool is_shared, const ReportingType type);

// -----------------------------------------------------------------------------
// User actions
// -----------------------------------------------------------------------------

// Records corresponding sign in user action for an access point.
void RecordSigninUserActionForAccessPoint(AccessPoint access_point,
                                          PromoAction promo_action);

// Records |Signin_ImpressionWithAccount_From*| user action.
void RecordSigninImpressionUserActionForAccessPoint(AccessPoint access_point);

// Records |Signin_Impression{With|No}Account_From*| user action.
void RecordSigninImpressionWithAccountUserActionForAccessPoint(
    AccessPoint access_point,
    bool with_account);

}  // namespace signin_metrics

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_METRICS_H_
