// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/controllers/bookmark_bar_preferences_adapter.h"

#include "base/check.h"
#include "chrome/browser/bookmarks/bookmark_bar_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/prefs/pref_service.h"

BookmarkBarPreferencesAdapter::BookmarkBarPreferencesAdapter(Profile* profile)
    : profile_(profile) {
  CHECK(profile_);
  pref_registrar_.Init(profile_->GetPrefs());
}

BookmarkBarPreferencesAdapter::~BookmarkBarPreferencesAdapter() = default;

bool BookmarkBarPreferencesAdapter::IsAppsShortcutEnabled() const {
  return chrome::ShouldShowAppsShortcutInBookmarkBar(profile_);
}

bool BookmarkBarPreferencesAdapter::IsTabGroupsEnabled() const {
  // Incognito browsers also get triggered if the associated regular profile
  // browser is triggered. Early return because incognito has no
  // `saved_tab_group_bar_`.
  return tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile_) !=
             nullptr &&
         chrome::ShouldShowTabGroupsInBookmarkBar(profile_);
}

bool BookmarkBarPreferencesAdapter::IsManagedBookmarksEnabled() const {
  return profile_->GetPrefs()->GetBoolean(
      bookmarks::prefs::kShowManagedBookmarksInBookmarkBar);
}

void BookmarkBarPreferencesAdapter::SetOnChangedCallback(
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
