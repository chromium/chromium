// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SETTINGS_SHORTCUT_SETTINGS_SHORTCUT_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SETTINGS_SHORTCUT_SETTINGS_SHORTCUT_RESULT_H_

#include <memory>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "base/macros.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"

class Profile;

namespace app_list {

struct SettingsShortcut;

// Result of SettingsShortcutProvider.
class SettingsShortcutResult : public ChromeSearchResult {
 public:
  SettingsShortcutResult(Profile* profile,
                         const SettingsShortcut& settings_shortcut);

  ~SettingsShortcutResult() override = default;

  // SearchResult overrides:
  void Open(int event_flags) override;
  void GetContextMenuModel(GetMenuModelCallback callback) override;
  ash::SearchResultType GetSearchResultType() const override;

 private:
  Profile* const profile_;

  const SettingsShortcut& settings_shortcut_;

  DISALLOW_COPY_AND_ASSIGN(SettingsShortcutResult);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SETTINGS_SHORTCUT_SETTINGS_SHORTCUT_RESULT_H_
