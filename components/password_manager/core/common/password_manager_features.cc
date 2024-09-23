// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/common/password_manager_features.h"

#include "base/feature_list.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"

namespace password_manager::features {
// NOTE: It is strongly recommended to use UpperCamelCase style for feature
//       names, e.g. "MyGreatFeature".

// Enables the overwriting of prefilled username fields if the server predicted
// the field to contain a placeholder value.
BASE_FEATURE(kEnableOverwritingPlaceholderUsernames,
             "EnableOverwritingPlaceholderUsernames",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_IOS)
// Enables password bottom sheet to be triggered on autofocus events (on iOS).
BASE_FEATURE(kIOSPasswordBottomSheetAutofocus,
             "kIOSPasswordBottomSheetAutofocus",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // IS_IOS

// Enables password reuse detection.
BASE_FEATURE(kPasswordReuseDetectionEnabled,
             "PasswordReuseDetectionEnabled",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Removes password suggestion filtering by username.
BASE_FEATURE(kNoPasswordSuggestionFiltering,
             "NoPasswordSuggestionFiltering",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allows to show suggestions automatically when password forms are autofocused
// on pageload.
BASE_FEATURE(kShowSuggestionsOnAutofocus,
             "ShowSuggestionsOnAutofocus",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// Enables the refactored Password Suggestion bottom sheet (Touch-To-Fill).
// The goal of the refactoring is to transfer the knowledge about the
// Touch-To-Fill feature to the browser code completely and so to simplify the
// renderer code. In the refactored version it will be decided inside the the
// `ContentPasswordManagerDriver::ShowPasswordSuggestions` whether to show the
// TTF to the user.
BASE_FEATURE(kPasswordSuggestionBottomSheetV2,
             "PasswordSuggestionBottomSheetV2",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables showing the warning about UPM migrating local passwords.
// The feature is limited to Canary/Dev/Beta by a check in
// local_passwords_migration_warning_util::ShouldShowWarning.
BASE_FEATURE(kUnifiedPasswordManagerLocalPasswordsMigrationWarning,
             "UnifiedPasswordManagerLocalPasswordsMigrationWarning",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_ANDROID)

// The version of the password migration warning prefs. When the version
// increases, the value of the pref LocalPasswordMigrationWarningPrefsVersion
// increases and the affected prefs are reset. The affected prefs are:
// LocalPasswordMigrationWarningShownAtStartup and
// LocalPasswordsMigrationWarningShownTimestamp.
extern const base::FeatureParam<int>
    kLocalPasswordMigrationWarningPrefsVersion = {
        &kUnifiedPasswordManagerLocalPasswordsMigrationWarning,
        "pwd_migration_warning_prefs_version", 1};
#endif

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

}  // namespace password_manager::features
