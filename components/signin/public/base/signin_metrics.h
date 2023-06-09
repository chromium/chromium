// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_METRICS_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_METRICS_H_

#include <limits.h>

#include "base/time/time.h"
#include "build/build_config.h"
#include "components/signin/public/base/consent_level.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace signin_metrics {

// Track all the ways a profile can become signed out as a histogram.
// Enum SigninSignoutProfile.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.signin.metrics
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: SignoutReason
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ProfileSignout {
  // The value used within unit tests.
  kTest = 0,
  // The preference or policy controlling if signin is valid has changed.
  kPrefChanged = 0,
  // The valid pattern for signing in to the Google service changed.
  kGoogleServiceNamePatternChanged = 1,
  // The preference or policy controlling if signin is valid changed during
  // the signin process.
  // Deprecated: kSigninPrefChangedDuringSignin = 2,
  // User clicked to signout from the settings page.
  kUserClickedSignoutSettings = 3,
  // The signin process was aborted, but signin had succeeded, so signout. This
  // may be due to a server response, policy definition or user action.
  kAbortSignin = 4,
  // The sync server caused the profile to be signed out.
  kServerForcedDisable = 5,
  // The credentials are being transferred to a new profile, so the old one is
  // signed out.
  // Deprecated: kTransferCredentials = 6,
  // Signed out because credentials are invalid and force-sign-in is enabled.
  kAuthenticationFailedWithForceSignin = 7,
  // The user disables sync from the DICE UI.
  // Deprecated: USER_TUNED_OFF_SYNC_FROM_DICE_UI = 8,
  // Signout forced because the account was removed from the device.
  kAccountRemovedFromDevice = 9,
  // Signin is no longer allowed when the profile is initialized.
  kSigninNotAllowedOnProfileInit = 10,
  // Sign out is forced allowed. Only used for tests.
  kForceSignoutAlwaysAllowedForTest = 11,
  // User cleared account cookies when there's no sync consent, which has caused
  // sign out.
  // Deprecated (re-numbered in M114): kUserDeletedAccountCookies = 12,
  // Signout triggered by MobileIdentityConsistency rollback.
  // Deprecated: kMobileIdentityConsistencyRollback = 13,
  // Sign-out when the account id migration to Gaia ID did not finish,
  // Deprecated: kAccountIdMigration = 14,
  // iOS Specific. Sign-out forced because the account was removed from the
  // device after a device restore.
  kIosAccountRemovedFromDeviceAfterRestore = 15,
  // User clicked to 'Turn off sync' from the settings page.
  // Currently only available for Android Unicorn users.
  kUserClickedRevokeSyncConsentSettings = 16,
  // User clicked to signout from the settings page.
  kUserClickedSignoutProfileMenu = 17,
  // User retriggered signin from the Android web sign-in bottomsheet.
  kSigninRetriggeredFromWebSignin = 18,
  // User clicked on sign-out from the notification dialog for User Policy. The
  // notification informs the user that from now on user policies may be
  // effective on their browser if they Sync with their managed account. The
  // user has the option to sign out to avoid user policies.
  kUserClickedSignoutFromUserPolicyNotificationDialog = 19,
  // The email address of the primary account on the device was updated,
  // triggering an automatic signout followed by signin.
  kAccountEmailUpdated = 20,
  // User clicked on sign-out from the clear browsing data page.
  kUserClickedSignoutFromClearBrowsingDataPage = 21,
  // Profile Signout during reconciliation triggered by a Gaia cookie update.
  kGaiaCookieUpdated = 22,
  // Profile Signout during reconciliation.
  kAccountReconcilorReconcile = 23,
  // Signin manager updates the unconsented primary account.
  kSigninManagerUpdateUPA = 24,
  // User cleared account cookies when there's no sync consent, which has caused
  // sign out.
  kUserDeletedAccountCookies = 25,
  // User tapped 'Undo' in a snackbar that is shown right after sign-in through
  // promo in bookmarks and reading list page. (iOS only).
  kUserTappedUndoRightAfterSignIn = 26,

  // Keep this as the last enum.
  kMaxValue = kUserTappedUndoRightAfterSignIn
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

// Enum values which enumerates all access points where sign in could be
// initiated. Not all of them exist on all platforms. They are used with
// "Signin.SigninStartedAccessPoint" and "Signin.SigninCompletedAccessPoint"
// histograms.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.signin.metrics
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: SigninAccessPoint
enum class AccessPoint : int {
  ACCESS_POINT_START_PAGE = 0,
  ACCESS_POINT_NTP_LINK = 1,
  ACCESS_POINT_MENU = 2,
  ACCESS_POINT_SETTINGS = 3,
  ACCESS_POINT_SUPERVISED_USER = 4,
  ACCESS_POINT_EXTENSION_INSTALL_BUBBLE = 5,
  ACCESS_POINT_EXTENSIONS = 6,
  // ACCESS_POINT_APPS_PAGE_LINK = 7, no longer used.
  ACCESS_POINT_BOOKMARK_BUBBLE = 8,
  ACCESS_POINT_BOOKMARK_MANAGER = 9,
  ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN = 10,
  ACCESS_POINT_USER_MANAGER = 11,
  ACCESS_POINT_DEVICES_PAGE = 12,
  ACCESS_POINT_CLOUD_PRINT = 13,
  ACCESS_POINT_CONTENT_AREA = 14,
  ACCESS_POINT_SIGNIN_PROMO = 15,
  ACCESS_POINT_RECENT_TABS = 16,
  // This should never have been used to get signin URL.
  ACCESS_POINT_UNKNOWN = 17,
  ACCESS_POINT_PASSWORD_BUBBLE = 18,
  ACCESS_POINT_AUTOFILL_DROPDOWN = 19,
  ACCESS_POINT_NTP_CONTENT_SUGGESTIONS = 20,
  ACCESS_POINT_RESIGNIN_INFOBAR = 21,
  ACCESS_POINT_TAB_SWITCHER = 22,
  // ACCESS_POINT_FORCE_SIGNIN_WARNING = 23, no longer used.
  // ACCESS_POINT_SAVE_CARD_BUBBLE = 24, no longer used
  // ACCESS_POINT_MANAGE_CARDS_BUBBLE = 25, no longer used
  ACCESS_POINT_MACHINE_LOGON = 26,
  ACCESS_POINT_GOOGLE_SERVICES_SETTINGS = 27,
  ACCESS_POINT_SYNC_ERROR_CARD = 28,
  ACCESS_POINT_FORCED_SIGNIN = 29,
  ACCESS_POINT_ACCOUNT_RENAMED = 30,
  ACCESS_POINT_WEB_SIGNIN = 31,
  ACCESS_POINT_SAFETY_CHECK = 32,
  ACCESS_POINT_KALEIDOSCOPE = 33,
  ACCESS_POINT_ENTERPRISE_SIGNOUT_COORDINATOR = 34,
  ACCESS_POINT_SIGNIN_INTERCEPT_FIRST_RUN_EXPERIENCE = 35,
  ACCESS_POINT_SEND_TAB_TO_SELF_PROMO = 36,
  ACCESS_POINT_NTP_FEED_TOP_PROMO = 37,
  ACCESS_POINT_SETTINGS_SYNC_OFF_ROW = 38,
  ACCESS_POINT_POST_DEVICE_RESTORE_SIGNIN_PROMO = 39,
  ACCESS_POINT_POST_DEVICE_RESTORE_BACKGROUND_SIGNIN = 40,
  ACCESS_POINT_NTP_SIGNED_OUT_ICON = 41,
  ACCESS_POINT_NTP_FEED_CARD_MENU_PROMO = 42,
  ACCESS_POINT_NTP_FEED_BOTTOM_PROMO = 43,
  // TODO(crbug.com/1261772): Not a real access point, as this is an internal
  // component. We should replace its usage with actual access points once we
  // find ways to attribute the changes accurately.
  ACCESS_POINT_DESKTOP_SIGNIN_MANAGER = 44,
  // Access point for the "For You" First Run Experience on Desktop. See
  // go/for-you-fre or launch/4223982 for more info.
  ACCESS_POINT_FOR_YOU_FRE = 45,
  // Access point for Cormorant (Creator Feed) on Android only when the "Follow"
  // button is tapped while in a signed-out state.
  ACCESS_POINT_CREATOR_FEED_FOLLOW = 46,
  // Access point for the reading list sign-in promo (launch/4231282).
  ACCESS_POINT_READING_LIST = 47,
  // Access point for the reauth info bar.
  ACCESS_POINT_REAUTH_INFO_BAR = 48,
  // Access point for the consistency service.
  ACCESS_POINT_ACCOUNT_CONSISTENCY_SERVICE = 49,
  // Access point for the search companion sign-in promo.
  ACCESS_POINT_SEARCH_COMPANION = 50,
  // Access point for the IOS Set Up List on the NTP.
  ACCESS_POINT_SET_UP_LIST = 51,

  // Add values above this line with a corresponding label to the
  // "SigninAccessPoint" enum in tools/metrics/histograms/enums.xml
  ACCESS_POINT_MAX,  // This must be last.
};

// Enum values which enumerates all access points where transactional reauth
// could be initiated. Transactional reauth is used when the user already has
// a valid refresh token but a system still wants to verify user's identity.
enum class ReauthAccessPoint {
  // The code expects kUnknown to be the first, so it should not be reordered.
  kUnknown = 0,

  // Account password storage opt-in:
  kAutofillDropdown = 1,
  // The password save bubble, which included the destination picker (set to
  // "Save to your Google Account").
  kPasswordSaveBubble = 2,
  kPasswordSettings = 3,
  kGeneratePasswordDropdown = 4,
  kGeneratePasswordContextMenu = 5,
  kPasswordMoveBubble = 6,
  // The password save bubble *without* a destination picker, i.e. the password
  // was already saved locally.
  kPasswordSaveLocallyBubble = 7,

  kMaxValue = kPasswordSaveLocallyBubble
};

// Enum values which enumerates all user actions on the sign-in promo.
enum class PromoAction : int {
  PROMO_ACTION_NO_SIGNIN_PROMO = 0,
  // The user selected the default account.
  PROMO_ACTION_WITH_DEFAULT,
  // On desktop, the user selected an account that is not the default. On
  // mobile, the user selected the generic "Use another account" button.
  PROMO_ACTION_NOT_DEFAULT,
  // Non personalized promo, when there is no account on the device.
  PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT,
  // The user clicked on the "Add account" button, when there are already
  // accounts on the device. (desktop only, the button does not exist on
  // mobile).
  PROMO_ACTION_NEW_ACCOUNT_EXISTING_ACCOUNT
};

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
// This class is used to record user action that was taken after
// receiving the header from Gaia in the web sign-in flow.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.signin.metrics
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: AccountConsistencyPromoAction
enum class AccountConsistencyPromoAction : int {
  // Promo is not shown as there are no accounts on device.
  SUPPRESSED_NO_ACCOUNTS = 0,
  // User has dismissed the promo by tapping back button.
  DISMISSED_BACK = 1,
  // User has tapped |Add account to device| from expanded account list.
  ADD_ACCOUNT_STARTED = 2,

  // Deprecated 05/2021, since the Incognito option has been removed from
  // account picker bottomsheet.
  // STARTED_INCOGNITO_SESSION = 3,

  // User has selected the default account and signed in with it
  SIGNED_IN_WITH_DEFAULT_ACCOUNT = 4,
  // User has selected one of the non default accounts and signed in with it.
  SIGNED_IN_WITH_NON_DEFAULT_ACCOUNT = 5,
  // The promo was shown to user.
  SHOWN = 6,
  // Promo is not shown due to sign-in being disallowed either by an enterprise
  // policy
  // or by |Allow Chrome sign-in| toggle.
  SUPPRESSED_SIGNIN_NOT_ALLOWED = 7,
  // User has added an account and signed in with this account.
  // When this metric is recorded, we won't record
  // SIGNED_IN_WITH_DEFAULT_ACCOUNT or
  // SIGNED_IN_WITH_NON_DEFAULT_ACCOUNT.
  SIGNED_IN_WITH_ADDED_ACCOUNT = 8,
  // User has dismissed the promo by tapping on the scrim above the bottom
  // sheet.
  DISMISSED_SCRIM = 9,
  // User has dismissed the promo by swiping down the bottom sheet.
  DISMISSED_SWIPE_DOWN = 10,
  // User has dismissed the promo by other means.
  DISMISSED_OTHER = 11,
  // The auth error screen was shown to the user.
  AUTH_ERROR_SHOWN = 12,
  // The generic error screen was shown to the user.
  GENERIC_ERROR_SHOWN = 13,
  // User has dismissed the promo by tapping on the dismissal button in the
  // bottom sheet.
  DISMISSED_BUTTON = 14,
  // User has completed the account addition flow triggered from the bottom
  // sheet.
  ADD_ACCOUNT_COMPLETED = 15,
  // The bottom sheet was suppressed as the user hit consecutive active
  // dismissal limit.
  SUPPRESSED_CONSECUTIVE_DISMISSALS = 16,
  // The timeout erreur was shown to the user.
  TIMEOUT_ERROR_SHOWN = 17,
  // The web sign-in is not shown because the user is already signed in.
  SUPPRESSED_ALREADY_SIGNED_IN = 18,
  // AuthenticationFlow failed to sign-in.
  SIGN_IN_FAILED = 19,
  // The promo was shown to the user, with no existing on-device account. (i.e.
  // the no-account menu was shown)
  SHOWN_WITH_NO_DEVICE_ACCOUNT = 20,
  // User tapped on "Sign In…" in the no-account menu of the bottom sheet,
  // starting an add-account flow.
  ADD_ACCOUNT_STARTED_WITH_NO_DEVICE_ACCOUNT = 21,
  // User successfully added an account after tapping "Sign In…" from the
  // no-account menu.
  ADD_ACCOUNT_COMPLETED_WITH_NO_DEVICE_ACCOUNT = 22,
  // User started with the bottom sheet without a device-account, and signed in
  // to chrome by finishing the add-account and sign-in flows.
  SIGNED_IN_WITH_NO_DEVICE_ACCOUNT = 23,
  kMaxValue = SIGNED_IN_WITH_NO_DEVICE_ACCOUNT,
};
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

// Enum values which enumerates all reasons to start sign in process.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Please keep in Sync with "SigninReason" in
// src/tools/metrics/histograms/enums.xml.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.signin.metrics
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: SigninReason
enum class Reason : int {
  kSigninPrimaryAccount = 0,
  kAddSecondaryAccount = 1,
  kReauthentication = 2,
  // REASON_UNLOCK = 3,  // DEPRECATED, profile unlocking was removed.
  // This should never have been used to get signin URL.
  kUnknownReason = 4,
  kForcedSigninPrimaryAccount = 5,
  // Used to simply login and acquire a login scope token without actually
  // signing into any profiles on Chrome. This allows the chrome signin page to
  // work in incognito mode.
  kFetchLstOnly = 6,
  kMaxValue = kFetchLstOnly,
};

// Enum values used for "Signin.AccountReconcilorState.OnGaiaResponse"
// histogram, which records the state of the AccountReconcilor when GAIA returns
// a specific response.
enum class AccountReconcilorState {
  // The AccountReconcilor has finished running and is up to date.
  kOk = 0,
  // The AccountReconcilor is running and gathering information.
  kRunning = 1,
  // The AccountReconcilor encountered an error and stopped.
  kError = 2,
  // The account reconcilor will start running soon.
  kScheduled = 3,
  // The account reconcilor is inactive, e.g. initializing or disabled.
  kInactive = 4,

  // Always the last enumerated type.
  kMaxValue = kInactive,
};

// Values of Signin.AccountType histogram. This histogram records if the user
// uses a gmail account or a managed account when signing in.
enum class SigninAccountType : int {
  // Gmail account.
  kRegular = 0,
  // Managed account.
  kManaged = 1,
  // Always the last enumerated type.
  kMaxValue = kManaged,
};

// When the user is give a choice of deleting their profile or not when signing
// out, the |kDeleted| or |kKeeping| metric should be used. If the user is not
// given any option, then use the |kIgnoreMetric| value should be used.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.signin.metrics
enum class SignoutDelete : int {
  kDeleted = 0,
  kKeeping,
  kIgnoreMetric,
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

// Various sources for refresh token operations (e.g. update or revoke
// credentials).
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SourceForRefreshTokenOperation {
  kUnknown = 0,
  kTokenService_LoadCredentials = 1,
  // DEPRECATED
  // kSupervisedUser_InitSync = 2,
  kInlineLoginHandler_Signin = 3,
  kPrimaryAccountManager_ClearAccount = 4,
  kPrimaryAccountManager_LegacyPreDiceSigninFlow = 5,
  kUserMenu_RemoveAccount = 6,
  kUserMenu_SignOutAllAccounts = 7,
  kSettings_Signout = 8,
  kSettings_PauseSync = 9,
  kAccountReconcilor_GaiaCookiesDeletedByUser = 10,
  kAccountReconcilor_GaiaCookiesUpdated = 11,
  kAccountReconcilor_Reconcile = 12,
  kDiceResponseHandler_Signin = 13,
  kDiceResponseHandler_Signout = 14,
  kTurnOnSyncHelper_Abort = 15,
  kMachineLogon_CredentialProvider = 16,
  kTokenService_ExtractCredentials = 17,
  // DEPRECATED on 09/2021 (used for force migration to DICE)
  // kAccountReconcilor_RevokeTokensNotInCookies = 18,
  kLogoutTabHelper_PrimaryPageChanged = 19,

  kMaxValue = kLogoutTabHelper_PrimaryPageChanged,
};

// Different types of reporting. This is used as a histogram suffix.
enum class ReportingType { PERIODIC, ON_CHANGE };

// Result for fetching account capabilities from the system library, used to
// record histogram Signin.AccountCapabilities.GetFromSystemLibraryResult.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.signin.metrics
enum class FetchAccountCapabilitiesFromSystemLibraryResult {
  // Errors common to iOS and Android.
  kSuccess = 0,
  kErrorGeneric = 1,

  // Errors from 10 to 19 are reserved for Android.
  kApiRequestFailed = 10,
  kApiError = 11,
  kApiNotPermitted = 12,
  kApiUnknownCapability = 13,
  kApiFailedToSync = 14,
  kApiNotAvailable = 15,

  // Errors after 20 are reserved for iOS.
  kErrorMissingCapability = 20,
  kErrorUnexpectedValue = 21,

  kMaxValue = kErrorUnexpectedValue
};

// Enum values used for "Signin.SyncConsentScreen.DataRowClicked"
// histogram, which records that a user tapped on an entry in TangibleSync
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// TODO(crbug.com/1373063): use this enum in java
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.signin.metrics
enum class SigninSyncConsentDataRow {
  // The bookmark row is tapped.
  kBookmarksRowTapped = 0,
  // The Autofill row is tapped.
  kAutofillRowTapped = 1,
  // The "History and more" row is tapped.
  kHistoryRowTapped = 2,
  // Always the last enumerated type.
  kMaxValue = kHistoryRowTapped,
};

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

// Logs sign in offered events and their associated access points.
// Access points (or features) are responsible for recording this where relevant
// for them.
void LogSignInOffered(AccessPoint access_point);

// Logs sign in start events and their associated access points. The
// completion events are automatically logged when the primary account state
// changes, see `signin::PrimaryAccountMutator`.
void LogSignInStarted(AccessPoint access_point);

// Logs sync opt-in start events and their associated access points. The
// completion events are automatically logged when the primary account state
// changes, see `signin::PrimaryAccountMutator`.
void LogSyncOptInStarted(AccessPoint access_point);

// Logs that the sync settings were opened at the end of the sync opt-in flow,
// and the associated access points.
void LogSyncSettingsOpened(AccessPoint access_point);

// Logs to UMA histograms how many accounts are in the browser for this
// profile.
void RecordAccountsPerProfile(int total_number_accounts);

// Logs duration of a single execution of AccountReconciler to UMA histograms.
// |duration| - How long execution of AccountReconciler took.
// |successful| - True if AccountReconciler was successful.
void LogSigninAccountReconciliationDuration(base::TimeDelta duration,
                                            bool successful);

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

// Records the AccountReconcilor |state| when GAIA returns a specific response.
// If |state| is different than ACCOUNT_RECONCILOR_OK it means the user will
// be shown a different set of accounts in the content-area and the settings UI.
void LogAccountReconcilorStateOnGaiaResponse(AccountReconcilorState state);

// Records the amount of time since the cookie jar was last changed.
void LogCookieJarStableAge(const base::TimeDelta stable_age,
                           const ReportingType type);

// Records three counts for the number of accounts in the cookie jar.
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

// Records the number of signed-in accounts in the cookie jar for the given
// (potentially unconsented) primary account type, characterized by sync being
// enabled (`primary_syncing`) and the account being managed (i.e. enterprise,
// `primary_managed`).
void LogSignedInCookiesCountsPerPrimaryAccountType(int signed_in_accounts_count,
                                                   bool primary_syncing,
                                                   bool primary_managed);

// Records the source that updated a refresh token.
void RecordRefreshTokenUpdatedFromSource(bool refresh_token_is_valid,
                                         SourceForRefreshTokenOperation source);

// Records the source that revoked a refresh token.
void RecordRefreshTokenRevokedFromSource(SourceForRefreshTokenOperation source);

#if BUILDFLAG(IS_IOS)
// Records the account type when the user signs in.
void RecordSigninAccountType(signin::ConsentLevel consent_level,
                             bool is_managed_account);
#endif

// -----------------------------------------------------------------------------
// User actions
// -----------------------------------------------------------------------------

// Records corresponding sign in user action for an access point.
void RecordSigninUserActionForAccessPoint(AccessPoint access_point);

// Records |Signin_Impression_From*| user action.
void RecordSigninImpressionUserActionForAccessPoint(AccessPoint access_point);

#if BUILDFLAG(IS_IOS)
// Records |Signin.AccountConsistencyPromoAction.{PromoEvent}| histogram.
void RecordConsistencyPromoUserAction(AccountConsistencyPromoAction action,
                                      AccessPoint access_point);
#endif  // BUILDFLAG(IS_IOS)

}  // namespace signin_metrics

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_METRICS_H_
