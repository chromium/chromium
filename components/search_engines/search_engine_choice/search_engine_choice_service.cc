// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"

#include <memory>

#include "base/callback_list.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/threading/thread_restrictions.h"
#include "base/version.h"
#include "build/chromeos_buildflags.h"
#include "components/country_codes/country_codes.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/eea_countries_ids.h"
#include "components/search_engines/search_engine_choice_utils.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/version_info/version_info.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/search_engines/android/jni_headers/SearchEngineChoiceService_jni.h"
#endif

namespace search_engines {
namespace {

#if !(BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || \
      BUILDFLAG(CHROME_FOR_TESTING))
// The choice screen should be shown if the `DefaultSearchProviderEnabled`
// policy is not set, or set to true and the
// `DefaultSearchProviderSearchURL` policy is not set.
bool IsSearchEngineChoiceScreenAllowedByPolicy(
    const policy::PolicyService& policy_service) {
  const auto& policies = policy_service.GetPolicies(
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()));

  const auto* default_search_provider_enabled = policies.GetValue(
      policy::key::kDefaultSearchProviderEnabled, base::Value::Type::BOOLEAN);
  // Policy is not set.
  if (!default_search_provider_enabled) {
    return true;
  }

  if (default_search_provider_enabled->GetBool()) {
    const auto* default_search_provider_search_url =
        policies.GetValue(policy::key::kDefaultSearchProviderSearchURL,
                          base::Value::Type::STRING);
    if (!default_search_provider_search_url) {
      return true;
    }
  }
  return false;
}

bool IsDefaultSearchProviderSetOrBlockedByPolicy(
    const TemplateURLService& template_url_service) {
  const TemplateURL* default_search_engine =
      template_url_service.GetDefaultSearchProvider();

  return !default_search_engine ||
         default_search_engine->created_by_policy() ==
             TemplateURLData::CreatedByPolicy::kDefaultSearchProvider;
}
#endif

SearchEngineType GetDefaultSearchEngineType(
    TemplateURLService& template_url_service) {
  const TemplateURL* default_search_engine =
      template_url_service.GetDefaultSearchProvider();

  return default_search_engine ? default_search_engine->GetEngineType(
                                     template_url_service.search_terms_data())
                               : SEARCH_ENGINE_OTHER;
}

// Returns true if all search engine choice prefs are set.
bool IsSearchEngineChoiceCompleted(const PrefService& prefs) {
  return prefs.HasPrefPath(
             prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp) &&
         prefs.HasPrefPath(
             prefs::kDefaultSearchProviderChoiceScreenCompletionVersion);
}

// Returns true if the version is valid and can be compared to the current
// Chrome version.
bool IsValidVersionFormat(const base::Version& version) {
  if (!version.IsValid()) {
    return false;
  }

  // The version should have the same number of components as the current Chrome
  // version.
  if (version.components().size() !=
      version_info::GetVersion().components().size()) {
    return false;
  }
  return true;
}

// Logs the outcome of a reprompt attempt for a specific key (either a specific
// country or the wildcard).
void LogSearchRepromptKeyHistograms(RepromptResult result, bool is_wildcard) {
  // `RepromptResult::kInvalidDictionary` is recorded separately.
  CHECK_NE(result, RepromptResult::kInvalidDictionary);

  base::UmaHistogramEnumeration(kSearchEngineChoiceRepromptHistogram, result);
  if (is_wildcard) {
    base::UmaHistogramEnumeration(kSearchEngineChoiceRepromptWildcardHistogram,
                                  result);
  } else {
    base::UmaHistogramEnumeration(
        kSearchEngineChoiceRepromptSpecificCountryHistogram, result);
  }
}

using NativeCallbackType = base::OnceCallback<void(int)>;

}  // namespace

SearchEngineChoiceService::SearchEngineChoiceService(PrefService& profile_prefs,
                                                     int variations_country_id)
    : profile_prefs_(profile_prefs),
      variations_country_id_(variations_country_id) {}

SearchEngineChoiceService::~SearchEngineChoiceService() = default;

bool SearchEngineChoiceService::ShouldShowUpdatedSettings() {
#if BUILDFLAG(IS_IOS)
  // TODO(b/318820137): There should not be a dependency on the country here.
  if (!IsEeaChoiceCountry(GetCountryId())) {
    return false;
  }
#endif
  return IsChoiceScreenFlagEnabled(ChoicePromo::kAny);
}

#if BUILDFLAG(IS_IOS)
bool SearchEngineChoiceService::ShouldShowChoiceScreen(
    const policy::PolicyService& policy_service,
    bool is_regular_profile,
    TemplateURLService* template_url_service) {
  PreprocessPrefsForReprompt();
  auto condition = GetStaticChoiceScreenConditions(
      policy_service, is_regular_profile, CHECK_DEREF(template_url_service));

  if (condition == SearchEngineChoiceScreenConditions::kEligible) {
    condition = GetDynamicChoiceScreenConditions(*template_url_service);
  }

  RecordChoiceScreenProfileInitCondition(condition);
  return condition == SearchEngineChoiceScreenConditions::kEligible;
}
#endif

SearchEngineChoiceScreenConditions
SearchEngineChoiceService::GetStaticChoiceScreenConditions(
    const policy::PolicyService& policy_service,
    bool is_regular_profile,
    const TemplateURLService& template_url_service) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || \
    BUILDFLAG(CHROME_FOR_TESTING)
  // TODO(b/319050536): Remove the function declaration on these platforms.
  return SearchEngineChoiceScreenConditions::kUnsupportedBrowserType;
#else
  if (!IsChoiceScreenFlagEnabled(ChoicePromo::kAny)) {
    return SearchEngineChoiceScreenConditions::kFeatureSuppressed;
  }

#if !BUILDFLAG(IS_IOS)
  // `prefs::kDefaultSearchProviderChoicePending` does not get set on
  // iOS. Instead, the iOS-specific wrapper
  // `ShouldDisplaySearchEngineChoiceScreen()` handles checking whether
  // the screen should be displayed based on the promo type.
  if (switches::kSearchEngineChoiceTriggerForTaggedProfilesOnly.Get() &&
      !profile_prefs_->GetBoolean(prefs::kDefaultSearchProviderChoicePending)) {
    return SearchEngineChoiceScreenConditions::kProfileOutOfScope;
  }
#endif

  if (!is_regular_profile) {
    // Naming not exactly accurate, but still reflect the fact that incognito,
    // kiosk, etc. are not supported and belongs in this bucked more than in
    // `kProfileOutOfScope` for example.
    return SearchEngineChoiceScreenConditions::kUnsupportedBrowserType;
  }

  base::CommandLine* const command_line =
      base::CommandLine::ForCurrentProcess();
  // A command line argument with the option for disabling the choice screen for
  // testing and automation environments.
  if (command_line->HasSwitch(switches::kDisableSearchEngineChoiceScreen)) {
    return SearchEngineChoiceScreenConditions::kFeatureSuppressed;
  }

  // Force triggering the choice screen for testing the screen itself.
  if (command_line->HasSwitch(switches::kForceSearchEngineChoiceScreen)) {
    return SearchEngineChoiceScreenConditions::kEligible;
  }

  if (IsSearchEngineChoiceCompleted(*profile_prefs_)) {
    return SearchEngineChoiceScreenConditions::kAlreadyCompleted;
  }

  int country_id = GetCountryId();
  DVLOG(1) << "Checking country for choice screen, found: "
           << country_codes::CountryIDToCountryString(country_id);
  if (!IsEeaChoiceCountry(country_id)) {
    return SearchEngineChoiceScreenConditions::kNotInRegionalScope;
  }

  // Initially exclude users with this type of override. Consult b/302675777 for
  // next steps.
  if (profile_prefs_->HasPrefPath(prefs::kSearchProviderOverrides)) {
    return SearchEngineChoiceScreenConditions::kSearchProviderOverride;
  }

  if (!IsSearchEngineChoiceScreenAllowedByPolicy(policy_service) ||
      IsDefaultSearchProviderSetOrBlockedByPolicy(template_url_service)) {
    return SearchEngineChoiceScreenConditions::kControlledByPolicy;
  }

  return SearchEngineChoiceScreenConditions::kEligible;
#endif
}

SearchEngineChoiceScreenConditions
SearchEngineChoiceService::GetDynamicChoiceScreenConditions(
    const TemplateURLService& template_url_service) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || \
    BUILDFLAG(CHROME_FOR_TESTING)
  // TODO(b/319050536): Remove the function declaration on these platforms.
  return SearchEngineChoiceScreenConditions::kUnsupportedBrowserType;
#else
  // Don't show the dialog if the default search engine is set by an extension.
  if (template_url_service.IsExtensionControlledDefaultSearch()) {
    return SearchEngineChoiceScreenConditions::kExtensionControlled;
  }

  if (IsDefaultSearchProviderSetOrBlockedByPolicy(template_url_service)) {
    return SearchEngineChoiceScreenConditions::kControlledByPolicy;
  }

  const TemplateURL* default_search_engine =
      template_url_service.GetDefaultSearchProvider();
  if (!default_search_engine) {
    // It is possible to not have a default search provider if the
    // "DefaultSearchProviderEnabled" policy is set to `false`.
    // It is somewhat that we could reach this, as
    // `GetStaticChoiceScreenConditions()` should already check for that.
    // Hypothetically, a race condition between a policy getting newly
    // downloaded and the user making their choice on the dialog could trigger
    // this (But not at profile creation, we wait for policies to finish
    // applying before proceeding to the choice screen).
    // If we proceeded here, the choice screen could be shown and we might
    // attempt to set a DSE based on the user selection, but that would be
    // ignored.
    return SearchEngineChoiceScreenConditions::kControlledByPolicy;
  }

  if (!template_url_service.IsPrepopulatedOrDefaultProviderByPolicy(
          default_search_engine)) {
    return SearchEngineChoiceScreenConditions::kHasCustomSearchEngine;
  }

  if (default_search_engine->prepopulate_id() >
      TemplateURLPrepopulateData::kMaxPrepopulatedEngineID) {
    // Don't show a choice screen when the user has a distribution custom search
    // engine as default (they have prepopulate ID > 1000).
    // TODO(crbug.com/324880292): Revisit how those are handled.
    return SearchEngineChoiceScreenConditions::
        kHasDistributionCustomSearchEngine;
  }

  if (!TemplateURLPrepopulateData::GetPrepopulatedEngineFromFullList(
          &profile_prefs_.get(), this,
          default_search_engine->prepopulate_id())) {
    // The current default search engine was at some point part of the
    // prepopulated data (it has a "normal"-looking ID), but it has since been
    // removed. Follow what we do for custom search engines, don't show the
    // choice screen.
    RecordUnexpectedSearchProvider(default_search_engine->data());
    return SearchEngineChoiceScreenConditions::
        kHasRemovedPrepopulatedSearchEngine;
  }

  // Force triggering the choice screen for testing the screen itself.
  // Deliberately checked after the conditions overriding the default search
  // engine with some custom one because they would put the choice screens in
  // some unstable state and they are rather easy to change if we want to
  // re-enable the triggering.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceSearchEngineChoiceScreen)) {
    return SearchEngineChoiceScreenConditions::kEligible;
  }

  if (IsSearchEngineChoiceCompleted(*profile_prefs_)) {
    return SearchEngineChoiceScreenConditions::kAlreadyCompleted;
  }

  return SearchEngineChoiceScreenConditions::kEligible;
#endif
}

int SearchEngineChoiceService::GetCountryId() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSearchEngineChoiceCountry)) {
    return country_codes::CountryStringToCountryID(
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kSearchEngineChoiceCountry));
  }

  bool force_eea_country =
      switches::kSearchEngineChoiceTriggerWithForceEeaCountry.Get();
  if (force_eea_country) {
    // `kSearchEngineChoiceTriggerWithForceEeaCountry` forces the search engine
    // choice country to Belgium.
    return country_codes::CountryStringToCountryID("BE");
  }

  if (!country_id_cache_.has_value()) {
    country_id_cache_ = GetCountryIdInternal();
  }
  return *country_id_cache_;
}

void SearchEngineChoiceService::RecordChoiceMade(
    ChoiceMadeLocation choice_location,
    TemplateURLService* template_url_service) {
  CHECK_NE(choice_location, ChoiceMadeLocation::kOther);

  if (!IsChoiceScreenFlagEnabled(ChoicePromo::kAny)) {
    return;
  }

  // Don't modify the pref if the user is not in the EEA region.
  if (!IsEeaChoiceCountry(GetCountryId())) {
    return;
  }

  // Don't modify the prefs if they were already set.
  if (IsSearchEngineChoiceCompleted(*profile_prefs_)) {
    return;
  }

  RecordChoiceScreenDefaultSearchProviderType(
      GetDefaultSearchEngineType(CHECK_DEREF(template_url_service)));
  profile_prefs_->SetInt64(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp,
      base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds());
  profile_prefs_->SetString(
      prefs::kDefaultSearchProviderChoiceScreenCompletionVersion,
      version_info::GetVersionNumber());

  if (profile_prefs_->HasPrefPath(prefs::kDefaultSearchProviderChoicePending)) {
    DVLOG(1) << "Choice made, removing profile tag.";
    profile_prefs_->ClearPref(prefs::kDefaultSearchProviderChoicePending);
  }
}

void SearchEngineChoiceService::PreprocessPrefsForReprompt() {
  if (!IsChoiceScreenFlagEnabled(ChoicePromo::kAny)) {
    return;
  }

  // If existing prefs are missing or have a wrong format, force a reprompt.
  if (!profile_prefs_->HasPrefPath(
          prefs::kDefaultSearchProviderChoiceScreenCompletionVersion)) {
    WipeSearchEngineChoicePrefs(
        profile_prefs_.get(),
        WipeSearchEngineChoiceReason::kMissingChoiceVersion);
    return;
  }

  base::Version choice_version(profile_prefs_->GetString(
      prefs::kDefaultSearchProviderChoiceScreenCompletionVersion));
  if (!IsValidVersionFormat(choice_version)) {
    WipeSearchEngineChoicePrefs(
        profile_prefs_.get(),
        WipeSearchEngineChoiceReason::kInvalidChoiceVersion);
    return;
  }

  // Check parameters from `switches::kSearchEngineChoiceTriggerRepromptParams`.
  absl::optional<base::Value::Dict> reprompt_params =
      base::JSONReader::ReadDict(
          switches::kSearchEngineChoiceTriggerRepromptParams.Get());
  if (!reprompt_params) {
    // No valid reprompt parameters.
    base::UmaHistogramEnumeration(kSearchEngineChoiceRepromptHistogram,
                                  RepromptResult::kInvalidDictionary);
    return;
  }

  const base::Version& current_version = version_info::GetVersion();
  int country_id = GetCountryId();
  const std::string wildcard_string("*");
  // Explicit country key takes precedence over the wildcard.
  for (const std::string& key :
       {country_codes::CountryIDToCountryString(country_id), wildcard_string}) {
    bool is_wildcard = key == wildcard_string;
    const std::string* reprompt_version_string =
        reprompt_params->FindString(key);
    if (!reprompt_version_string) {
      // No version string for this country. Fallback to the wildcard.
      LogSearchRepromptKeyHistograms(RepromptResult::kNoDictionaryKey,
                                     is_wildcard);
      continue;
    }

    base::Version reprompt_version(*reprompt_version_string);
    if (!IsValidVersionFormat(reprompt_version)) {
      // The version is ill-formatted.
      LogSearchRepromptKeyHistograms(RepromptResult::kInvalidVersion,
                                     is_wildcard);
      break;
    }

    // Do not reprompt if the current version is too old, to avoid endless
    // reprompts.
    if (current_version < reprompt_version) {
      LogSearchRepromptKeyHistograms(RepromptResult::kChromeTooOld,
                                     is_wildcard);
      break;
    }

    if (choice_version >= reprompt_version) {
      // No need to reprompt, the choice is recent enough.
      LogSearchRepromptKeyHistograms(RepromptResult::kRecentChoice,
                                     is_wildcard);
      break;
    }

    // Wipe the choice to force a reprompt.
    LogSearchRepromptKeyHistograms(RepromptResult::kReprompt, is_wildcard);
    WipeSearchEngineChoicePrefs(profile_prefs_.get(),
                                WipeSearchEngineChoiceReason::kReprompt);
    return;
  }
}

int SearchEngineChoiceService::GetCountryIdInternal() {
  // `country_codes::kCountryIDAtInstall` may not be set yet.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  // On Android, ChromeOS and Linux, `country_codes::kCountryIDAtInstall` is
  // computed asynchronously using platform-specific signals, and may not be
  // available yet.
  if (!IsChoiceScreenFlagEnabled(ChoicePromo::kAny)) {
    return country_codes::GetCountryIDFromPrefs(&profile_prefs_.get());
  }

  if (profile_prefs_->HasPrefPath(country_codes::kCountryIDAtInstall)) {
    return profile_prefs_->GetInteger(country_codes::kCountryIDAtInstall);
  }
  // If `country_codes::kCountryIDAtInstall` is not available, attempt to
  // compute it at startup. On success, it is saved to prefs and never changes
  // later. Until then, fall back to `country_codes::GetCurrentCountryID()`.
#if BUILDFLAG(IS_ANDROID)
  // On Android get it from Play API in Java.
  // Usage of `WeakPtr` is crucial here, as `SearchEngineChoiceService` is
  // not guaranteed to be alive when the response from Java arrives.
  auto heap_callback = std::make_unique<NativeCallbackType>(base::BindOnce(
      &SearchEngineChoiceService::ProcessGetCountryResponseFromPlayApi,
      weak_ptr_factory_.GetWeakPtr()));
  // The ownership of the callback on the heap is passed to Java. It will be
  // deleted by JNI_SearchEngineChoiceService_ProcessCountryFromPlayApi.
  Java_SearchEngineChoiceService_requestCountryFromPlayApi(
      base::android::AttachCurrentThread(),
      reinterpret_cast<intptr_t>(heap_callback.release()));
#else  // BUILDFLAG(IS_ANDROID)
  // On ChromeOS and Linux, get it from `VariationsService`, by polling at every
  // startup until it is found.
  if (variations_country_id_ != country_codes::kCountryIDUnknown) {
    profile_prefs_->SetInteger(country_codes::kCountryIDAtInstall,
                               variations_country_id_);
  }
#endif

  // The preference may have been updated, so we need to re-check.
  if (!profile_prefs_->HasPrefPath(country_codes::kCountryIDAtInstall)) {
    // Couldn't get the value from the asynchronous API, fallback to locale.
    return country_codes::GetCurrentCountryID();
  }
  return profile_prefs_->GetInteger(country_codes::kCountryIDAtInstall);

#else
  // On other platforms, `country_codes::kCountryIDAtInstall` is computed
  // synchronously inside `country_codes::GetCountryIDFromPrefs()`.
  return country_codes::GetCountryIDFromPrefs(&profile_prefs_.get());
#endif
}

#if BUILDFLAG(IS_ANDROID)
void SearchEngineChoiceService::ProcessGetCountryResponseFromPlayApi(
    int country_id) {
  profile_prefs_->SetInteger(country_codes::kCountryIDAtInstall, country_id);
}
#endif

}  // namespace search_engines

#if BUILDFLAG(IS_ANDROID)
void JNI_SearchEngineChoiceService_ProcessCountryFromPlayApi(
    JNIEnv* env,
    jlong ptr_to_native_callback,
    const base::android::JavaParamRef<jstring>& j_device_country) {
  // Using base::WrapUnique ensures that the callback is deleted when this goes
  // out of scope.
  std::unique_ptr<search_engines::NativeCallbackType> heap_callback =
      base::WrapUnique(reinterpret_cast<search_engines::NativeCallbackType*>(
          ptr_to_native_callback));
  CHECK(heap_callback);
  if (!j_device_country) {
    return;
  }
  std::string device_country =
      base::android::ConvertJavaStringToUTF8(env, j_device_country);
  int device_country_id =
      country_codes::CountryStringToCountryID(device_country);
  if (device_country_id == country_codes::kCountryIDUnknown) {
    return;
  }
  std::move(*heap_callback).Run(device_country_id);
}
#endif
