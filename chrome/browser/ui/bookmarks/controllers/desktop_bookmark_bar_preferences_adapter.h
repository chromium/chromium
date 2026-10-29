// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_DESKTOP_BOOKMARK_BAR_PREFERENCES_ADAPTER_H_
#define CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_DESKTOP_BOOKMARK_BAR_PREFERENCES_ADAPTER_H_

#include "base/memory/raw_ptr.h"
#include "components/browser_apis/ui_controllers/bookmark_bar/adapters/preferences_adapter.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;

// Reads the bookmark bar preferences for `profile`, applying the policy and
// feature checks that go with them.
class DesktopBookmarkBarPreferencesAdapter
    : public bookmark_bar_api::PreferencesAdapter {
 public:
  explicit DesktopBookmarkBarPreferencesAdapter(Profile* profile);
  DesktopBookmarkBarPreferencesAdapter(
      const DesktopBookmarkBarPreferencesAdapter&) = delete;
  DesktopBookmarkBarPreferencesAdapter& operator=(
      const DesktopBookmarkBarPreferencesAdapter&) = delete;
  ~DesktopBookmarkBarPreferencesAdapter() override;

  // bookmark_bar_api::PreferencesAdapter overrides:
  bool IsAppsShortcutEnabled() const override;
  bool IsTabGroupsEnabled() const override;
  bool IsManagedBookmarksEnabled() const override;
  void SetOnChangedCallback(base::RepeatingClosure callback) override;

 private:
  raw_ptr<Profile> profile_;
  PrefChangeRegistrar pref_registrar_;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_DESKTOP_BOOKMARK_BAR_PREFERENCES_ADAPTER_H_
