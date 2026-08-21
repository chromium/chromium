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

// Manages search engines and aggregators configured via enterprise policies
// (specifically `SiteSearchSettings` and `EnterpriseSearchAggregatorSettings`).
//
// Key Concepts:
// - **"In Policy"**: A search engine is considered "in policy" (or "defined by
//   policy") if its definition is present in the active managed policy
//   configuration (e.g. `SiteSearchSettings`). If an administrator removes an
//   engine from the policy list, it is no longer "in policy".
// - **Enforced vs. Recommended**: Engines defined in policy can be "enforced"
//   (mandatory; user overrides and deletions are ignored) or "recommended"
//   (optional; users are allowed to delete or edit them).
// - **User Overrides**: When a user deletes or modifies a recommended policy
//   engine, its keyword is recorded in
//   `kSiteSearchSettingsOverriddenKeywordsPrefName` so that subsequent policy
//   syncs do not restore or re-add it.
// - **Pruning**: If an overridden engine is later removed from the enterprise
//   policy by the administrator (so it is no longer "in policy") or its status
//   is upgraded to enforced, its override record is obsolete and is pruned on
//   subsequent writes.
//
// Responsibilities:
// - Observes managed policy preferences and translates them into
//   TemplateURLData.
// - Merges policy definitions with user overrides for recommended engines.
// - Notifies observers (e.g. `TemplateURLService`) whenever the effective set
//   of enterprise search engines changes.
class EnterpriseSearchManager {
 public:
  // Managed preference (List of Dicts) containing enterprise-configured site
  // search engines.
  static const char kSiteSearchSettingsPrefName[];

  // User preference (List of Strings) tracking keywords of recommended site
  // search engines that the user has overridden (deleted or modified).
  // Note: Consumers should not read or modify this preference directly, as
  // EnterpriseSearchManager automatically merges overrides with policy rules.
  static const char kSiteSearchSettingsOverriddenKeywordsPrefName[];

  // Managed preference (List of Dicts) containing enterprise search aggregator
  // settings.
  static const char kEnterpriseSearchAggregatorSettingsPrefName[];

  // Managed preference (Boolean) indicating whether a shortcut keyword is
  // required to query the enterprise search aggregator.
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

  // Returns true if invoking the enterprise search aggregator requires typing
  // its `@shortcut` keyword in the omnibox, and false otherwise.
  //
  // Returns the policy preference value if managed, or the mock setting if
  // feature flags are active, defaulting to false.
  bool GetRequireShortcutValue() const;

  // Records that the user has overridden (deleted or edited) a recommended
  // policy site search engine with `keyword`.
  // This prevents the engine from being restored on subsequent policy syncs.
  //
  // Also prunes stale entries from storage for keywords that are no longer
  // "in policy" (i.e. removed from the enterprise policy by the administrator)
  // or that have been changed to strictly enforced.
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

  raw_ptr<PrefService> pref_service_;
  PrefChangeRegistrar pref_change_registrar_;

  // Invoked when changes to the list of managed site search engines are
  // detected.
  const ObserverCallback change_observer_;
};

#endif  // COMPONENTS_SEARCH_ENGINES_ENTERPRISE_ENTERPRISE_SEARCH_MANAGER_H_
