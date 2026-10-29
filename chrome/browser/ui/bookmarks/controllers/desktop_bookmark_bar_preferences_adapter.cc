// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/controllers/desktop_bookmark_bar_preferences_adapter.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/prefs/pref_service.h"

DesktopBookmarkBarPreferencesAdapter::DesktopBookmarkBarPreferencesAdapter(
    Profile* profile)
    : profile_(profile) {
  pref_registrar_.Init(profile_->GetPrefs());
}

DesktopBookmarkBarPreferencesAdapter::~DesktopBookmarkBarPreferencesAdapter() =
    default;

bool DesktopBookmarkBarPreferencesAdapter::IsAppsShortcutEnabled() const {
  return chrome::ShouldShowAppsShortcutInBookmarkBar(profile_);
}

bool DesktopBookmarkBarPreferencesAdapter::IsTabGroupsEnabled() const {
  // Incognito browsers also get triggered if the associated regular profile
  // browser is triggered. Early return because incognito has no
  // `saved_tab_group_bar_`.
  return tab_groups::SavedTabGroupUtils::IsEnabledForProfile(profile_) &&
         chrome::ShouldShowTabGroupsInBookmarkBar(profile_);
}

bool DesktopBookmarkBarPreferencesAdapter::IsManagedBookmarksEnabled() const {
  return profile_->GetPrefs()->GetBoolean(
      bookmarks::prefs::kShowManagedBookmarksInBookmarkBar);
}

void DesktopBookmarkBarPreferencesAdapter::SetOnChangedCallback(
    base::RepeatingClosure callback) {
  pref_registrar_.RemoveAll();
  if (callback.is_null()) {
    return;
  }
  for (const char* pref_name :
       {bookmarks::prefs::kShowAppsShortcutInBookmarkBar,
        bookmarks::prefs::kShowTabGroupsInBookmarkBar,
        bookmarks::prefs::kShowManagedBookmarksInBookmarkBar}) {
    pref_registrar_.Add(pref_name, callback);
  }
}
