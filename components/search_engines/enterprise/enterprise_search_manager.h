// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_ENTERPRISE_ENTERPRISE_SEARCH_MANAGER_H_
#define COMPONENTS_SEARCH_ENGINES_ENTERPRISE_ENTERPRISE_SEARCH_MANAGER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

class PrefValueMap;
struct TemplateURLData;

namespace user_prefs {
class PrefRegistrySyncable;
}

class EnterpriseSearchManager {
 public:
  static const char kSiteSearchSettingsPrefName[];
  static const char kSiteSearchSettingsOverriddenKeywordsPrefName[];
  static const char kEnterpriseSearchAggregatorSettingsPrefName[];
  static const char
      kEnterpriseSearchAggregatorSettingsRequireShortcutPrefName[];

  using OwnedTemplateURLDataVector =
      std::vector<std::unique_ptr<TemplateURLData>>;
  using ObserverCallback =
      base::RepeatingCallback<void(OwnedTemplateURLDataVector&&)>;

  // Possible states for loading search engines from prefs or from mock
  // settings.
  enum class LoadingResult {
    // Source is not available (e.g. controlling feature is disabled), so it
    // should be ignored.
    kUnavailable,
    // Source is available and provides an empty list of search engines.
    // Note: this state forces resetting search engines in the
    // TemplateURLService, which is not the case when the policy is disabled.
    kAvailableEmpty,
    // Source is available and provides a non-empty list of search engines.
    kAvailableNonEmpty,
  };

  EnterpriseSearchManager(PrefService* pref_service,
                          const ObserverCallback& change_observer);
  ~EnterpriseSearchManager();

  // Registers prefs needed for tracking the site search engines.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns the `require_shortcut` value. If set by policy, the preference
  // value is returned. Otherwise, if a valid mock search engine is defined, the
  // mock setting's value is used. Defaults to preference default if neither is
  // set.
  bool GetRequireShortcutValue() const;

  // Adds a keyword to the `kSiteSearchSettingsOverriddenKeywordsPrefName`
  // pref, indicating that the user has overridden the associated engine.
  void AddOverriddenKeyword(const std::string& keyword);

 private:
  // Handles changes to managed prefs due to policy updates. Calls
  // NotifyObserver() if search providers may have changed. Invokes
  // `change_observer_` if it is not NULL.
  void OnPrefChanged();

  LoadingResult LoadSearchEnginesFromPrefs(
      const PrefService::Preference* pref,
      EnterpriseSearchManager::OwnedTemplateURLDataVector* search_engines);

  LoadingResult LoadSearchAggregator(
      EnterpriseSearchManager::OwnedTemplateURLDataVector* search_engines);

  // Updates the `kSiteSearchSettingsOverriddenKeywordsPrefName` pref based
  // on the provided list of site search engines.
  void LoadOverriddenKeywordsPref(const base::Value::List& engine_list);

  raw_ptr<PrefService> pref_service_;
  PrefChangeRegistrar pref_change_registrar_;

  // Invoked when changes to the list of managed site search engines are
  // detected.
  const ObserverCallback change_observer_;
};

#endif  // COMPONENTS_SEARCH_ENGINES_ENTERPRISE_ENTERPRISE_SEARCH_MANAGER_H_
