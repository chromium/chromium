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
  static const char kEnterpriseSearchAggregatorSettingsPrefName[];

  using OwnedTemplateURLDataVector =
      std::vector<std::unique_ptr<TemplateURLData>>;
  using ObserverCallback =
      base::RepeatingCallback<void(OwnedTemplateURLDataVector&&)>;

  EnterpriseSearchManager(PrefService* pref_service,
                          const ObserverCallback& change_observer);
  ~EnterpriseSearchManager();

  // Registers prefs needed for tracking the site search engines.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  // Handles changes to managed prefs due to policy updates. Calls
  // NotifyObserver() if search providers may have changed. Invokes
  // `change_observer_` if it is not NULL.
  void OnPrefChanged();

  raw_ptr<PrefService> pref_service_;
  PrefChangeRegistrar pref_change_registrar_;

  // Invoked when changes to the list of managed site search engines are
  // detected.
  const ObserverCallback change_observer_;
};

#endif  // COMPONENTS_SEARCH_ENGINES_ENTERPRISE_ENTERPRISE_SEARCH_MANAGER_H_
