// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WELCOME_HELPERS_H_
#define CHROME_BROWSER_UI_WEBUI_WELCOME_HELPERS_H_

#include <string>

#include "base/macros.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace base {
class DictionaryValue;
struct Feature;
}  // namespace base

namespace policy {
class PolicyMap;
}  // namespace policy

class Profile;

namespace welcome {

// Onboarding groups are used for running field trials related to first run
// experience. This will make a new profile join whatever group is currently
// active. Any profile that is already part of an onboarding group will remain
// in that group.
void JoinOnboardingGroup(Profile* profile);

bool IsEnabled(Profile* profile);

bool IsAppVariationEnabled();

bool HasModulesToShow(Profile* profile);

base::DictionaryValue GetModules(Profile* profile);

// Exposed for testing.
extern const base::Feature kForceEnabled;

bool CanShowGoogleAppModuleForTesting(const policy::PolicyMap& policies);
bool CanShowNTPBackgroundModuleForTesting(const policy::PolicyMap& policies,
                                          Profile* profile);
bool CanShowSetDefaultModuleForTesting(const policy::PolicyMap& policies);
bool CanShowSigninModuleForTesting(const policy::PolicyMap& policies);

}  // namespace welcome

#endif  // CHROME_BROWSER_UI_WEBUI_WELCOME_HELPERS_H_
