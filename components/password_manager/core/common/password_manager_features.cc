// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/common/password_manager_features.h"

#include "build/build_config.h"

namespace password_manager {

// NOTE: It is strongly recommended to use UpperCamelCase style for feature
//       names, e.g. "MyGreatFeature".
namespace features {

// Enables Biometrics for the Touch To Fill feature. This only effects Android
// and requires autofill::features::kAutofillTouchToFill to be enabled as well.
const base::Feature kBiometricTouchToFill = {"BiometricTouchToFill",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Enables creating Affiliation Service and prefetching change password info for
// requested sites.
const base::Feature kChangePasswordAffiliationInfo = {
    "ChangePasswordAffiliationInfo", base::FEATURE_DISABLED_BY_DEFAULT};

// After saving/updating a password show a bubble reminder about the status of
// other compromised credentials.
const base::Feature kCompromisedPasswordsReengagement = {
    "CompromisedPasswordsReengagement", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the editing of passwords in Chrome settings.
const base::Feature kEditPasswordsInSettings = {
    "EditPasswordsInSettings", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the overwriting of prefilled username fields if the server predicted
// the field to contain a placeholder value.
const base::Feature kEnableOverwritingPlaceholderUsernames{
    "EnableOverwritingPlaceholderUsernames", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables a second, Gaia-account-scoped password store for users who are signed
// in but not syncing.
const base::Feature kEnablePasswordsAccountStorage = {
    "EnablePasswordsAccountStorage", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature KEnablePasswordGenerationForClearTextFields = {
    "EnablePasswordGenerationForClearTextFields",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables showing UI button in password fallback sheet.
// The button opens a different sheet that allows filling a password from any
// origin.
const base::Feature kFillingPasswordsFromAnyOrigin{
    "FillingPasswordsFromAnyOrigin", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the experiment for the password manager to only fill on account
// selection, rather than autofilling on page load, with highlighting of fields.
const base::Feature kFillOnAccountSelect = {"fill-on-account-select",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Enables password change flow from leaked password dialog.
const base::Feature kPasswordChange = {"PasswordChange",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Enables password change flow from bulk leak check in settings.
const base::Feature kPasswordChangeInSettings = {
    "PasswordChangeInSettings", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the bulk Password Check feature for signed in users.
const base::Feature kPasswordCheck = {"PasswordCheck",
#if defined(OS_ANDROID) || defined(OS_IOS)
                                      base::FEATURE_DISABLED_BY_DEFAULT
#else
                                      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Controls the ability to import passwords from Chrome's settings page.
const base::Feature kPasswordImport = {"PasswordImport",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Enables password scripts fetching for the |PasswordChangeInSettings| feature.
const base::Feature kPasswordScriptsFetching = {
    "PasswordScriptsFetching", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables checking credentials for weakness in Password Check.
const base::Feature kPasswordsWeaknessCheck = {
    "PasswordsWeaknessCheck", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables showing UI which allows users to easily revert their choice to
// never save passwords on a certain website.
const base::Feature kRecoverFromNeverSaveAndroid = {
    "RecoverFromNeverSaveAndroid", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables support of filling and saving on username first flow.
const base::Feature kUsernameFirstFlow = {"UsernameFirstFlow",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enable support for .well-known/change-password URLs.
const base::Feature kWellKnownChangePassword = {
    "WellKnownChangePassword", base::FEATURE_DISABLED_BY_DEFAULT};

// Field trial identifier for password generation requirements.
const char kGenerationRequirementsFieldTrial[] =
    "PasswordGenerationRequirements";

// The file version number of password requirements files. If the prefix length
// changes, this version number needs to be updated.
// Default to 0 in order to get an empty requirements file.
const char kGenerationRequirementsVersion[] = "version";

// Length of a hash prefix of domain names. This is used to shard domains
// across multiple files.
// Default to 0 in order to put all domain names into the same shard.
const char kGenerationRequirementsPrefixLength[] = "prefix_length";

// Timeout (in milliseconds) for password requirements lookups. As this is a
// network request in the background that does not block the UI, the impact of
// high values is not strong.
// Default to 5000 ms.
const char kGenerationRequirementsTimeout[] = "timeout";

// Enables showing leaked dialog after every successful form submission.
const char kPasswordChangeWithForcedDialogAfterEverySuccessfulSubmission[] =
    "should_force_dialog_after_every_sucessful_form_submission";

// Enables showing leaked warning for every site while doing bulk leak check in
// settings.
const char kPasswordChangeInSettingsWithForcedWarningForEverySite[] =
    "should_force_warning_for_every_site_in_settings";

// Number of times the user can refuse an offer to move a password to the
// account before Chrome stops offering this flow. Only applies to users who
// haven't gone through the opt-in flow for passwords account storage.
const char kMaxMoveToAccountOffersForNonOptedInUser[] =
    "max_move_to_account_offers_for_non_opted_in_user";

const int kMaxMoveToAccountOffersForNonOptedInUserDefaultValue = 5;

}  // namespace features

}  // namespace password_manager
