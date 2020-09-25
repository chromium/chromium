// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_FEATURES_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_FEATURES_H_

// This file defines all the base::FeatureList features for the Password Manager
// module.

#include "base/feature_list.h"

namespace password_manager {

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

extern const base::Feature kBiometricTouchToFill;
extern const base::Feature kChangePasswordAffiliationInfo;
extern const base::Feature kCompromisedPasswordsReengagement;
extern const base::Feature kEditPasswordsInSettings;
extern const base::Feature kEnableOverwritingPlaceholderUsernames;
extern const base::Feature kEnablePasswordsAccountStorage;
extern const base::Feature KEnablePasswordGenerationForClearTextFields;
extern const base::Feature kFillingPasswordsFromAnyOrigin;
extern const base::Feature kFillOnAccountSelect;
extern const base::Feature kPasswordChange;
extern const base::Feature kPasswordChangeInSettings;
extern const base::Feature kPasswordCheck;
extern const base::Feature kPasswordImport;
extern const base::Feature kPasswordScriptsFetching;
extern const base::Feature kPasswordsWeaknessCheck;
extern const base::Feature kRecoverFromNeverSaveAndroid;
extern const base::Feature kUsernameFirstFlow;
extern const base::Feature kWellKnownChangePassword;

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

// Password change feature variations.
extern const char
    kPasswordChangeWithForcedDialogAfterEverySuccessfulSubmission[];
extern const char kPasswordChangeInSettingsWithForcedWarningForEverySite[];

// |kEnablePasswordAccountStorage| variations.
extern const char kMaxMoveToAccountOffersForNonOptedInUser[];
extern const int kMaxMoveToAccountOffersForNonOptedInUserDefaultValue;

}  // namespace features

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_FEATURES_H_
