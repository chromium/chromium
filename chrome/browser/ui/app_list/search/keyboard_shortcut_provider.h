// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_KEYBOARD_SHORTCUT_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_KEYBOARD_SHORTCUT_PROVIDER_H_

#include "chrome/browser/ui/app_list/search/search_provider.h"

class Profile;

namespace app_list {

class KeyboardShortcutProvider : public SearchProvider {
 public:
  explicit KeyboardShortcutProvider(Profile* profile);
  ~KeyboardShortcutProvider() override;

  KeyboardShortcutProvider(const KeyboardShortcutProvider&) = delete;
  KeyboardShortcutProvider& operator=(const KeyboardShortcutProvider&) = delete;

  // SearchProvider:
  void Start(const std::u16string& query) override;
  ash::AppListSearchResultType ResultType() const override;

 private:
  Profile* const profile_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_KEYBOARD_SHORTCUT_PROVIDER_H_
