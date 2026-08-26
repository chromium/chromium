// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ADAPTERS_BOOKMARK_BAR_PREFS_ADAPTER_H_
#define CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ADAPTERS_BOOKMARK_BAR_PREFS_ADAPTER_H_

#include <string>

#include "base/functional/callback.h"

class BookmarkBarPrefsAdapter {
 public:
  virtual ~BookmarkBarPrefsAdapter() = default;

  // Returns the boolean value for a given preference.
  virtual bool GetBoolean(const std::string& pref_name) const = 0;

  // Registers a callback to be run when the specified preference changes.
  using PrefChangedCallback = base::RepeatingClosure;
  virtual void AddObserver(const std::string& pref_name,
                           PrefChangedCallback callback) = 0;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ADAPTERS_BOOKMARK_BAR_PREFS_ADAPTER_H_
