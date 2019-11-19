// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_DEFAULT_SEARCH_MANAGER_H_
#define COMPONENTS_SEARCH_ENGINES_DEFAULT_SEARCH_MANAGER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "components/prefs/pref_change_registrar.h"

namespace base {
class DictionaryValue;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class PrefService;
class PrefValueMap;
struct TemplateURLData;

// DefaultSearchManager handles the loading and writing of the user's default
// search engine selection to and from prefs.
class DefaultSearchManager {
 public:
  static const char kDefaultSearchProviderDataPrefName[];

  static const char kID[];
  static const char kShortName[];
  static const char kKeyword[];
  static const char kPrepopulateID[];
  static const char kSyncGUID[];

  static const char kURL[];
  static const char kSuggestionsURL[];
  static const char kImageURL[];
  static const char kNewTabURL[];
  static const char kContextualSearchURL[];
  static const char kFaviconURL[];
  static const char kLogoURL[];
  static const char kDoodleURL[];
  static const char kOriginatingURL[];

  static const char kSearchURLPostParams[];
  static const char kSuggestionsURLPostParams[];
  static const char kImageURLPostParams[];

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
  };

  typedef base::Callback<void(const TemplateURLData*, Source)> ObserverCallback;

  DefaultSearchManager(PrefService* pref_service,
                       const ObserverCallback& change_observer);

  ~DefaultSearchManager();

  // Register prefs needed for tracking the default search provider.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Save default search provider pref values into the map provided.
  static void AddPrefValueToMap(std::unique_ptr<base::DictionaryValue> value,
                                PrefValueMap* pref_value_map);

  // Testing code can call this with |disabled| set to true to cause
  // GetDefaultSearchEngine() to return nullptr instead of
  // |fallback_default_search_| in cases where the DSE source is FROM_FALLBACK.
  static void SetFallbackSearchEnginesDisabledForTesting(bool disabled);

  // Gets a pointer to the current Default Search Engine. If NULL, indicates
  // that Default Search is explicitly disabled. |source|, if not NULL, will be
  // filled in with the source of the result.
  const TemplateURLData* GetDefaultSearchEngine(Source* source) const;

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
  // LoadPrepopulatedDefaultSearch() and NotifyObserver() if the effective DSE
  // might have changed.
  void OnOverridesPrefChanged();

  // Updates |prefs_default_search_| with values from its corresponding
  // pre-populated search provider record, if any.
  void MergePrefsDataWithPrepopulated();

  // Reads default search provider data from |pref_service_|, updating
  // |prefs_default_search_| and |default_search_controlled_by_policy_|.
  // Invokes MergePrefsDataWithPrepopulated().
  void LoadDefaultSearchEngineFromPrefs();

  // Reads pre-populated search providers, which will be built-in or overridden
  // by kSearchProviderOverrides. Updates |fallback_default_search_|. Invoke
  // MergePrefsDataWithPrepopulated().
  void LoadPrepopulatedDefaultSearch();

  // Invokes |change_observer_| if it is not NULL.
  void NotifyObserver();

  PrefService* pref_service_;
  const ObserverCallback change_observer_;
  PrefChangeRegistrar pref_change_registrar_;

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
  std::unique_ptr<TemplateURLData> prefs_default_search_;

  // True if the default search is currently enforced by policy.
  bool default_search_controlled_by_policy_;

  DISALLOW_COPY_AND_ASSIGN(DefaultSearchManager);
};

#endif  // COMPONENTS_SEARCH_ENGINES_DEFAULT_SEARCH_MANAGER_H_
