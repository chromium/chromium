// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_tabbed_utils.h"

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_app {

bool HasPinnedHomeTab(TabStripModel* tab_strip_model) {
  if (!tab_strip_model->ContainsIndex(0))
    return false;
  return tab_strip_model->IsTabPinned(0);
}

bool IsPinnedHomeTab(TabStripModel* tab_strip_model, int index) {
  return HasPinnedHomeTab(tab_strip_model) && index == 0;
}

bool IsPinnedHomeTabUrl(const WebAppRegistrar& registrar,
                        const AppId& app_id,
                        GURL launch_url) {
  if (!registrar.IsTabbedWindowModeEnabled(app_id))
    return false;

  absl::optional<GURL> pinned_home_url =
      registrar.GetAppPinnedHomeTabUrl(app_id);
  if (!pinned_home_url)
    return false;

  // A launch URL which is the home tab URL with query params and
  // hash ref should be opened as the home tab.
  GURL::Replacements replacements;
  replacements.ClearQuery();
  replacements.ClearRef();
  return launch_url.ReplaceComponents(replacements) ==
         pinned_home_url.value().ReplaceComponents(replacements);
}

}  // namespace web_app
