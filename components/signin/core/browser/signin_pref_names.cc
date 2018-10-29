// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_pref_names.h"

namespace prefs {

#if defined(OS_CHROMEOS)
// Boolean identifying if Mirror account consistency is required for profile.
const char kAccountConsistencyMirrorRequired[] =
    "account_consistency_mirror.required";
#endif

// An integer property indicating the state of account id migration from
// email to gaia id for the the profile.  See account_tracker_service.h
// for possible values.
const char kAccountIdMigrationState[] = "account_id_migration_state";

// Boolean identifying whether reverse auto-login is enabled.
const char kAutologinEnabled[] = "autologin.enabled";

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

// Typically contains an obfuscated gaiaid and will match the value of
// kGoogleServicesUserAccountId. Some platforms and legacy clients may have
// an email stored in this preference instead. This is transitional and will
// eventually be fixed, allowing the removal of kGoogleServicesUserAccountId.
const char kGoogleServicesAccountId[] = "google.services.account_id";

// The profile's hosted domain; empty if unset;
// AccountTrackerService::kNoHostedDomainFound if there is none.
const char kGoogleServicesHostedDomain[] = "google.services.hosted_domain";

// Similar to kGoogleServicesLastUsername, this is the corresponding version of
// kGoogleServicesAccountId that is not cleared on signout.
const char kGoogleServicesLastAccountId[] = "google.services.last_account_id";

// String the identifies the last user that logged into sync and other
// google services. As opposed to kGoogleServicesUsername, this value is not
// cleared on signout, but while the user is signed in the two values will
// be the same.  This pref remains in order to pre-fill the sign in page when
// reconnecting a profile, but programmatic checks to see if a given account
// is the same as the last account should use kGoogleServicesLastAccountId
// instead.
const char kGoogleServicesLastUsername[] = "google.services.last_username";

// Device id scoped to single signin. This device id will be regenerated if user
// signs out and signs back in. When refresh token is requested for this user it
// will be annotated with this device id.
const char kGoogleServicesSigninScopedDeviceId[] =
    "google.services.signin_scoped_device_id";

// Obfuscated account ID that identifies the current user logged into sync and
// other google services.
const char kGoogleServicesUserAccountId[] = "google.services.user_account_id";

// String that identifies the current user logged into sync and other google
// services.
// DEPRECATED.
const char kGoogleServicesUsername[] = "google.services.username";

// Local state pref containing a string regex that restricts which accounts
// can be used to log in to chrome (e.g. "*@google.com"). If missing or blank,
// all accounts are allowed (no restrictions).
const char kGoogleServicesUsernamePattern[] =
    "google.services.username_pattern";

// List to keep track of emails for which the user has rejected one-click
// sign-in.
const char kReverseAutologinRejectedEmailList[] =
    "reverse_autologin.rejected_email_list";

// Int64 which tracks, as time from epoch, when last time the user signed in
// to the browser.
const char kSignedInTime[] = "signin.signedin_time";

// Boolean which stores if the user is allowed to signin to chrome.
const char kSigninAllowed[] = "signin.allowed";

// True if the token service has been prepared for Dice migration.
const char kTokenServiceDiceCompatible[] = "token_service.dice_compatible";

// Boolean which stores if the OAuth2TokenService should ignore secondary
// accounts.
const char kTokenServiceExcludeAllSecondaryAccounts[] =
    "token_service.exclude_all_secondary_accounts";

// List that identifies the account id that should be ignored by the token
// service.
const char kTokenServiceExcludedSecondaryAccounts[] =
    "token_service.excluded_secondary_accounts";

}  // namespace prefs
