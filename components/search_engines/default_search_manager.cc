// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/default_search_manager.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_value_map.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_data_util.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "services/preferences/tracked/pref_hash_filter.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "base/enterprise_util.h"
#include "services/preferences/tracked/features.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

namespace {
bool g_fallback_search_engines_disabled = false;
}  // namespace

const char DefaultSearchManager::kID[] = "id";
const char DefaultSearchManager::kShortName[] = "short_name";
const char DefaultSearchManager::kKeyword[] = "keyword";
const char DefaultSearchManager::kPrepopulateID[] = "prepopulate_id";
const char DefaultSearchManager::kSyncGUID[] = "synced_guid";

const char DefaultSearchManager::kURL[] = "url";
const char DefaultSearchManager::kSuggestionsURL[] = "suggestions_url";
const char DefaultSearchManager::kImageURL[] = "image_url";
const char DefaultSearchManager::kImageTranslateURL[] = "image_translate_url";
const char DefaultSearchManager::kNewTabURL[] = "new_tab_url";
const char DefaultSearchManager::kContextualSearchURL[] =
    "contextual_search_url";
const char DefaultSearchManager::kFaviconURL[] = "favicon_url";
const char DefaultSearchManager::kLogoURL[] = "logo_url";
const char DefaultSearchManager::kDoodleURL[] = "doodle_url";
const char DefaultSearchManager::kOriginatingURL[] = "originating_url";

const char DefaultSearchManager::kSearchURLPostParams[] =
    "search_url_post_params";
const char DefaultSearchManager::kSuggestionsURLPostParams[] =
    "suggestions_url_post_params";
const char DefaultSearchManager::kImageURLPostParams[] =
    "image_url_post_params";
const char DefaultSearchManager::kImageSearchBrandingLabel[] =
    "image_search_branding_label";
const char DefaultSearchManager::kSearchIntentParams[] = "search_intent_params";
const char DefaultSearchManager::kImageTranslateSourceLanguageParamKey[] =
    "image_translate_source_language_param_key";
const char DefaultSearchManager::kImageTranslateTargetLanguageParamKey[] =
    "image_translate_target_language_param_key";

const char DefaultSearchManager::kSafeForAutoReplace[] = "safe_for_autoreplace";
const char DefaultSearchManager::kInputEncodings[] = "input_encodings";

const char DefaultSearchManager::kDateCreated[] = "date_created";
const char DefaultSearchManager::kLastModified[] = "last_modified";
const char DefaultSearchManager::kLastVisited[] = "last_visited";

const char DefaultSearchManager::kUsageCount[] = "usage_count";
const char DefaultSearchManager::kAlternateURLs[] = "alternate_urls";
const char DefaultSearchManager::kPolicyOrigin[] = "policy_origin";
const char DefaultSearchManager::kDisabledByPolicy[] = "disabled_by_policy";
const char DefaultSearchManager::kCreatedFromPlayAPI[] =
    "created_from_play_api";
const char DefaultSearchManager::kFeaturedByPolicy[] = "featured_by_policy";
const char DefaultSearchManager::kRequireShortcut[] = "require_shortcut";
const char DefaultSearchManager::kPreconnectToSearchUrl[] =
    "preconnect_to_search_url";
const char DefaultSearchManager::kPrefetchLikelyNavigations[] =
    "prefetch_likely_navigations";
const char DefaultSearchManager::kIsActive[] = "is_active";
const char DefaultSearchManager::kStarterPackId[] = "starter_pack_id";
const char DefaultSearchManager::kEnforcedByPolicy[] = "enforced_by_policy";

const char DefaultSearchManager::kDefaultSearchEngineMirroredMetric[] =
    "Search.DefaultSearchEngineMirrored";
const char
    DefaultSearchManager::kDefaultSearchEngineMirrorCheckOutcomeMetric[] =
        "Search.DefaultSearchEngineMirrorCheckOutcome";

DefaultSearchManager::DefaultSearchManager(
    PrefService* pref_service,
    search_engines::SearchEngineChoiceService* search_engine_choice_service,
    TemplateURLPrepopulateData::Resolver& prepopulate_data_resolver,
    const ObserverCallback& change_observer)
    : pref_service_(pref_service),
      search_engine_choice_service_(search_engine_choice_service),
      change_observer_(change_observer),
      search_engine_choice_service_observation_(this),
      prepopulate_data_resolver_(prepopulate_data_resolver),
      prefs_default_search_(prepopulate_data_resolver) {
  if (pref_service_) {
    pref_change_registrar_.Init(pref_service_);
    pref_change_registrar_.Add(
        kDefaultSearchProviderDataPrefName,
        base::BindRepeating(&DefaultSearchManager::OnDefaultSearchPrefChanged,
                            base::Unretained(this)));
    pref_change_registrar_.Add(
        prefs::kSearchProviderOverrides,
        base::BindRepeating(&DefaultSearchManager::OnOverridesPrefChanged,
                            base::Unretained(this)));
  }
  LoadPrepopulatedFallbackSearch();
  if (search_engine_choice_service->IsDsePropagationAllowedForGuest()) {
    // Observe the SearchEngineChoiceService because the saved DSE can change
    // during a Guest session and we need to restore it for the next session.
    // TODO(crbug.com: 369959287): This is not needed if we destroy the guest
    // profile.
    search_engine_choice_service_observation_.Observe(
        search_engine_choice_service);
    LoadSavedGuestSearch();
  }
  LoadDefaultSearchEngineFromPrefs();
  const base::Value::Dict& url_dict =
      pref_service_->GetDict(kDefaultSearchProviderDataPrefName);
  const base::Value::Dict& mirrored_dict =
      pref_service_->GetDict(kMirroredDefaultSearchProviderDataPrefName);
  if (mirrored_dict.empty() && !url_dict.empty()) {
    pref_service_->SetDict(kMirroredDefaultSearchProviderDataPrefName,
                           url_dict.Clone());
  } else {
    base::UmaHistogramBoolean(
        DefaultSearchManager::kDefaultSearchEngineMirroredMetric,
        mirrored_dict == url_dict);
    HandleDefaultSearchEngineTampering(url_dict, mirrored_dict);
  }
}

DefaultSearchManager::~DefaultSearchManager() = default;

// static
void DefaultSearchManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(kDefaultSearchProviderDataPrefName);
  registry->RegisterDictionaryPref(kMirroredDefaultSearchProviderDataPrefName);
  registry->RegisterBooleanPref(
      prefs::kUnacknowledgedDefaultSearchEngineResetOccurred, false);
  registry->RegisterTimePref(
      prefs::kDefaultSearchEngineMirrorCheckResetTimeStamp, base::Time());
  registry->RegisterTimePref(prefs::kResetTimeForLastShownNotification,
                             base::Time());
}

// static
void DefaultSearchManager::AddPrefValueToMap(base::Value::Dict value,
                                             PrefValueMap* pref_value_map) {
  pref_value_map->SetValue(kDefaultSearchProviderDataPrefName,
                           base::Value(std::move(value)));
}

// static
void DefaultSearchManager::SetFallbackSearchEnginesDisabledForTesting(
    bool disabled) {
  g_fallback_search_engines_disabled = disabled;
}

const TemplateURLData* DefaultSearchManager::GetDefaultSearchEngine(
    Source* source) const {
  if (default_search_mandatory_by_policy_) {
    if (source)
      *source = FROM_POLICY;
    return prefs_default_search_.Get();
  }
  if (default_search_recommended_by_policy_) {
    if (source)
      *source = FROM_POLICY_RECOMMENDED;
    return prefs_default_search_.Get();
  }
  if (extension_default_search_) {
    if (source)
      *source = FROM_EXTENSION;
    return extension_default_search_.get();
  }
  if (prefs_default_search_.Get()) {
    if (source)
      *source = FROM_USER;
    return prefs_default_search_.Get();
  }
  if (source)
    *source = FROM_FALLBACK;
  return GetFallbackSearchEngine();
}

std::unique_ptr<TemplateURLData>
DefaultSearchManager::GetDefaultSearchEngineIgnoringExtensions() const {
  if (prefs_default_search_.Get()) {
    return std::make_unique<TemplateURLData>(*prefs_default_search_.Get());
  }

  if (default_search_mandatory_by_policy_ ||
      default_search_recommended_by_policy_) {
    // If a policy specified a specific engine, it would be returned above
    // as |prefs_default_search_|. The only other scenario is that policy has
    // disabled default search, in which case we return null.
    return nullptr;
  }

  // |prefs_default_search_| may not be populated even if there is a user
  // preference; check prefs directly as the source of truth.
  const base::Value* user_value =
      pref_service_->GetUserPrefValue(kDefaultSearchProviderDataPrefName);
  if (user_value && user_value->is_dict()) {
    auto turl_data = TemplateURLDataFromDictionary(user_value->GetDict());
    if (turl_data)
      return turl_data;
  }

  const TemplateURLData* fallback = GetFallbackSearchEngine();
  if (fallback)
    return std::make_unique<TemplateURLData>(*fallback);

  return nullptr;
}

DefaultSearchManager::Source
DefaultSearchManager::GetDefaultSearchEngineSource() const {
  Source source;
  GetDefaultSearchEngine(&source);
  return source;
}

const TemplateURLData* DefaultSearchManager::GetFallbackSearchEngine() const {
  if (g_fallback_search_engines_disabled) {
    return nullptr;
  }
  if (saved_guest_search_) {
    return saved_guest_search_.get();
  }
  return fallback_default_search_.get();
}

void DefaultSearchManager::SetUserSelectedDefaultSearchEngine(
    const TemplateURLData& data) {
  if (!pref_service_) {
    prefs_default_search_.SetAndReconcile(
        std::make_unique<TemplateURLData>(data));
    NotifyObserver();
    return;
  }
  pref_service_->SetDict(kDefaultSearchProviderDataPrefName,
                         TemplateURLDataToDictionary(data));
#if BUILDFLAG(IS_ANDROID)
  // Commit the pref immediately so it isn't lost if the app is killed.
  pref_service_->CommitPendingWrite();
#endif
}

void DefaultSearchManager::ClearUserSelectedDefaultSearchEngine() {
  if (pref_service_) {
    pref_service_->ClearPref(kDefaultSearchProviderDataPrefName);
  } else {
    prefs_default_search_.SetAndReconcile({});
    NotifyObserver();
  }
}

void DefaultSearchManager::OnDefaultSearchPrefChanged() {
  bool source_was_fallback = GetDefaultSearchEngineSource() == FROM_FALLBACK;

  LoadDefaultSearchEngineFromPrefs();
  // Mirror the dse pref to the mirrored pref.
  const base::Value::Dict& url_dict =
      pref_service_->GetDict(kDefaultSearchProviderDataPrefName);
  pref_service_->SetDict(kMirroredDefaultSearchProviderDataPrefName,
                         url_dict.Clone());

  if (base::FeatureList::IsEnabled(
          switches::kResetTamperedDefaultSearchEngine)) {
    if (url_dict.empty()) {
      pref_service_->SetBoolean(
          prefs::kUnacknowledgedDefaultSearchEngineResetOccurred, true);
    } else {
      // Cancel the pending notification if, following a security based DSE
      // reset, the DSE is changed by the user before the dialog is shown. In
      // other cases, this is a harmless no-op as the pref is already false.
      pref_service_->SetBoolean(
          prefs::kUnacknowledgedDefaultSearchEngineResetOccurred, false);
    }
  }

  // The effective DSE may have changed unless we were using the fallback source
  // both before and after the above load.
  if (!source_was_fallback || (GetDefaultSearchEngineSource() != FROM_FALLBACK))
    NotifyObserver();
}

void DefaultSearchManager::OnOverridesPrefChanged() {
  LoadPrepopulatedFallbackSearch();

  const TemplateURLData* effective_data = GetDefaultSearchEngine(nullptr);
  if (effective_data && effective_data->prepopulate_id) {
    // A user-selected, policy-selected or fallback pre-populated engine is
    // active and may have changed with this event.
    NotifyObserver();
  }
}

void DefaultSearchManager::OnSavedGuestSearchChanged() {
  LoadSavedGuestSearch();

  const TemplateURLData* effective_data = GetDefaultSearchEngine(nullptr);
  if (effective_data && effective_data->prepopulate_id) {
    // A user-selected, policy-selected or fallback pre-populated engine is
    // active and may have changed with this event.
    NotifyObserver();
  }
}

void DefaultSearchManager::LoadDefaultSearchEngineFromPrefs() {
  if (!pref_service_)
    return;

  prefs_default_search_.SetAndReconcile({});
  extension_default_search_.reset();
  const PrefService::Preference* pref =
      pref_service_->FindPreference(kDefaultSearchProviderDataPrefName);
  DCHECK(pref);
  default_search_mandatory_by_policy_ = pref->IsManaged();
  default_search_recommended_by_policy_ = pref->IsRecommended();

  const base::Value::Dict& url_dict =
      pref_service_->GetDict(kDefaultSearchProviderDataPrefName);
  if (url_dict.empty()) {
    return;
  }

  if (default_search_mandatory_by_policy_ ||
      default_search_recommended_by_policy_) {
    if (url_dict.FindBool(kDisabledByPolicy).value_or(false))
      return;
  }

  auto turl_data = TemplateURLDataFromDictionary(url_dict);
  if (!turl_data)
    return;

  // Check if default search preference is overridden by extension.
  if (pref->IsExtensionControlled()) {
    extension_default_search_ = std::move(turl_data);
  } else {
    prefs_default_search_.SetAndReconcile(std::move(turl_data));
  }
}

void DefaultSearchManager::LoadSavedGuestSearch() {
  std::optional<int> prepopulate_id =
      search_engine_choice_service_->GetSavedSearchEngineBetweenGuestSessions();
  if (prepopulate_id.has_value()) {
    saved_guest_search_ =
        prepopulate_data_resolver_->GetEngineFromFullList(*prepopulate_id);
  } else {
    saved_guest_search_.reset();
  }
}

void DefaultSearchManager::LoadPrepopulatedFallbackSearch() {
  std::unique_ptr<TemplateURLData> data =
      prepopulate_data_resolver_->GetFallbackSearch();
  fallback_default_search_ = std::move(data);
}

void DefaultSearchManager::NotifyObserver() {
  if (!change_observer_.is_null()) {
    Source source = FROM_FALLBACK;
    const TemplateURLData* data = GetDefaultSearchEngine(&source);
    change_observer_.Run(data, source);
  }
}

void DefaultSearchManager::HandleDefaultSearchEngineTampering(
    const base::Value::Dict& url_dict,
    const base::Value::Dict& mirrored_dict) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

  if (!base::FeatureList::IsEnabled(
          switches::kResetTamperedDefaultSearchEngine)) {
    return;
  }

  DefaultSearchEngineMirrorCheckOutcomeType outcome;
  if (mirrored_dict == url_dict) {  // No tampering detected.
    outcome = DefaultSearchEngineMirrorCheckOutcomeType::kNoTamperingDetected;
  } else if (url_dict.empty()) {  // DSE reset by HMAC check.
    if (HasRecentPrefReset()) {   // HMAC reset occurred recently.
      outcome = DefaultSearchEngineMirrorCheckOutcomeType::kRecentHmacReset;
      pref_service_->SetBoolean(
          prefs::kUnacknowledgedDefaultSearchEngineResetOccurred, true);
    } else {
      // HMAC reset occurred long ago, but mirrored pref was never cleared.
      outcome = DefaultSearchEngineMirrorCheckOutcomeType::kStaleHmacReset;
    }
    // Clear the mirrored pref to eliminate future mismatch.
    pref_service_->ClearPref(kMirroredDefaultSearchProviderDataPrefName);
  } else {  // Tampering detected.
    if (!base::IsEnterpriseDevice() ||
        base::FeatureList::IsEnabled(
            tracked::kEnableEncryptedTrackedPrefOnEnterprise)) {
      outcome = DefaultSearchEngineMirrorCheckOutcomeType::kMirrorCheckReset;
      pref_service_->ClearPref(kDefaultSearchProviderDataPrefName);
      // Clear the mirrored pref to eliminate future mismatch.
      pref_service_->ClearPref(kMirroredDefaultSearchProviderDataPrefName);
      pref_service_->SetBoolean(
          prefs::kUnacknowledgedDefaultSearchEngineResetOccurred, true);
      pref_service_->SetTime(
          prefs::kDefaultSearchEngineMirrorCheckResetTimeStamp,
          base::Time::Now());
    } else {
      outcome = DefaultSearchEngineMirrorCheckOutcomeType::
          kResetSkippedForEnterpriseDevice;
    }
  }
  base::UmaHistogramEnumeration(kDefaultSearchEngineMirrorCheckOutcomeMetric,
                                outcome);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
}

bool DefaultSearchManager::HasRecentPrefReset() {
  base::Time reset_time = PrefHashFilter::GetResetTime(pref_service_);

  if (reset_time.is_null()) {
    return false;
  }
  static constexpr base::TimeDelta kRecentResetThreshold = base::Hours(1);
  // Time between when the reset occurred and when the DefaultSearchManager is
  // aware of it.
  const base::TimeDelta since_reset = base::Time::Now() - reset_time;
  return since_reset < kRecentResetThreshold;
}

bool DefaultSearchManager::GetUnacknowledgedDefaultSearchEngineReset() const {
  return pref_service_->GetBoolean(
      prefs::kUnacknowledgedDefaultSearchEngineResetOccurred);
}

void DefaultSearchManager::SetUnacknowledgedDefaultSearchEngineReset(
    bool unacknowledged_reset) {
  pref_service_->SetBoolean(
      prefs::kUnacknowledgedDefaultSearchEngineResetOccurred,
      unacknowledged_reset);
}

base::Time
DefaultSearchManager::GetDefaultSearchEngineMirrorCheckResetTimeStamp() const {
  return pref_service_->GetTime(
      prefs::kDefaultSearchEngineMirrorCheckResetTimeStamp);
}

void DefaultSearchManager::
    SetDefaultSearchEngineMirrorCheckResetTimeStampForTesting(base::Time time) {
  pref_service_->SetTime(prefs::kDefaultSearchEngineMirrorCheckResetTimeStamp,
                         time);
}

base::Time DefaultSearchManager::GetResetTimeForLastShownNotification() const {
  return pref_service_->GetTime(prefs::kResetTimeForLastShownNotification);
}

void DefaultSearchManager::SetResetTimeForLastShownNotification(
    base::Time time) {
  pref_service_->SetTime(prefs::kResetTimeForLastShownNotification, time);
}
