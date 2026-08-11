// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_MOST_VISITED_MOST_VISITED_PREF_OBSERVER_H_
#define CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_MOST_VISITED_MOST_VISITED_PREF_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_change_registrar.h"

class MostVisitedHandler;
class PrefRegistrySimple;
class PrefService;
class Profile;

// Manages and observes 1P preferences (custom links, enterprise shortcuts,
// visibility) for MostVisitedHandler. Used by 1P surfaces (NewTabPageUI,
// OmniboxEverywhereUI) so 3P surfaces (NewTabPageThirdPartyUI) remain isolated.
class MostVisitedPrefObserver {
 public:
  MostVisitedPrefObserver(Profile* profile, MostVisitedHandler* handler);
  MostVisitedPrefObserver(const MostVisitedPrefObserver&) = delete;
  MostVisitedPrefObserver& operator=(const MostVisitedPrefObserver&) = delete;
  ~MostVisitedPrefObserver();

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
  static void ResetProfilePrefs(PrefService* prefs);
  static void MigrateDeprecatedUseMostVisitedTilesPref(PrefService* prefs);
  static void MigrateDeprecatedShortcutsTypePref(PrefService* prefs);

 private:
  bool IsShortcutsVisible() const;
  void UpdateMostVisitedTileTypes();
  void OnTileTypesChanged();
  void OnTilesVisibilityPrefChanged();
  void OnEnterpriseShortcutsPolicyChanged();
  void MaybeEnableEnterpriseShortcutsVisibility();

  raw_ptr<Profile> profile_;
  raw_ptr<MostVisitedHandler> handler_;
  PrefChangeRegistrar pref_change_registrar_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_MOST_VISITED_MOST_VISITED_PREF_OBSERVER_H_
