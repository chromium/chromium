// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SETTINGS_SHORTCUT_SETTINGS_SHORTCUT_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SETTINGS_SHORTCUT_SETTINGS_SHORTCUT_PROVIDER_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"

class Profile;

namespace app_list {

// Search provider for Settings shortcut.
class SettingsShortcutProvider : public SearchProvider {
 public:
  explicit SettingsShortcutProvider(Profile* profile);

  ~SettingsShortcutProvider() override = default;

  // SearchProvider overrides:
  void Start(const std::u16string& query) override;
  ash::AppListSearchResultType ResultType() override;

 private:
  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(SettingsShortcutProvider);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SETTINGS_SHORTCUT_SETTINGS_SHORTCUT_PROVIDER_H_
