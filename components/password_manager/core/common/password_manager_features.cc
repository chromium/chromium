// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/common/password_manager_features.h"

namespace password_manager {

namespace features {

// Enable affiliation based matching, so that credentials stored for an Android
// application will also be considered matches for, and be filled into
// corresponding Web applications.
const base::Feature kAffiliationBasedMatching = {
    "AffiliationBasedMatching", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables links to the setting pages from the Chrome profile menu for Passwords
// and Autofill.
const base::Feature kAutofillHome = {"AutofillHome",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// Recovers lost passwords on Mac by deleting the ones that cannot be decrypted
// with the present encryption key from the Keychain.
const base::Feature kDeleteCorruptedPasswords = {
    "DeleteCorruptedPasswords", base::FEATURE_DISABLED_BY_DEFAULT};

// Use HTML based username detector.
const base::Feature kHtmlBasedUsernameDetector = {
    "HtmlBaseUsernameDetector", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether password requirements can be overridden for domains
// (as opposed to only relying on the autofill server).
const base::Feature kPasswordGenerationRequirementsDomainOverrides = {
    "PasswordGenerationRequirementsDomainOverrides",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Disallow autofilling of the sync credential.
const base::Feature kProtectSyncCredential = {
    "protect-sync-credential", base::FEATURE_DISABLED_BY_DEFAULT};

// Disallow autofilling of the sync credential only for transactional reauth
// pages.
const base::Feature kProtectSyncCredentialOnReauth = {
    "ProtectSyncCredentialOnReauth", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls the ability to import passwords from Chrome's settings page.
const base::Feature kPasswordImport = {"PasswordImport",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Allows searching for saved passwords in the settings page on mobile devices.
const base::Feature kPasswordSearchMobile = {"PasswordSearchMobile",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Adds password-related features to the keyboard accessory on mobile devices.
const base::Feature kPasswordsKeyboardAccessory = {
    "PasswordsKeyboardAccessory", base::FEATURE_DISABLED_BY_DEFAULT};

// Deletes entries from local database on Mac which cannot be decrypted when
// merging data with Sync.
const base::Feature kRecoverPasswordsForSyncUsers = {
    "RecoverPasswordsForSyncUsers", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the experiment for the password manager to only fill on account
// selection, rather than autofilling on page load, with highlighting of fields.
const base::Feature kFillOnAccountSelect = {"fill-on-account-select",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Enables new password form parsing mechanism for filling passwords, details in
// https://goo.gl/QodPH1
const base::Feature kNewPasswordFormParsing = {
    "new-password-form-parsing", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables new password form parsing mechanism for saving passwords, details in
// https://goo.gl/QodPH1
const base::Feature kNewPasswordFormParsingForSaving = {
    "new-password-form-parsing-for-saving", base::FEATURE_DISABLED_BY_DEFAULT};

// Performs a one-off migration (with retries) from a native backend into
// logindb. Passwords are served from the new location.
const base::Feature kMigrateLinuxToLoginDB = {
    "migrate-linux-to-logindb", base::FEATURE_DISABLED_BY_DEFAULT};

// Field trial identifier for password generation requirements.
const char* kGenerationRequirementsFieldTrial =
    "PasswordGenerationRequirements";

// The file version number of password requirements files. If the prefix length
// changes, this version number needs to be updated.
// Default to 0 in order to get an empty requirements file.
const char* kGenerationRequirementsVersion = "version";

// Length of a hash prefix of domain names. This is used to shard domains
// across multiple files.
// Default to 0 in order to put all domain names into the same shard.
const char* kGenerationRequirementsPrefixLength = "prefix_length";

// Timeout (in milliseconds) for password requirements lookups. As this is a
// network request in the background that does not block the UI, the impact of
// high values is not strong.
// Default to 5000 ms.
const char* kGenerationRequirementsTimeout = "timeout";

}  // namespace features

}  // namespace password_manager
