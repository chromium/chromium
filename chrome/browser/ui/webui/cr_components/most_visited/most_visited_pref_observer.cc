// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/most_visited/most_visited_pref_observer.h"

#include "base/functional/bind.h"
#include "chrome/browser/new_tab_page/new_tab_page_util.h"
#include "chrome/browser/new_tab_page/prefs/ntp_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/cr_components/most_visited/most_visited_handler.h"
#include "components/ntp_tiles/constants.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/ntp_tiles/tile_type.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

MostVisitedPrefObserver::MostVisitedPrefObserver(Profile* profile,
                                                 MostVisitedHandler* handler)
    : profile_(profile), handler_(handler) {
  CHECK(profile_);
  CHECK(handler_);

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      ntp_prefs::kNtpCustomLinksVisible,
      base::BindRepeating(&MostVisitedPrefObserver::OnTileTypesChanged,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      ntp_prefs::kNtpEnterpriseShortcutsVisible,
      base::BindRepeating(&MostVisitedPrefObserver::OnTileTypesChanged,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      ntp_prefs::kNtpPersonalShortcutsVisible,
      base::BindRepeating(&MostVisitedPrefObserver::OnTileTypesChanged,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      ntp_prefs::kNtpShortcutsVisible,
      base::BindRepeating(
          &MostVisitedPrefObserver::OnTilesVisibilityPrefChanged,
          base::Unretained(this)));
// TODO(b/502297163): Implement for Android.
#if !BUILDFLAG(IS_ANDROID)
  pref_change_registrar_.Add(
      ntp_tiles::prefs::kEnterpriseShortcutsPolicyList,
      base::BindRepeating(
          &MostVisitedPrefObserver::OnEnterpriseShortcutsPolicyChanged,
          base::Unretained(this)));
  MaybeEnableEnterpriseShortcutsVisibility();
#endif

  UpdateMostVisitedTileTypes();
  handler_->SetShortcutsVisible(IsShortcutsVisible());
}

MostVisitedPrefObserver::~MostVisitedPrefObserver() = default;

// static
void MostVisitedPrefObserver::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(ntp_prefs::kNtpCustomLinksVisible, true);
  registry->RegisterBooleanPref(ntp_prefs::kNtpEnterpriseShortcutsVisible,
                                false);
  registry->RegisterBooleanPref(ntp_prefs::kNtpShortcutsVisible, true);
  registry->RegisterIntegerPref(ntp_prefs::kNtpShortcutsStalenessCount, 0);
  registry->RegisterTimePref(ntp_prefs::kNtpLastShortcutsStalenessUpdate,
                             base::Time());
  registry->RegisterBooleanPref(ntp_prefs::kNtpShortcutsAutoRemovalDisabled,
                                false);
  registry->RegisterBooleanPref(ntp_prefs::kNtpPersonalShortcutsVisible, true);
  registry->RegisterBooleanPref(ntp_prefs::kNtpShowAllMostVisitedTiles, false);
  registry->RegisterInt64Pref(ntp_prefs::kNtpMostVisitedTileHoverCount, 0);
  registry->RegisterInt64Pref(ntp_prefs::kNtpMostVisitedTileNavigationCount, 0);
}

// static
void MostVisitedPrefObserver::ResetProfilePrefs(PrefService* prefs) {
  ntp_tiles::MostVisitedSites::ResetProfilePrefs(prefs);
  prefs->SetBoolean(ntp_prefs::kNtpCustomLinksVisible, true);
  prefs->SetBoolean(ntp_prefs::kNtpEnterpriseShortcutsVisible, false);
  prefs->SetBoolean(ntp_prefs::kNtpShortcutsVisible, true);
  prefs->SetInteger(ntp_prefs::kNtpShortcutsStalenessCount, 0);
  prefs->SetTime(ntp_prefs::kNtpLastShortcutsStalenessUpdate, base::Time());
  prefs->SetBoolean(ntp_prefs::kNtpShortcutsAutoRemovalDisabled, false);
  prefs->SetBoolean(ntp_prefs::kNtpPersonalShortcutsVisible, true);
  prefs->SetBoolean(ntp_prefs::kNtpShowAllMostVisitedTiles, false);
  prefs->SetInt64(ntp_prefs::kNtpMostVisitedTileHoverCount, 0);
  prefs->SetInt64(ntp_prefs::kNtpMostVisitedTileNavigationCount, 0);
}

// static
void MostVisitedPrefObserver::MigrateDeprecatedUseMostVisitedTilesPref(
    PrefService* prefs) {
  // Skip migration if the new preference is already set.
  if (prefs->HasPrefPath(ntp_prefs::kNtpShortcutsType)) {
    return;
  }
  const base::Value* user_value =
      prefs->GetUserPrefValue(ntp_prefs::kNtpUseMostVisitedTiles);
  if (user_value) {
    if (user_value->is_bool()) {
      prefs->SetInteger(
          ntp_prefs::kNtpShortcutsType,
          user_value->GetBool()
              ? static_cast<int>(ntp_tiles::TileType::kTopSites)
              : static_cast<int>(ntp_tiles::TileType::kCustomLinks));
    }
    prefs->ClearPref(ntp_prefs::kNtpUseMostVisitedTiles);
  }
}

// static
void MostVisitedPrefObserver::MigrateDeprecatedShortcutsTypePref(
    PrefService* prefs) {
  // Skip migration if the new preferences are already set.
  if (prefs->HasPrefPath(ntp_prefs::kNtpCustomLinksVisible) ||
      prefs->HasPrefPath(ntp_prefs::kNtpEnterpriseShortcutsVisible)) {
    return;
  }
  const base::Value* user_value =
      prefs->GetUserPrefValue(ntp_prefs::kNtpShortcutsType);
  if (user_value) {
    if (user_value->is_int()) {
      switch (static_cast<ntp_tiles::TileType>(user_value->GetInt())) {
        case ntp_tiles::TileType::kTopSites:
          prefs->SetBoolean(ntp_prefs::kNtpCustomLinksVisible, false);
          prefs->SetBoolean(ntp_prefs::kNtpEnterpriseShortcutsVisible, false);
          break;
        case ntp_tiles::TileType::kCustomLinks:
          prefs->SetBoolean(ntp_prefs::kNtpCustomLinksVisible, true);
          prefs->SetBoolean(ntp_prefs::kNtpEnterpriseShortcutsVisible, false);
          break;
        case ntp_tiles::TileType::kEnterpriseShortcuts:
          prefs->SetBoolean(ntp_prefs::kNtpCustomLinksVisible, false);
          prefs->SetBoolean(ntp_prefs::kNtpEnterpriseShortcutsVisible, true);
          break;
      }
    }
    prefs->ClearPref(ntp_prefs::kNtpShortcutsType);
  }
}

bool MostVisitedPrefObserver::IsShortcutsVisible() const {
  return profile_->GetPrefs()->GetBoolean(ntp_prefs::kNtpShortcutsVisible);
}

void MostVisitedPrefObserver::UpdateMostVisitedTileTypes() {
  auto enabled_types = GetEnabledTileTypes(profile_);
  handler_->EnableTileTypes(
      ntp_tiles::MostVisitedSites::EnableTileTypesOptions()
          .with_top_sites(
              enabled_types.contains(ntp_tiles::TileType::kTopSites))
          .with_custom_links(
              enabled_types.contains(ntp_tiles::TileType::kCustomLinks))
          .with_enterprise_shortcuts(enabled_types.contains(
              ntp_tiles::TileType::kEnterpriseShortcuts)));
}

void MostVisitedPrefObserver::OnTileTypesChanged() {
  UpdateMostVisitedTileTypes();
}

void MostVisitedPrefObserver::OnTilesVisibilityPrefChanged() {
  handler_->SetShortcutsVisible(IsShortcutsVisible());
}

void MostVisitedPrefObserver::OnEnterpriseShortcutsPolicyChanged() {
  MaybeEnableEnterpriseShortcutsVisibility();
  OnTileTypesChanged();
}

void MostVisitedPrefObserver::MaybeEnableEnterpriseShortcutsVisibility() {
// TODO(b/502297163): Implement for Android.
#if !BUILDFLAG(IS_ANDROID)
  // If enterprise shortcuts are available by policy and the user
  // has not previously set the visibility preference, then enable enterprise
  // shortcuts by default.
  if (!profile_->GetPrefs()
           ->GetList(ntp_tiles::prefs::kEnterpriseShortcutsPolicyList)
           .empty() &&
      !profile_->GetPrefs()->HasPrefPath(
          ntp_prefs::kNtpEnterpriseShortcutsVisible)) {
    profile_->GetPrefs()->SetBoolean(ntp_prefs::kNtpEnterpriseShortcutsVisible,
                                     true);
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}
