// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_KEYBOARD_SHORTCUT_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_KEYBOARD_SHORTCUT_RESULT_H_

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/keyboard_shortcut_data.h"
#include "chromeos/components/string_matching/tokenized_string.h"

namespace app_list {

// TODO(crbug.com/1290682): Finish implementation.
class KeyboardShortcutResult : public ChromeSearchResult {
 public:
  explicit KeyboardShortcutResult(Profile* profile,
                                  const KeyboardShortcutData& data,
                                  double relevance);
  KeyboardShortcutResult(const KeyboardShortcutResult&) = delete;
  KeyboardShortcutResult& operator=(const KeyboardShortcutResult&) = delete;

  ~KeyboardShortcutResult() override;

  // ChromeSearchResult:
  void Open(int event_flags) override;

  // Calculates the shortcut's relevance score. Will return a default score if
  // the query is missing or the target is empty.
  static double CalculateRelevance(
      const chromeos::string_matching::TokenizedString& query_tokenized,
      const std::u16string& target);

  Profile* profile_;

  // The description of the shortcut action e.g. "Dock a window on the right".
  std::u16string description_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_KEYBOARD_SHORTCUT_RESULT_H_
