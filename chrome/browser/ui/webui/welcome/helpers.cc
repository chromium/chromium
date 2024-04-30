// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome/helpers.h"

#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/policy/browser_signin_policy_handler.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"

namespace welcome {

// Available modules for both new and returning users.
const char kDefaultNewUserModules[] =
    "nux-google-apps,nux-ntp-background,nux-set-as-default,signin-view";
const char kDefaultReturningUserModules[] = "nux-set-as-default";

// Feature flag.
BASE_FEATURE(kFeature, "NuxOnboarding", base::FEATURE_ENABLED_BY_DEFAULT);
// For testing purposes
BASE_FEATURE(kForceEnabled,
             "NuxOnboardingForceEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The value of these FeatureParam values should be a comma-delimited list
// of element names allowlisted in the MODULES_WHITELIST list, defined in
// chrome/browser/resources/welcome/welcome_app.js
const base::FeatureParam<std::string> kNewUserModules{
    &kFeature, "new-user-modules", kDefaultNewUserModules};
const base::FeatureParam<std::string> kReturningUserModules{
    &kFeature, "returning-user-modules", kDefaultReturningUserModules};
// For testing purposes
const base::FeatureParam<std::string> kForceEnabledNewUserModules = {
    &kForceEnabled, "new-user-modules", kDefaultNewUserModules};
const base::FeatureParam<std::string> kForceEnabledReturningUserModules = {
    &kForceEnabled, "returning-user-modules", kDefaultReturningUserModules};

// FeatureParam for app variation.
const base::FeatureParam<bool> kShowGoogleApp{&kFeature,
                                              "app-variation-enabled", false};
// For testing purposes
const base::FeatureParam<bool> kForceEnabledShowGoogleApp = {
    &kForceEnabled, "app-variation-enabled", false};

bool IsPolicySetAndFalse(const policy::PolicyMap& policies,
                         const std::string& policy_name) {
  const base::Value* policy =
      policies.GetValue(policy_name, base::Value::Type::BOOLEAN);
  return policy && !policy->GetBool();
}

bool CanShowGoogleAppModule(const policy::PolicyMap& policies) {
  if (IsPolicySetAndFalse(policies, policy::key::kBookmarkBarEnabled))
    return false;

  if (IsPolicySetAndFalse(policies, policy::key::kEditBookmarksEnabled))
    return false;

  return true;
}

bool CanShowNTPBackgroundModule(const policy::PolicyMap& policies,
                                Profile* profile) {
  // We can't set the background if the NTP is something other than Google.
  return !policies.GetValue(policy::key::kNewTabPageLocation,
                            base::Value::Type::STRING) &&
         search::DefaultSearchProviderIsGoogle(profile);
}

bool CanShowSetDefaultModule(const policy::PolicyMap& policies) {
  if (IsPolicySetAndFalse(policies, policy::key::kDefaultBrowserSettingEnabled))
    return false;

  return true;
}

bool CanShowSigninModule(const policy::PolicyMap& policies) {
  const base::Value* browser_signin_value = policies.GetValue(
      policy::key::kBrowserSignin, base::Value::Type::INTEGER);

  if (!browser_signin_value)
    return true;

  return static_cast<policy::BrowserSigninMode>(
             browser_signin_value->GetInt()) !=
         policy::BrowserSigninMode::kDisabled;
}

// Welcome experiments depend on Google being the default search provider.
static bool CanExperimentWithVariations(Profile* profile) {
  return search::DefaultSearchProviderIsGoogle(profile);
}

bool IsEnabled(Profile* profile) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return base::FeatureList::IsEnabled(welcome::kFeature) ||
         base::FeatureList::IsEnabled(welcome::kForceEnabled);
#else
  // Allow enabling outside official builds for testing purposes.
  return base::FeatureList::IsEnabled(welcome::kForceEnabled);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

bool IsAppVariationEnabled() {
  return kForceEnabledShowGoogleApp.Get() || kShowGoogleApp.Get();
}

const policy::PolicyMap& GetPoliciesFromProfile(Profile* profile) {
  policy::ProfilePolicyConnector* profile_connector =
      profile->GetProfilePolicyConnector();
  DCHECK(profile_connector);
  return profile_connector->policy_service()->GetPolicies(
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()));
}

std::vector<std::string> GetAvailableModules(Profile* profile) {
  const policy::PolicyMap& policies = GetPoliciesFromProfile(profile);
  std::vector<std::string> available_modules;

  if (CanShowGoogleAppModule(policies))
    available_modules.push_back("nux-google-apps");
  if (CanShowNTPBackgroundModule(policies, profile))
    available_modules.push_back("nux-ntp-background");
  if (CanShowSetDefaultModule(policies))
    available_modules.push_back("nux-set-as-default");
  if (CanShowSigninModule(policies))
    available_modules.push_back("signin-view");

  return available_modules;
}

std::string FilterModules(const std::string& requested_modules,
                          const std::vector<std::string>& available_modules) {
  std::vector<std::string> filtered_modules;
  base::ranges::copy_if(
      base::SplitString(requested_modules, ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY),
      std::back_inserter(filtered_modules),
      [&available_modules](const std::string& module) {
        return !module.empty() && base::Contains(available_modules, module);
      });
  return base::JoinString(filtered_modules, ",");
}

base::Value::Dict GetModules(Profile* profile) {
  // This function should not be called when feature is not on.
  DCHECK(welcome::IsEnabled(profile));

  std::string new_user_modules = kDefaultNewUserModules;
  std::string returning_user_modules = kDefaultReturningUserModules;

  if (base::FeatureList::IsEnabled(welcome::kForceEnabled)) {
    new_user_modules = kForceEnabledNewUserModules.Get();
    returning_user_modules = kForceEnabledReturningUserModules.Get();
  } else if (CanExperimentWithVariations(profile)) {
    new_user_modules = kNewUserModules.Get();
    returning_user_modules = kReturningUserModules.Get();
  }

  std::vector<std::string> available_modules = GetAvailableModules(profile);

  base::Value::Dict modules;
  modules.Set("new-user", FilterModules(new_user_modules, available_modules));
  modules.Set("returning-user",
              FilterModules(returning_user_modules, available_modules));
  return modules;
}

bool CanShowGoogleAppModuleForTesting(const policy::PolicyMap& policies) {
  return CanShowGoogleAppModule(policies);
}

bool CanShowNTPBackgroundModuleForTesting(const policy::PolicyMap& policies,
                                          Profile* profile) {
  return CanShowNTPBackgroundModule(policies, profile);
}

bool CanShowSetDefaultModuleForTesting(const policy::PolicyMap& policies) {
  return CanShowSetDefaultModule(policies);
}

bool CanShowSigninModuleForTesting(const policy::PolicyMap& policies) {
  return CanShowSigninModule(policies);
}
}  // namespace welcome
