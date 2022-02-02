// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_KEYBOARD_SHORTCUT_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_KEYBOARD_SHORTCUT_RESULT_H_

#include "chrome/browser/ui/app_list/search/chrome_search_result.h"

namespace app_list {

// TODO(crbug.com/1290682): Implement.
class KeyboardShortcutResult : public ChromeSearchResult {
 public:
  KeyboardShortcutResult();
  KeyboardShortcutResult(const KeyboardShortcutResult&) = delete;
  KeyboardShortcutResult& operator=(const KeyboardShortcutResult&) = delete;

  ~KeyboardShortcutResult() override;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_KEYBOARD_SHORTCUT_RESULT_H_
