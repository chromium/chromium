// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/controllers/adapters/desktop_bookmark_bar_prefs_adapter.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/prefs/pref_service.h"

DesktopBookmarkBarPrefsAdapter::DesktopBookmarkBarPrefsAdapter(Profile* profile)
    : profile_(profile) {
  pref_registrar_.Init(profile_->GetPrefs());
}

DesktopBookmarkBarPrefsAdapter::~DesktopBookmarkBarPrefsAdapter() = default;

bool DesktopBookmarkBarPrefsAdapter::GetBoolean(
    const std::string& pref_name) const {
  if (pref_name == bookmarks::prefs::kShowAppsShortcutInBookmarkBar) {
    return chrome::ShouldShowAppsShortcutInBookmarkBar(profile_);
  }
  if (pref_name == bookmarks::prefs::kShowTabGroupsInBookmarkBar) {
    // Incognito browsers also get triggered if the associated regular profile
    // browser is triggered. Early return because incognito has no
    // `saved_tab_group_bar_`.
    return tab_groups::SavedTabGroupUtils::IsEnabledForProfile(profile_) &&
           chrome::ShouldShowTabGroupsInBookmarkBar(profile_);
  }
  return profile_->GetPrefs()->GetBoolean(pref_name);
}

void DesktopBookmarkBarPrefsAdapter::AddObserver(const std::string& pref_name,
                                                 PrefChangedCallback callback) {
  pref_registrar_.Add(pref_name, std::move(callback));
}
