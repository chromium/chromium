// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome/helpers.h"

#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/policy/browser_signin_policy_handler.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search/ntp_features.h"
#include "chrome/browser/search/search.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"

namespace welcome {

// Available modules for both new and returning users.
const char kDefaultNewUserModules[] =
    "nux-google-apps,nux-ntp-background,nux-set-as-default,signin-view";
const char kDefaultReturningUserModules[] = "nux-set-as-default";

// Feature flag.
const base::Feature kFeature{"NuxOnboarding", base::FEATURE_ENABLED_BY_DEFAULT};
// For testing purposes
const base::Feature kForceEnabled = {"NuxOnboardingForceEnabled",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// The value of these FeatureParam values should be a comma-delimited list
// of element names whitelisted in the MODULES_WHITELIST list, defined in
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
  const base::Value* policy = policies.GetValue(policy_name);
  return policy && policy->is_bool() && !policy->GetBool();
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
  return !policies.GetValue(policy::key::kNewTabPageLocation) &&
         search::DefaultSearchProviderIsGoogle(profile);
}

bool CanShowSetDefaultModule(const policy::PolicyMap& policies) {
  if (IsPolicySetAndFalse(policies, policy::key::kDefaultBrowserSettingEnabled))
    return false;

  return true;
}

bool CanShowSigninModule(const policy::PolicyMap& policies) {
  const base::Value* browser_signin_value =
      policies.GetValue(policy::key::kBrowserSignin);

  if (!browser_signin_value)
    return true;

  int int_browser_signin_value;
  bool success = browser_signin_value->GetAsInteger(&int_browser_signin_value);
  DCHECK(success);

  return static_cast<policy::BrowserSigninMode>(int_browser_signin_value) !=
         policy::BrowserSigninMode::kDisabled;
}

// Welcome experiments depend on Google being the default search provider.
static bool CanExperimentWithVariations(Profile* profile) {
  return search::DefaultSearchProviderIsGoogle(profile);
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && defined(OS_WIN)
// These feature flags are used to tie our experiment to specific studies.
// go/navi-app-variation for details.
// TODO(hcarmona): find a solution that scales better.
const base::Feature kNaviControlEnabled = {"NaviControlEnabled",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kNaviAppVariationEnabled = {
    "NaviAppVariationEnabled", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kNaviNTPVariationEnabled = {
    "NaviNTPVariationEnabled", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kNaviShortcutVariationEnabled = {
    "NaviShortcutVariationEnabled", base::FEATURE_DISABLED_BY_DEFAULT};

// Get the group for users who onboard in this experiment.
// Groups are:
//   - Specified by study
//   - The same for all experiments in study
//   - Incremented with each new version
//   - Not reused
static std::string GetOnboardingGroup(Profile* profile) {
  if (!CanExperimentWithVariations(profile)) {
    // If we cannot run any variations, we bucket the users into a separate
    // synthetic group that we will ignore data for.
    return "NaviNoVariationSynthetic";
  }

  // We need to use |base::GetFieldTrialParamValue| instead of
  // |base::FeatureParam| because our control group needs a custom value for
  // this param.
  // "NaviOnboarding" match study name in configs.
  return base::GetFieldTrialParamValue("NaviOnboarding", "onboarding-group");
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) && defined(OS_WIN)

void JoinOnboardingGroup(Profile* profile) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && defined(OS_WIN)
  PrefService* prefs = profile->GetPrefs();

  std::string group;
  if (prefs->GetBoolean(prefs::kHasSeenWelcomePage)) {
    // Get user's original group.
    group = prefs->GetString(prefs::kNaviOnboardGroup);

    // Users who onboarded before Navi won't have a group.
    if (group.empty())
      return;
  } else {
    // Join the latest group if onboarding for the first time!
    group = GetOnboardingGroup(profile);
    profile->GetPrefs()->SetString(prefs::kNaviOnboardGroup, group);
  }

  // User will be tied to their original group, even after experiment ends.
  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      "NaviOnboardingSynthetic", group);

  // Check for feature based on group.
  // TODO(hcarmona): find a solution that scales better.
  if (group.compare("ControlSynthetic-008") == 0)
    base::FeatureList::IsEnabled(kNaviControlEnabled);
  else if (group.compare("AppVariationSynthetic-008") == 0)
    base::FeatureList::IsEnabled(kNaviAppVariationEnabled);
  else if (group.compare("NTPVariationSynthetic-008") == 0)
    base::FeatureList::IsEnabled(kNaviNTPVariationEnabled);
  else if (group.compare("ShortcutVariationSynthetic-008") == 0)
    base::FeatureList::IsEnabled(kNaviShortcutVariationEnabled);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) && defined(OS_WIN)
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

bool HasModulesToShow(Profile* profile) {
  // Modules won't have lasting effect if profile is ephemeral, so we can skip.
  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry = nullptr;
  if (storage.GetProfileAttributesWithPath(profile->GetPath(), &entry) &&
      entry->IsEphemeral()) {
    return false;
  }

  return !GetAvailableModules(profile).empty();
}

std::string FilterModules(const std::string& requested_modules,
                          const std::vector<std::string>& available_modules) {
  std::vector<std::string> requested_list = base::SplitString(
      requested_modules, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::vector<std::string> filtered_modules;

  std::copy_if(requested_list.begin(), requested_list.end(),
               std::back_inserter(filtered_modules),
               [available_modules](std::string module) {
                 return !module.empty() &&
                        base::Contains(available_modules, module);
               });

  return base::JoinString(filtered_modules, ",");
}

base::DictionaryValue GetModules(Profile* profile) {
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

  base::DictionaryValue modules;
  modules.SetString("new-user",
                    FilterModules(new_user_modules, available_modules));
  modules.SetString("returning-user",
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
