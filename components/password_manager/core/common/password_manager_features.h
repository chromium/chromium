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

extern const base::Feature kAffiliationBasedMatching;
extern const base::Feature kAutofillHome;
extern const base::Feature kDeleteCorruptedPasswords;
extern const base::Feature kHtmlBasedUsernameDetector;
extern const base::Feature kPasswordGenerationRequirementsDomainOverrides;
extern const base::Feature kFillOnAccountSelect;
extern const base::Feature kMigrateLinuxToLoginDB;
extern const base::Feature kNewPasswordFormParsing;
extern const base::Feature kNewPasswordFormParsingForSaving;
extern const base::Feature kPasswordImport;
extern const base::Feature kPasswordSearchMobile;
extern const base::Feature kPasswordsKeyboardAccessory;
extern const base::Feature kProtectSyncCredential;
extern const base::Feature kProtectSyncCredentialOnReauth;
extern const base::Feature kRecoverPasswordsForSyncUsers;

// Field trial and corresponding parameters.
// To manually override this, start Chrome with the following parameters:
//   --enable-features=PasswordGenerationRequirements,\
//       PasswordGenerationRequirementsDomainOverrides
//   --force-fieldtrials=PasswordGenerationRequirements/Enabled
//   --force-fieldtrial-params=PasswordGenerationRequirements.Enabled:\
//       version/0/prefix_length/0/timeout/5000
extern const char* kGenerationRequirementsFieldTrial;
extern const char* kGenerationRequirementsVersion;
extern const char* kGenerationRequirementsPrefixLength;
extern const char* kGenerationRequirementsTimeout;

}  // namespace features

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_FEATURES_H_
