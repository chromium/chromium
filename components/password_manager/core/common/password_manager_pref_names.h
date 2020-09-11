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
// through Credential Manager API.
extern const char kCredentialsEnableAutosignin[];

// The value of this preference controls whether the Password Manager will save
// credentials. When it is false, it doesn't ask if you want to save passwords
// but will continue to fill passwords.
// TODO(melandory): Preference should also control autofill behavior for the
// passwords.
extern const char kCredentialsEnableService[];

#if !defined(OS_APPLE) && !defined(OS_CHROMEOS) && defined(OS_POSIX)
// The current state of the migration to LoginDB from Keyring/Kwallet on Linux.
extern const char kMigrationToLoginDBStep[];
#endif

#if defined(OS_WIN)
// Whether the password was blank, only valid if OS password was last changed
// on or before the value contained in kOsPasswordLastChanged.
extern const char kOsPasswordBlank[];

// The number of seconds since epoch that the OS password was last changed.
extern const char kOsPasswordLastChanged[];
#endif

#if defined(OS_APPLE)
// The current status of migrating the passwords from the Keychain to the
// database. Stores a value from MigrationStatus.
extern const char kKeychainMigrationStatus[];

// The date of when passwords were cleaned up for MacOS users who previously
// lost access to their password because of encryption key modification in
// Keychain.
extern const char kPasswordRecovery[];
#endif

// Boolean that indicated whether first run experience for the auto sign-in
// prompt was shown or not.
extern const char kWasAutoSignInFirstRunExperienceShown[];

// Boolean that indicated if user interacted with the Chrome Sign in promo.
extern const char kWasSignInPasswordPromoClicked[];

// Number of times the Chrome Sign in promo popped up.
extern const char kNumberSignInPasswordPromoShown[];

// True if the counters for the sign in promo were reset for M79.
// Safe to remove for M82.
extern const char kSignInPasswordPromoRevive[];

// A dictionary of account-storage-related settings that exist per Gaia account
// (e.g. whether that user has opted in). It maps from hash of Gaia ID to
// dictionary of key-value pairs.
extern const char kAccountStoragePerAccountSettings[];

// A boolean that tracks whether the account-scoped password store exists on
// disk. When the factory needs to delete the store from disk, it uses this pref
// to only trigger the deletion if the store actually exists.
extern const char kAccountStorageExists[];

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

// List that contains captured password hashes.
extern const char kPasswordHashDataList[];

// Boolean indicating whether Chrome should check whether the credentials
// submitted by the user were part of a leak.
extern const char kPasswordLeakDetectionEnabled[];

// Timestamps of when credentials from the profile / account store were last
// used to fill a form, in microseconds since Windows epoch.
extern const char kProfileStoreDateLastUsedForFilling[];
extern const char kAccountStoreDateLastUsedForFilling[];

// Number of times the check for leaked password has been performed from the
// password settings.
extern const char kSettingsLaunchedPasswordChecks[];

}  // namespace prefs
}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_PREF_NAMES_H_
