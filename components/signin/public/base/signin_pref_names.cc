// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_pref_names.h"

#include "build/chromeos_buildflags.h"

namespace prefs {

#if BUILDFLAG(IS_CHROMEOS_ASH)
// A boolean pref - should unauthenticated user should be logged out
// automatically. Default value is false.
const char kForceLogoutUnauthenticatedUserEnabled[] =
    "profile.force_logout_unauthenticated_user_enabled";

// An integer property indicating the state of account id migration from
// email to gaia id for the the profile.  See account_tracker_service.h
// for possible values.
const char kAccountIdMigrationState[] = "account_id_migration_state";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Name of the preference property that persists the account information
// tracked by this signin.
const char kAccountInfo[] = "account_info";

// Whether the "clear on exit" migration is complete.
// If this preference is not true, then the user needs to be migrated.
// If a user has set clear cookies on exit prior to the activation of
// `switches:: kExplicitBrowserSigninUIOnDesktop` which changes the behavior of
// signed in users, they will need to do a migration.
// The user can be migrated in various ways:
// - the first time they launch Chrome, if they don't use the cookie setting
// - by changing the value of the setting when it has the new behavior
// - by seeing a notice dialog if they close the browser while being in a state
//   where the new cookie setting behavior makes a difference (signed in with
//   Uno and non-syncing).
const char kCookieClearOnExitMigrationNoticeComplete[] =
    "signin.cookie_clear_on_exit_migration_notice_complete";

// A hash of the GAIA accounts present in the content area. Order does not
// affect the hash, but signed in/out status will. Stored as the Base64 string.
const char kGaiaCookieHash[] = "gaia_cookie.hash";

// The last time that kGaiaCookieHash was last updated. Stored as a double that
// should be converted into base::Time.
const char kGaiaCookieChangedTime[] = "gaia_cookie.changed_time";

// The last time that periodic reporting occured, to allow us to report as close
// to once per intended interval as possible, through restarts. Stored as a
// double that should be converted into base::Time.
const char kGaiaCookiePeriodicReportTime[] = "gaia_cookie.periodic_report_time";

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

// Contains last |ListAccounts| data which corresponds to Gaia cookies.
const char kGaiaCookieLastListAccountsData[] =
    "gaia_cookie.last_list_accounts_data";

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

// List of patterns to determine the account visibility.
const char kRestrictAccountsToPatterns[] =
    "signin.restrict_accounts_to_patterns";

// Boolean which indicates if the user is allowed to sign into Chrome on the
// next startup.
const char kSigninAllowedOnNextStartup[] = "signin.allowed_on_next_startup";

// String that represent the url for which cookies will have to be moved to a
// newly created profile via signin interception.
const char kSigninInterceptionIDPCookiesUrl[] =
    "signin.interception.idp_cookies.url";

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
// Note: this pref is only recorded when the
// `switches::kExplicitBrowserSigninUIOnDesktop` is enabled.
const char kExplicitBrowserSignin[] =
    "signin.signin_with_explicit_browser_signin_on";

// Boolean indicating whether the Device Bound Session Credentials should be
// enabled. Takes precedence over the "EnableBoundSessionCredentials" feature
// flag state. The value is controlled by the BoundSessionCredentialsEnabled
// policy. More details can be found at BoundSessionCredentialsEnabled.yaml.
const char kBoundSessionCredentialsEnabled[] =
    "signin.bound_session_credentials_enabled";

}  // namespace prefs
