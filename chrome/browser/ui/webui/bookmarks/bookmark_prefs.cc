// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/bookmarks/bookmark_prefs.h"

#include "chrome/browser/ui/webui/side_panel/bookmarks/bookmarks.mojom.h"
#include "components/prefs/pref_registry_simple.h"

namespace bookmarks_webui {

namespace prefs {

const char kBookmarksSortOrder[] = "bookmarks.side_panel.sort_order";
const char kBookmarksViewType[] = "bookmarks.side_panel.view_type";

}  // namespace prefs

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kBookmarksSortOrder,
      static_cast<int>(side_panel::mojom::SortOrder::kNewest));
  registry->RegisterIntegerPref(
      prefs::kBookmarksViewType,
      static_cast<int>(side_panel::mojom::ViewType::kExpanded));
}

}  // namespace bookmarks_webui
