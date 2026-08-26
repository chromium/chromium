// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ADAPTERS_DESKTOP_BOOKMARK_BAR_PREFS_ADAPTER_H_
#define CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ADAPTERS_DESKTOP_BOOKMARK_BAR_PREFS_ADAPTER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/bookmarks/controllers/adapters/bookmark_bar_prefs_adapter.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;

class DesktopBookmarkBarPrefsAdapter : public BookmarkBarPrefsAdapter {
 public:
  explicit DesktopBookmarkBarPrefsAdapter(Profile* profile);
  DesktopBookmarkBarPrefsAdapter(const DesktopBookmarkBarPrefsAdapter&) =
      delete;
  DesktopBookmarkBarPrefsAdapter& operator=(
      const DesktopBookmarkBarPrefsAdapter&) = delete;
  ~DesktopBookmarkBarPrefsAdapter() override;

  // BookmarkBarPrefsAdapter overrides:
  bool GetBoolean(const std::string& pref_name) const override;
  void AddObserver(const std::string& pref_name,
                   PrefChangedCallback callback) override;

 private:
  raw_ptr<Profile> profile_;
  PrefChangeRegistrar pref_registrar_;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ADAPTERS_DESKTOP_BOOKMARK_BAR_PREFS_ADAPTER_H_
