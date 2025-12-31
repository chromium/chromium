// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_pref_names.h"

namespace prefs {

#if BUILDFLAG(IS_CHROMEOS)
// An integer property indicating the state of account id migration from
// email to gaia id for the the profile.  See account_tracker_service.h
// for possible values.
const char kAccountIdMigrationState[] = "account_id_migration_state";
#endif  // BUILDFLAG(IS_CHROMEOS)

// Name of the preference property that persists the account information
// tracked by this signin.
const char kAccountInfo[] = "account_info";

// Whether the "clear on exit" migration is complete.
// If this preference is not true, then the user needs to be migrated.
// If a user has set clear cookies on exit prior to the activation of explicit
// signin which changes the behavior of signed in users, they will need to do a
// migration. The user can be migrated in various ways:
// - the first time they launch Chrome, if they don't use the cookie setting
// - by changing the value of the setting when it has the new behavior
// - by seeing a notice dialog if they close the browser while being in a state
//   where the new cookie setting behavior makes a difference (signed in with
//   explicit signin and non-syncing).
const char kCookieClearOnExitMigrationNoticeComplete[] =
    "signin.cookie_clear_on_exit_migration_notice_complete";

// A hash of the GAIA accounts present in the content area. Order does not
// affect the hash, but signed in/out status will. Stored as the Base64 string.
const char kGaiaCookieHash[] = "gaia_cookie.hash";

// The last time that kGaiaCookieHash was last updated. Stored as a double that
// should be converted into base::Time.
const char kGaiaCookieChangedTime[] = "gaia_cookie.changed_time";

// The last time that periodic reporting occured, to allow us to report as close
// to once per intended interval as possible, through restarts.
const char kGaiaCookiePeriodicReportTime[] =
    "gaia_cookie.periodic_report_time_2";

// Typically contains an obfuscated gaiaid. Some platforms may have
// an email stored in this preference instead. This is transitional and will
// eventually be fixed.
const char kGoogleServicesAccountId[] = "google.services.account_id";

// Boolean indicating if the user gave consent for Sync.
const char kGoogleServicesConsentedToSync[] =
    "google.services.consented_to_sync";

// Similar to `kGoogleServicesLastSyncingUsername` that is not cleared on
// signout. Note this is always a Gaia ID, as opposed to
// `kGoogleServicesAccountId` which may be an email.
const char kGoogleServicesLastSyncingGaiaId[] = "google.services.last_gaia_id";

// String the identifies the last user that logged into sync and other
// google services. This value is not cleared on signout.
// This pref remains in order to pre-fill the sign in page when reconnecting a
// profile, but programmatic checks to see if a given account is the same as the
// last account should use `kGoogleServicesLastSyncingGaiaId` instead.
const char kGoogleServicesLastSyncingUsername[] =
    "google.services.last_username";

// Similar to kGoogleServicesLastSyncingUsername above but written for all
// signed-in users, no matter whether they were syncing or not.
const char kGoogleServicesLastSignedInUsername[] =
    "google.services.last_signed_in_username";

// Device id scoped to single signin. This device id will be regenerated if user
// signs out and signs back in. When refresh token is requested for this user it
// will be annotated with this device id.
const char kGoogleServicesSigninScopedDeviceId[] =
    "google.services.signin_scoped_device_id";

// A string indicating the Gaia ID (as in `kGoogleServicesAccountId`) of a
// user who was previously syncing (had `kGoogleServicesConsentedToSync` set to
// true), and was migrated to the signed-in non-syncing state. See feature
// `kMigrateSyncingUserToSignedIn`.
const char kGoogleServicesSyncingGaiaIdMigratedToSignedIn[] =
    "google.services.syncing_gaia_id_migrated_to_signed_in";

// Like `kGoogleServicesSyncingAccountIdMigratedToSignedIn` but for the username
// instead of the account ID.
const char kGoogleServicesSyncingUsernameMigratedToSignedIn[] =
    "google.services.syncing_username_migrated_to_signed_in";

// Local state pref containing a string regex that restricts which accounts
// can be used to log in to chrome (e.g. "*@google.com"). If missing or blank,
// all accounts are allowed (no restrictions).
const char kGoogleServicesUsernamePattern[] =
    "google.services.username_pattern";

// Boolean indicating if this profile was signed in with information from a
// credential provider.
const char kSignedInWithCredentialProvider[] =
    "signin.with_credential_provider";

// Boolean which stores if the user is allowed to signin to chrome.
const char kSigninAllowed[] = "signin.allowed";

// Contains last |ListAccounts| data which corresponds to Gaia cookies in
// base64-encoded protobuf.
const char kGaiaCookieLastListAccountsBinaryData[] =
    "gaia_cookie.last_list_accounts_binary_data";

// The timestamp when History Sync was last declined (in the opt-in screen or
// in the settings).
// This value is reset when the user opts in to History Sync.
// TODO(b/344543852): This pref is not used on iOS. Migrate the equivalent iOS
// pref to this one.
const char kHistorySyncLastDeclinedTimestamp[] =
    "signin.history_sync.last_declined_timestamp";

// Number of times the user successively declined History Sync (in the opt-in
// screen or in the settings).
// This value is reset to zero when the user accepts History Sync.
// TODO(b/344543852): This pref is not used on iOS. Migrate the equivalent iOS
// pref to this one.
const char kHistorySyncSuccessiveDeclineCount[] =
    "signin.history_sync.successive_decline_count";

// A timestamp of the last time the history sync promo was dismissed.
const char kHistoryPageHistorySyncPromoLastDismissedTimestamp[] =
    "history_page.history_sync_promo_last_dismissed_timestamp";

// A boolean preference to store whether the history sync promo was shown one
// more time after the user dismissed it.
const char kHistoryPageHistorySyncPromoShownAfterDismissal[] =
    "history_page.history_sync_promo_shown_after_dismissal";

// An integer preference to store the number of times the history sync promo
// has been shown on the history page.
const char kHistoryPageHistorySyncPromoShownCount[] =
    "history.sync_promo_shown_count";

#if BUILDFLAG(IS_IOS)
// List of patterns to determine the account visibility, according to the
// "RestrictAccountsToPatterns" policy. Note that the policy also exists on
// Android, but has a separate implementation there which doesn't use this pref.
const char kRestrictAccountsToPatterns[] =
    "signin.restrict_accounts_to_patterns";

// Boolean that represent whether signin is allowed by the user. It is also used
// to synchronize kSigninAllowed across profiles. This is used to
// ensure that all profiles respect the setting while `kSigninAllowed` only
// applies to a single profile. This is the UX we want on iOS since there are
// multi profiles but not exposed to the user, so we should treat this setting
// as affecting all profiles.
const char kSigninAllowedOnDevice[] = "signin.allowed_on_device";

// TODO(crbug.com/424385780): Update this comment.
// Integer that represents the value of BrowserSigninPolicy. Values are defined
// in ios/chrome/browser/policy/model/policy_util.h.
const char kBrowserSigninPolicy[] = "signin.browser_signin_policy";
#endif  // BUILDFLAG(IS_IOS)

// Boolean which indicates if the user is allowed to sign into Chrome on the
// next startup.
const char kSigninAllowedOnNextStartup[] = "signin.allowed_on_next_startup";

// String that represent the url for which cookies will have to be moved to a
// newly created profile via signin interception.
const char kSigninInterceptionIDPCookiesUrl[] =
    "signin.interception.idp_cookies.url";

// Integer pref to store the number of times the address bubble signin promo
// has been shown per profile while the user is signed out used for
// SigninPromoLimitsExperiment.
const char kAddressSignInPromoShownCountPerProfileForLimitsExperiment[] =
    "signin.AddressSignInPromoShownCountForLimitsExperiment";

// Integer pref to store the number of times the bookmark bubble signin promo
// has been shown per profile while the user is signed out used for
// SigninPromoLimitsExperiment.
const char kBookmarkSignInPromoShownCountPerProfileForLimitsExperiment[] =
    "signin.BookmarkSignInPromoShownCountForLimitsExperiment";

// Integer pref to store the number of times the password bubble signin promo
// has been shown per profile while the user is signed out used for
// SigninPromoLimitsExperiment.
const char kPasswordSignInPromoShownCountPerProfileForLimitsExperiment[] =
    "signin.PasswordSignInPromoShownCountForLimitsExperiment";

// Integer which indicates whether enterprise profile separation is enforced or
// disabled.
const char kProfileSeparationSettings[] = "profile_separation.settings";

// Integer which indicates which options users have for their existing data when
// creating a new profile via the enterprise profile separation flow.
const char kProfileSeparationDataMigrationSettings[] =
    "profile_separation.data_migration_settings";

// List of domains that are not required to create a new profile after a content
// area signin.
const char kProfileSeparationDomainExceptionList[] =
    "profile_separation.domain_exception_list";

// Response set by chrome://policy/test for
// UserCloudSigninRestrictionPolicyFetcher::GetManagedAccountsSigninRestriction.
// This is only used on Canary and for testing.
const char kUserCloudSigninPolicyResponseFromPolicyTestPage[] =
    "signin.user_cloud_signin_policy_response_from_policy_test_page";

// Registers that the sign in occurred with an explicit user action.
// Affected by all signin sources except when signing in to Chrome caused by a
// web sign in or by an unknown source.
// Note: this pref is only recorded when explicit signin is enabled.
const char kExplicitBrowserSignin[] =
    "signin.signin_with_explicit_browser_signin_on";

// Whether the account storage for preferences, themes and search engines is
// enabled by default. Only set on new signins and for sync users.
// Note: this pref is only recorded when the feature
// `syncer::kEnablePreferencesAccountStorage` is enabled.
const char kPrefsThemesSearchEnginesAccountStorageEnabled[] =
    "signin.prefs_themes_search_engines_account_storage_enabled";

// Boolean indicating whether the Device Bound Session Credentials should be
// enabled. Takes precedence over the "EnableBoundSessionCredentials" feature
// flag state. The value is controlled by the BoundSessionCredentialsEnabled
// policy. More details can be found at BoundSessionCredentialsEnabled.yaml.
const char kBoundSessionCredentialsEnabled[] =
    "signin.bound_session_credentials_enabled";

// A boolean that is true if the primary account was set after the
// sync-to-signin migration, where Sync is deprecated. This value is not cleared
// on signout.
//
// This pref is used to provide a different welcome experience for various
// groups of users. If false (signed in before the migration) and if the user:
//   - did not have Sync enabled,
//   - was not migrated from DICe,
// they are shown an In-Product Help (IPH) bubble explaining the new benefits.
// Otherwise, other UIs are used to inform the user of the benefits.
const char kPrimaryAccountSetAfterSigninMigration[] =
    "signin.primary_account_set_after_signin_migration";

}  // namespace prefs
