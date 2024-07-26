// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_FEATURES_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_FEATURES_H_

// DON'T ADD NEW FEATURES here.
// If the feature belongs logically to the browser process, put it into
// components/password_manager/core/browser/features/password_features.h.

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace password_manager::features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.
BASE_DECLARE_FEATURE(kEnableOverwritingPlaceholderUsernames);

#if BUILDFLAG(IS_IOS)
BASE_DECLARE_FEATURE(kIOSPasswordBottomSheetAutofocus);
#endif  // IS_IOS
BASE_DECLARE_FEATURE(kPasswordReuseDetectionEnabled);
BASE_DECLARE_FEATURE(kNoPasswordSuggestionFiltering);
BASE_DECLARE_FEATURE(kShowSuggestionsOnAutofocus);

#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kPasswordSuggestionBottomSheetV2);
BASE_DECLARE_FEATURE(kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
#endif

// All features parameters are in alphabetical order.

#if BUILDFLAG(IS_ANDROID)
// Whether to ignore the 1 month timeout in between migration warning prompts.
// Used for manual testing.
inline constexpr base::FeatureParam<bool> kIgnoreMigrationWarningTimeout = {
    &kUnifiedPasswordManagerLocalPasswordsMigrationWarning,
    "ignore_migration_warning_timeout", false};

extern const base::FeatureParam<int> kLocalPasswordMigrationWarningPrefsVersion;
#endif

// Field trial and corresponding parameters.
// To manually override this, start Chrome with the following parameters:
//   --enable-features=PasswordGenerationRequirements,\
//       PasswordGenerationRequirementsDomainOverrides
//   --force-fieldtrials=PasswordGenerationRequirements/Enabled
//   --force-fieldtrial-params=PasswordGenerationRequirements.Enabled:\
//       version/0/prefix_length/0/timeout/5000
extern const char kGenerationRequirementsFieldTrial[];
extern const char kGenerationRequirementsVersion[];
extern const char kGenerationRequirementsPrefixLength[];
extern const char kGenerationRequirementsTimeout[];

}  // namespace password_manager::features

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_FEATURES_H_
