// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_UI_CONTROLLERS_BOOKMARK_BAR_ADAPTERS_PREFERENCES_ADAPTER_H_
#define COMPONENTS_BROWSER_APIS_UI_CONTROLLERS_BOOKMARK_BAR_ADAPTERS_PREFERENCES_ADAPTER_H_

#include "base/functional/callback.h"

namespace bookmark_bar_api {

// Gives the controller the embedder's bookmark bar preferences, which back
// mojom::DisplayOptions. The embedder decides how each value is computed (e.g.
// from prefs, with policy or feature checks) and when it has changed.
class PreferencesAdapter {
 public:
  virtual ~PreferencesAdapter() = default;

  // Whether the apps page shortcut is enabled.
  virtual bool IsAppsShortcutEnabled() const = 0;

  // Whether saved tab groups are enabled for the bookmark bar.
  virtual bool IsTabGroupsEnabled() const = 0;

  // Whether managed bookmarks are enabled for the bookmark bar.
  virtual bool IsManagedBookmarksEnabled() const = 0;

  // Sets the callback to run when any of the values above may have changed,
  // replacing any previous one. A null callback stops the notifications.
  virtual void SetOnChangedCallback(base::RepeatingClosure callback) = 0;
};

}  // namespace bookmark_bar_api

#endif  // COMPONENTS_BROWSER_APIS_UI_CONTROLLERS_BOOKMARK_BAR_ADAPTERS_PREFERENCES_ADAPTER_H_
