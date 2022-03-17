// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_PREF_NAMES_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_PREF_NAMES_H_

#include "build/build_config.h"

namespace password_manager {
namespace prefs {

// Alphabetical list of preference names specific to the PasswordManager
// component.

// Boolean controlling whether the password manager allows automatic signing in
// through Credential Management API.
extern const char kCredentialsEnableAutosignin[];

// The value of this preference controls whether the Password Manager will save
// credentials. When it is false, it doesn't ask if you want to save passwords
// but will continue to fill passwords.
// TODO(melandory): Preference should also control autofill behavior for the
// passwords.
extern const char kCredentialsEnableService[];

#if BUILDFLAG(IS_ANDROID)
// Integer value which indicates the version used to migrate passwords from
// built in storage to Google Mobile Services.
extern const char kCurrentMigrationVersionToGoogleMobileServices[];

// Timestamps of when credentials from the GMS Core to the built in storage were
// last time migrated, in microseconds since Windows epoch.
extern const char kTimeOfLastMigrationAttempt[];
#endif

#if BUILDFLAG(IS_WIN)
// Whether the password was blank, only valid if OS password was last changed
// on or before the value contained in kOsPasswordLastChanged.
extern const char kOsPasswordBlank[];

// The number of seconds since epoch that the OS password was last changed.
extern const char kOsPasswordLastChanged[];
#endif

#if BUILDFLAG(IS_APPLE)
// The current status of migrating the passwords from the Keychain to the
// database. Stores a value from MigrationStatus.
extern const char kKeychainMigrationStatus[];
#endif

// Boolean that indicated whether first run experience for the auto sign-in
// prompt was shown or not.
extern const char kWasAutoSignInFirstRunExperienceShown[];

// Boolean that indicated whether one time removal of old google.com logins was
// performed.
extern const char kWereOldGoogleLoginsRemoved[];

// A dictionary of account-storage-related settings that exist per Gaia account
// (e.g. whether that user has opted in). It maps from hash of Gaia ID to
// dictionary of key-value pairs.
extern const char kAccountStoragePerAccountSettings[];

// String that represents the sync password hash.
extern const char kSyncPasswordHash[];

// String that represents the sync password length and salt. Its format is
// encrypted and converted to base64 string "<password length, as ascii
// int>.<16 char salt>".
extern const char kSyncPasswordLengthAndHashSalt[];

// Indicates the time (in seconds) when last cleaning of obsolete HTTP
// credentials was performed.
extern const char kLastTimeObsoleteHttpCredentialsRemoved[];

// The last time the password check has run to completion.
extern const char kLastTimePasswordCheckCompleted[];

// Timestamps of when password store metrics where last reported, in
// microseconds since Windows epoch.
extern const char kLastTimePasswordStoreMetricsReported[];

// The last time the password check has run to completion synced across devices.
// It's used on passwords.google.com and not in Chrome.
extern const char kSyncedLastTimePasswordCheckCompleted[];

// List that contains captured password hashes.
extern const char kPasswordHashDataList[];

// Boolean indicating whether Chrome should check whether the credentials
// submitted by the user were part of a leak.
extern const char kPasswordLeakDetectionEnabled[];

// Boolean indicating whether users can mute (aka dismiss) alerts resulting from
// compromised credentials that were submitted by the user.
extern const char kPasswordDismissCompromisedAlertEnabled[];

// Timestamps of when credentials from the profile / account store were last
// used to fill a form, in microseconds since Windows epoch.
extern const char kProfileStoreDateLastUsedForFilling[];
extern const char kAccountStoreDateLastUsedForFilling[];

}  // namespace prefs
}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_PREF_NAMES_H_
