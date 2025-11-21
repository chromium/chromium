// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_METRICS_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_METRICS_H_

#include <limits.h>

#include "build/build_config.h"
#include "components/signin/public/base/consent_level.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace signin_metrics {

// Track all the ways a profile can become signed out as a histogram.
// Enum SigninSignoutProfile.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.signin.metrics
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: SignoutReason
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange
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
  // User retriggered signin from the Android sign-in bottomsheet.
  kSigninRetriggered = 18,
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
  // promo in bookmarks and reading list page.
  kUserTappedUndoRightAfterSignIn = 26,
  // User has signed-in previously for the sole purpose of enabling history sync
  // (eg. using history sync promo in recent tabs), but declined history sync
  // eventually.
  kUserDeclinedHistorySyncAfterDedicatedSignIn = 27,
  // If the device lock is removed from an Android automotive device, the
  // current account is automatically signed out.
  kDeviceLockRemovedOnAutomotive = 28,
  // User revoked Sync from the Settings by pressing "Turn off" in the "Sync and
  // Google Services" page.
  kRevokeSyncFromSettings = 29,
  // User was in the web-only signed in state in the UNO model and clicked to
  // turn on sync, but cancelled the sync confirmation dialog so they are
  // reverted to the initial state, signed out in the profile but keeping the
  // account on the web only.
  kCancelSyncConfirmationOnWebOnlySignedIn = 30,
  // Profile signout when IdleTimeoutActions enterprise policy triggers sign
  // out.
  kIdleTimeoutPolicyTriggeredSignOut = 31,
  // User adds the primary account through the sync flow then aborts.
  kCancelSyncConfirmationRemoveAccount = 32,
  // Move primary account to another profile on sign in interception or sync
  // merge data confirmation.
  kMovePrimaryAccount = 33,
  // Signout as part of the profile deletion procedure, to avoid that deletion
  // of data propagates via sync.
  kSignoutDuringProfileDeletion = 34,
  // Signout to switch account. This can still happen when switching from a
  // managed profile to the personal profile, if the personal profile doesn't
  // have the right primary account.
  kSignoutForAccountSwitching = 35,
  // User clicked to signout from the account menu view.
  kUserClickedSignoutInAccountMenu = 36,
  // User disabled allow chrome sign-in from google settings page.
  kUserDisabledAllowChromeSignIn = 37,
  // User was forced signed out as there was a supervised user added to the
  // device.
  kSignoutBeforeSupervisedSignin = 38,
  // Triggered when the user opens the app from a widget with no selected
  // account. iOS only.
  kSignoutFromWidgets = 39,
  // User declined the enterprise management disclaimer.
  kUserDeclinedEnterpriseManagementDisclaimer = 40,
  // DICe user was forcefully signed out.
  kForcedDiceMigration = 41,
  // Keep this as the last enum.
  kMaxValue = kForcedDiceMigration,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/signin/enums.xml)

// Enum values which enumerates all access points where sign in could be
// initiated. Not all of them exist on all platforms.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.signin.metrics
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: SigninAccessPoint
// LINT.IfChange
enum class AccessPoint : int {
  kStartPage = 0,
  kNtpLink = 1,
  // Access point from the three dot app menu.
  kMenu = 2,
  kSettings = 3,
  kSupervisedUser = 4,
  kExtensionInstallBubble = 5,
  kExtensions = 6,
  // kAppsPageLink = 7, no longer used.
  kBookmarkBubble = 8,
  kBookmarkManager = 9,
  kAvatarBubbleSignIn = 10,
  kUserManager = 11,
  kDevicesPage = 12,
  // kCloudPrint = 13, no longer used.
  // kContentArea = 14, no longer used.
  kFullscreenSigninPromo = 15,
  kRecentTabs = 16,
  // This should never have been used to get signin URL.
  kUnknown = 17,
  kPasswordBubble = 18,
  kAutofillDropdown = 19,
  // kNtpContentSuggestions = 20, no longer used.
  kResigninInfobar = 21,
  kTabSwitcher = 22,
  // kForceSigninWarning = 23, no longer used.
  // kSaveCardBubble = 24, no longer used
  // kManageCardsBubble = 25, no longer used
  kMachineLogon = 26,
  kGoogleServicesSettings = 27,
  kSyncErrorCard = 28,
  kForcedSignin = 29,
  kAccountRenamed = 30,
  kWebSignin = 31,
  kSafetyCheck = 32,
  kKaleidoscope = 33,
  kEnterpriseSignoutCoordinator = 34,
  kSigninInterceptFirstRunExperience = 35,
  kSendTabToSelfPromo = 36,
  kNtpFeedTopPromo = 37,
  kSettingsSyncOffRow = 38,
  kPostDeviceRestoreSigninPromo = 39,
  kPostDeviceRestoreBackgroundSignin = 40,
  kNtpSignedOutIcon = 41,
  kNtpFeedCardMenuPromo = 42,
  kNtpFeedBottomPromo = 43,
  kDesktopSigninManager = 44,
  // Access point for the "For You" First Run Experience on Desktop. See
  // go/for-you-fre or launch/4223982 for more info.
  kForYouFre = 45,
  // Access point for Cormorant (Creator Feed) on Android only when the "Follow"
  // button is tapped while in a signed-out state.
  kCreatorFeedFollow = 46,
  // Access point for the reading list sign-in promo (launch/4231282).
  kReadingList = 47,
  // Access point for the reauth info bar.
  kReauthInfoBar = 48,
  // Access point for the consistency service.
  kAccountConsistencyService = 49,
  // Access point for the search companion sign-in promo.
  kSearchCompanion = 50,
  // Access point for the IOS Set Up List on the NTP.
  kSetUpList = 51,
  // Access point for the local password migration warning on Android.
  // Deprecated: kPasswordMigrationWarningAndroid = 52,
  // Access point for the Save to Photos feature on iOS.
  kSaveToPhotosIos = 53,
  // Access point for the Chrome Signin Intercept Bubble.
  kChromeSigninInterceptBubble = 54,
  // Restore primary account info in case it was lost.
  kRestorePrimaryAccountOnProfileLoad = 55,
  // Access point for the tab organization UI within the tab search bubble.
  kTabOrganization = 56,
  // Access point for the Save to Drive feature on iOS.
  kSaveToDriveIos = 57,
  // Access point for the Tips Notification on iOS.
  kTipsNotification = 58,
  // Access point for the Notifications Opt-In Screen.
  kNotificationsOptInScreenContentToggle = 59,
  // Access point for a web sign with an explicit signin choice remembered.
  kSigninChoiceRemembered = 60,
  // Confirmation prompt shown when the user tries to sign out from the profile
  // menu or settings. The signout prompt may have a "Verify it's you" button
  // allowing the user to reauth.
  kProfileMenuSignoutConfirmationPrompt = 61,
  kSettingsSignoutConfirmationPrompt = 62,
  // The identity disc (avatar) on the New Tab page. Note that this only covers
  // SignedIn avatars - interactions with the signed-out avatar are instead
  kNtpIdentityDisc = 63,
  // The identity is received through an interception of a 3rd party OIDC auth
  // redirection.
  kOidcRedirectionInterception = 64,
  // The "Sign in again" button on a Web Authentication modal dialog when
  // reauthentication is necessary to sign in with or save a passkey from the
  // Google Password Manager.
  kWebauthnModalDialog = 65,
  // Signin button from the profile menu that is labelled as a "Signin" button,
  // but is followed by a Sync confirmation screen as a promo.
  kAvatarBubbleSignInWithSyncPromo = 66,
  // Signin as part of switching accounts via the account menu.
  kAccountMenuSwitchAccount = 67,
  // Signin via Product Specifications.
  kProductSpecifications = 68,
  // The user is signed-back into their previous account after failing to switch
  // to a new one.
  kAccountMenuSwitchAccountFailed = 69,
  // The user signs in from a sign in promo after an address save.
  kAddressBubble = 70,
  // A message notification displayed on CCTs embedded in 1P apps when there is
  // an account mismatch between Chrome and the 1P app. Android only.
  kCctAccountMismatchNotification = 71,
  // Access point for the Drive file picker on iOS.
  kDriveFilePickerIos = 72,
  // Access point triggered when a user attempts to share a tab group without
  // being signed in or synced.
  kCollaborationShareTabGroup = 73,
  // Glic launch button on the tab strip.
  kGlicLaunchButton = 74,
  // History sync promo shown on the History page. Should not be visible when
  // the use is not signed-in. Android only.
  kHistoryPage = 75,
  // Access point triggered when a user attempts to join a tab group without
  // being signed in or synced.
  kCollaborationJoinTabGroup = 76,
  // Access point triggered when a user attempts to opt-in to history sync from
  // the history sync opt-in expanded pill (expanded on startup).
  kHistorySyncOptinExpansionPillOnStartup = 77,
  // Access point triggered when the account used in widget is different from
  // the one used in the app. iOS only.
  kWidget = 78,
  // Access point triggered when a user attempts to leave or delete a tab group
  // without being signed in or synced.
  kCollaborationLeaveOrDeleteTabGroup = 79,
  // Access point triggered when a user attempts to opt-in to history sync from
  // the history sync opt-in expanded pill (expanded on inactivity).
  // kHistorySyncOptinExpansionPillOnInactivity = 80, // no longer used
  // History sync education tip is shown on the NTP to users who have history
  // sync disabled. Android only.
  kHistorySyncEducationalTip = 81,
  // iOS only: The user switched to a managed account for the first time, and
  // the corresponding profile was automatically signed in.
  kManagedProfileAutoSigninIos = 82,
  // iOS only: Access point for the contextual non modal sign-in password promo.
  kNonModalSigninPasswordPromo = 83,
  // iOS only: Access point for the contextual non modal sign-in bookmark promo.
  kNonModalSigninBookmarkPromo = 84,
  // Access point for the user manager with a prefilled email.
  kUserManagerWithPrefilledEmail = 85,
  // Access point for the enterprise management disclaimer at startup.
  kEnterpriseManagementDisclaimerAtStartup = 86,
  // Access point for the enterprise management disclaimer after browser focus.
  kEnterpriseManagementDisclaimerAfterBrowserFocus = 87,
  // Access point for the enterprise management disclaimer after sign-in.
  kEnterpriseManagementDisclaimerAfterSignin = 88,
  // New Tab Page sign-in feature promotion.
  kNtpFeaturePromo = 89,
  // Access point for the enterprise interception that result in profile
  // separation.
  kEnterpriseDialogAfterSigninInterception = 90,
  // "Your saved info" settings page.
  kSettingsYourSavedInfo = 91,
  // Add values above this line with a corresponding label to the
  // "SigninAccessPoint" enum in
  // tools/metrics/histograms/metadata/signin/enums.xml.
  kMaxValue = kSettingsYourSavedInfo,  // This must be last.
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/signin/enums.xml)

// Enum values which enumerates all user actions on the sign-in promo.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.signin.metrics
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: SigninPromoAction
// GENERATED_JAVA_PREFIX_TO_STRIP: PROMO_ACTION_
enum class PromoAction : int {
  PROMO_ACTION_NO_SIGNIN_PROMO = 0,
  // The user selected the default account.
  PROMO_ACTION_WITH_DEFAULT = 1,
  // On desktop, the user selected an account that is not the default. On
  // mobile, the user selected the generic "Use another account" button.
  PROMO_ACTION_NOT_DEFAULT = 2,
  // Non personalized promo, when there is no account on the device.
  PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT = 3,
  // The user clicked on the "Add account" button, when there are already
  // accounts on the device. (desktop only, the button does not exist on
  // mobile).
  PROMO_ACTION_NEW_ACCOUNT_EXISTING_ACCOUNT = 4,
  kMaxValue = PROMO_ACTION_NEW_ACCOUNT_EXISTING_ACCOUNT,
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
  // AuthenticationFlow on iOS is cancelled or failed to sign-in.
  IOS_AUTH_FLOW_CANCELLED_OR_FAILED = 19,
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
  // User was shown the confirm management screen on signin.
  CONFIRM_MANAGEMENT_SHOWN = 24,
  // User accepted management on signin.
  CONFIRM_MANAGEMENT_ACCEPTED = 25,
  kMaxValue = CONFIRM_MANAGEMENT_ACCEPTED,
};
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

// Enum values which enumerates all reasons to start sign in process.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Please keep in sync with "SigninReason" in
// src/tools/metrics/histograms/enums.xml.
enum class Reason : int {
  // Used only for the Sync flows, i.e. the user will be proposed to enable Sync
  // after sign-in.
  kSigninPrimaryAccount = 0,
  // Used for signing in without enabling Sync. This might also be used for
  // adding a new primary account without enabling Sync.
  kAddSecondaryAccount = 1,
  kReauthentication = 2,
  // REASON_UNLOCK = 3,  // DEPRECATED, profile unlocking was removed.
  // This should never have been used to get signin URL.
  kUnknownReason = 4,
  kForcedSigninPrimaryAccount = 5,
  // Used to simply login and acquire a login scope token without actually
  // signing into any profiles on Chrome. This allows the Chrome sign-in page to
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
// LINT.IfChange(SourceForRefreshTokenOperation)
enum class SourceForRefreshTokenOperation {
  kUnknown = 0,
  kTokenService_LoadCredentials = 1,
  // DEPRECATED
  // kSupervisedUser_InitSync = 2,
  kInlineLoginHandler_Signin = 3,
  kPrimaryAccountManager_ClearAccount = 4,
  // DEPRECATED
  // kPrimaryAccountManager_LegacyPreDiceSigninFlow = 5,
  // DEPRECATED
  // kUserMenu_RemoveAccount = 6,
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
  kForceSigninReauthWithDifferentAccount = 20,
  kAccountReconcilor_RevokeTokensNotInCookies = 21,
  // DEPRECATED on 05/2024
  // kDiceResponseHandler_PasswordPromoSignin = 22,
  kEnterpriseForcedProfileCreation_UserDecline = 23,
  kEnterprisePolicy_AccountNotAllowedInContentArea = 24,
  kDiceAccountReconcilorDelegate_RefreshTokensBoundToDifferentKeys = 25,

  kMaxValue = kDiceAccountReconcilorDelegate_RefreshTokensBoundToDifferentKeys,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:SourceForRefreshTokenOperation)

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

// Tracks type of the button that was presented to the user.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.signin.metrics
enum class SyncButtonsType : int {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  kSyncEqualWeighted = 0,
  kSyncNotEqualWeighted = 1,

  // kHistorySyncEqualWeighted = 2,  // no longer used, split into
  // `kHistorySyncEqualWeightedFromDeadline` and
  // `kHistorySyncEqualWeightedFromCapability`

  kHistorySyncNotEqualWeighted = 3,

  // Either use one of the two or kSyncEqualWeighted.
  kSyncEqualWeightedFromDeadline = 4,
  kSyncEqualWeightedFromCapability = 5,

  kHistorySyncEqualWeightedFromDeadline = 6,
  kHistorySyncEqualWeightedFromCapability = 7,

  kMaxValue = kHistorySyncEqualWeightedFromCapability,
};

#if BUILDFLAG(IS_IOS)
// The reason an alert dialog is shown when the user is about to sign out.
enum class SignoutDataLossAlertReason : int {
  // The user has unsynced data that will be lost on signout.
  kSignoutWithUnsyncedData = 0,
  // A managed user is signing out and the data will be cleared from the device.
  kSignoutWithClearDataForManagedUser = 1,
};

// Values of Signin.AccountType histogram. This histogram records if the user
// uses a gmail account or a managed account when signing in.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Keep in sync with SigninAccountType in
// tools/metrics/histograms/metadata/signin/enums.xml.
enum class SigninAccountType {
  // Gmail account.
  kRegular = 0,
  // Managed account.
  kManaged = 1,
  // Always the last enumerated type.
  kMaxValue = kManaged,
};

// Event within the reauth flow.
//
// LINT.IfChange(ReauthFlowEvent)
enum class ReauthFlowEvent : int {
  // The reauth flow has started.
  kStarted = 0,
  // The reauth flow has completed successfully.
  kCompleted = 1,
  // There was an error during the reauth flow.
  kError = 2,
  // The reauth flow was cancelled by the user.
  kCancelled = 3,
  // The reauth flow was cancelled because the coordinator was stopped.
  kInterrupted = 4,
  kMaxValue = kInterrupted
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/signin/histograms.xml:ReauthFlowEvent)

// Identifies explicit reauthentication UI entry points.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(ReauthAccessPoint)
enum class ReauthAccessPoint : int {
  // Error card in the account menu.
  kAccountMenu = 0,
  kMaxValue = kAccountMenu,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:ReauthAccessPoint)
#endif  // BUILDFLAG(IS_IOS)

// -----------------------------------------------------------------------------
// Histograms
// -----------------------------------------------------------------------------

// Tracks the access point of sign in.
void LogSigninAccessPointStarted(AccessPoint access_point,
                                 PromoAction promo_action);
void LogSigninAccessPointCompleted(AccessPoint access_point,
                                   PromoAction promo_action);

// Logs sign in offered events and their associated access points.
// Access points (or features) are responsible for recording this where relevant
// for them. The `promo_action` determines which specific histogram will be
// recorded based and should be computed based on the signin state when the
// promo is offered.
void LogSignInOffered(AccessPoint access_point, PromoAction promo_action);

// Logs sign in start events and their associated access points. The
// completion events are automatically logged when the primary account state
// changes, see `signin::PrimaryAccountMutator`.
void LogSignInStarted(AccessPoint access_point);

// Logs that sign in was offered when the user is in SigninPending state.
void LogSigninPendingOffered(AccessPoint access_point);

#if BUILDFLAG(IS_IOS)
// Records the account type when the user signs in.
void LogSigninWithAccountType(SigninAccountType account_type);
#endif  // BUILDFLAG(IS_IOS)

// Logs sync opt-in start events and their associated access points. The
// completion events are automatically logged when the primary account state
// changes, see `signin::PrimaryAccountMutator`.
void LogSyncOptInStarted(AccessPoint access_point);

// Logs a sync opt-in offered event (`Signin.SyncOptIn.Offered` histogram)
// and its associated access point.
void LogSyncOptInOffered(AccessPoint access_point);

// Logs a sync opt-in offered event (`Signin.HistorySyncOptIn.Offered`
// histogram) and its associated access point.
void LogHistorySyncOptInOffered(AccessPoint access_point);

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
void LogSignout(ProfileSignout source_metric);

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
// Records whether the user choose to "Sign Out" or "Cancel" when an alert for
// data loss is displayed.
void RecordSignoutConfirmationFromDataLossAlert(
    SignoutDataLossAlertReason reason,
    bool signout_confirmed);

// Records the progression of the reauthentication flow that was started within
// the sign-in flow designated by `access_point`. `event` is converted into a
// suffix for `Signin.Reauth.InSigninFlow` histogram family.
void RecordReauthFlowEventInSigninFlow(signin_metrics::AccessPoint access_point,
                                       ReauthFlowEvent event);

// Records the progression of the reauthentication flow that was started via an
// explicit reauthentication UI. `event` is converted into a suffix for
// `Signin.Reauth.InSigninFlow` histogram family.
void RecordReauthFlowEventInExplicitFlow(ReauthAccessPoint access_point,
                                         ReauthFlowEvent event);
#endif  // BUILDFLAG(IS_IOS)

// Records the total number of open tabs at the moment of signin or enabling
// sync.
void RecordOpenTabCountOnSignin(signin::ConsentLevel consent_level,
                                size_t tabs_count);

// Records the history opt-in state, at the moment of signin or turning on sync.
// For `ConsentLevel::kSync` users, this is true by default. Conversely, for
// `ConsentLevel::kSignin` users, it's false by default, unless the same user
// was previously signed in and has opted in then. Note that, depending on the
// signin entry point and other conditions, the user may be presented with a
// history opt-in right after this is recorded.
void RecordHistoryOptInStateOnSignin(signin_metrics::AccessPoint access_point,
                                     signin::ConsentLevel consent_level,
                                     bool opted_in);

// -----------------------------------------------------------------------------
// User actions
// -----------------------------------------------------------------------------

// Records corresponding sign in user action for an access point.
void RecordSigninUserActionForAccessPoint(AccessPoint access_point);

// Records |Signin_Impression_From*| user action.
void RecordSigninImpressionUserActionForAccessPoint(AccessPoint access_point);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
// Records |Signin.AccountConsistencyPromoAction.{PromoEvent}| histogram.
void RecordConsistencyPromoUserAction(AccountConsistencyPromoAction action,
                                      AccessPoint access_point);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

}  // namespace signin_metrics

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_METRICS_H_
