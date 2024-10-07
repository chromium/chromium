// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_DEFAULT_SEARCH_MANAGER_H_
#define COMPONENTS_SEARCH_ENGINES_DEFAULT_SEARCH_MANAGER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/search_engines/reconciling_template_url_data_holder.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"

namespace search_engines {
class SearchEngineChoiceService;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class PrefService;
class PrefValueMap;
struct TemplateURLData;

// DefaultSearchManager handles the loading and writing of the user's default
// search engine selection to and from prefs.
class DefaultSearchManager
    : public search_engines::SearchEngineChoiceService::Observer {
 public:
  // A dictionary to hold all data related to the Default Search Engine.
  // Eventually, this should replace all the data stored in the
  // default_search_provider.* prefs.
  static constexpr char kDefaultSearchProviderDataPrefName[] =
      "default_search_provider_data.template_url_data";

  static const char kID[];
  static const char kShortName[];
  static const char kKeyword[];
  static const char kPrepopulateID[];
  static const char kSyncGUID[];

  static const char kURL[];
  static const char kSuggestionsURL[];
  static const char kImageURL[];
  static const char kImageTranslateURL[];
  static const char kNewTabURL[];
  static const char kContextualSearchURL[];
  static const char kFaviconURL[];
  static const char kLogoURL[];
  static const char kDoodleURL[];
  static const char kOriginatingURL[];

  static const char kSearchURLPostParams[];
  static const char kSuggestionsURLPostParams[];
  static const char kImageURLPostParams[];
  static const char kSideSearchParam[];
  static const char kSideImageSearchParam[];
  static const char kImageSearchBrandingLabel[];
  static const char kSearchIntentParams[];
  static const char kImageTranslateSourceLanguageParamKey[];
  static const char kImageTranslateTargetLanguageParamKey[];

  static const char kSafeForAutoReplace[];
  static const char kInputEncodings[];

  static const char kDateCreated[];
  static const char kLastModified[];
  static const char kLastVisited[];

  static const char kUsageCount[];
  static const char kAlternateURLs[];
  static const char kCreatedByPolicy[];
  static const char kDisabledByPolicy[];
  static const char kCreatedFromPlayAPI[];
  static const char kFeaturedByPolicy[];
  static const char kPreconnectToSearchUrl[];
  static const char kPrefetchLikelyNavigations[];
  static const char kIsActive[];
  static const char kStarterPackId[];
  static const char kEnforcedByPolicy[];

  enum Source {
    // Default search engine chosen either from prepopulated engines set for
    // current country or overriden from kSearchProviderOverrides preference.
    FROM_FALLBACK = 0,
    // User selected engine.
    FROM_USER,
    // Search engine set by extension overriding default search.
    FROM_EXTENSION,
    // Search engine controlled externally through enterprise configuration
    // management (e.g. windows group policy).
    FROM_POLICY,
    // Search engine recommended externally through enterprise configuration
    // management but allows for user modification.
    FROM_POLICY_RECOMMENDED,
  };

  using ObserverCallback =
      base::RepeatingCallback<void(const TemplateURLData*, Source)>;

  DefaultSearchManager(
      PrefService* pref_service,
      search_engines::SearchEngineChoiceService* search_engine_choice_service,
      const ObserverCallback& change_observer
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      ,
      bool for_lacros_main_profile
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  );

  DefaultSearchManager(const DefaultSearchManager&) = delete;
  DefaultSearchManager& operator=(const DefaultSearchManager&) = delete;

  ~DefaultSearchManager() override;

  // Register prefs needed for tracking the default search provider.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Save default search provider pref values into the map provided.
  static void AddPrefValueToMap(base::Value::Dict value,
                                PrefValueMap* pref_value_map);

  // Testing code can call this with |disabled| set to true to cause
  // GetDefaultSearchEngine() to return nullptr instead of
  // |fallback_default_search_| in cases where the DSE source is FROM_FALLBACK.
  static void SetFallbackSearchEnginesDisabledForTesting(bool disabled);

  // Gets a pointer to the current Default Search Engine. If NULL, indicates
  // that Default Search is explicitly disabled. |source|, if not NULL, will be
  // filled in with the source of the result.
  const TemplateURLData* GetDefaultSearchEngine(Source* source) const;

  // Returns a pointer to the highest-ranking search provider while ignoring
  // any extension-provided search engines.
  std::unique_ptr<TemplateURLData> GetDefaultSearchEngineIgnoringExtensions()
      const;

  // Gets the source of the current Default Search Engine value.
  Source GetDefaultSearchEngineSource() const;

  // Returns a pointer to the fallback engine.
  const TemplateURLData* GetFallbackSearchEngine() const;

  // Write default search provider data to |pref_service_|.
  void SetUserSelectedDefaultSearchEngine(const TemplateURLData& data);

  // Clear the user's default search provider choice from |pref_service_|. Does
  // not explicitly disable Default Search. The new default search
  // engine will be defined by policy, extensions, or pre-populated data.
  void ClearUserSelectedDefaultSearchEngine();

 private:
  // Handles changes to kDefaultSearchProviderData pref. This includes sync and
  // policy changes. Calls LoadDefaultSearchEngineFromPrefs() and
  // NotifyObserver() if the effective DSE might have changed.
  void OnDefaultSearchPrefChanged();

  // Handles changes to kSearchProviderOverrides pref. Calls
  // LoadPrepopulatedFallbackSearch() and NotifyObserver() if the effective DSE
  // might have changed.
  void OnOverridesPrefChanged();

  // Reads default search provider data from |pref_service_|, updating
  // |prefs_default_search_|, |default_search_mandatory_by_policy_|, and
  // |default_search_recommended_by_policy_|.
  // Invokes MergePrefsDataWithPrepopulated().
  void LoadDefaultSearchEngineFromPrefs();

  // Reads guest search provider, which was previously saved for future guest
  // session. Updates |saved_guest_search_|.
  void LoadSavedGuestSearch();

  // Reads pre-populated search providers, which will be built-in or overridden
  // by kSearchProviderOverrides. Updates |fallback_default_search_|. Invoke
  // MergePrefsDataWithPrepopulated().
  void LoadPrepopulatedFallbackSearch();

  // Invokes |change_observer_| if it is not NULL.
  void NotifyObserver();

  // search_engines::SearchEngineChoiceService::Observer
  void OnSavedGuestSearchChanged() override;

  const raw_ptr<PrefService> pref_service_;
  const raw_ptr<search_engines::SearchEngineChoiceService>
      search_engine_choice_service_ = nullptr;

  const ObserverCallback change_observer_;
  PrefChangeRegistrar pref_change_registrar_;
  base::ScopedObservation<search_engines::SearchEngineChoiceService,
                          search_engines::SearchEngineChoiceService::Observer>
      search_engine_choice_service_observation_{this};

  // Default search engine provided by pre-populated data or by the
  // |kSearchProviderOverrides| pref. This will be used when no other default
  // search engine has been selected.
  std::unique_ptr<TemplateURLData> fallback_default_search_;

  // Default search engine provided by extension (usings Settings Override API).
  // This will be null if there are no extensions installed which provide
  // default search engines.
  std::unique_ptr<TemplateURLData> extension_default_search_;

  // Default search engine provided by prefs (either user prefs or policy
  // prefs). This will be null if no value was set in the pref store.
  ReconcilingTemplateURLDataHolder prefs_default_search_;

  // Default search engine provided by previous SearchEngineChoice in guest
  // mode.
  std::unique_ptr<TemplateURLData> saved_guest_search_;

  // True if the default search is currently enforced by policy.
  bool default_search_mandatory_by_policy_ = false;

  // True if the default search is currently recommended by policy.
  bool default_search_recommended_by_policy_ = false;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // True if this instance is used for the Lacros primary profile.
  bool for_lacros_main_profile_ = false;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

#endif  // COMPONENTS_SEARCH_ENGINES_DEFAULT_SEARCH_MANAGER_H_
