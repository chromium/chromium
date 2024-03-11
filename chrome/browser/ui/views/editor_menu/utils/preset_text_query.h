// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_UTILS_PRESET_TEXT_QUERY_H_
#define CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_UTILS_PRESET_TEXT_QUERY_H_

#include <string>
#include <vector>

namespace chromeos::editor_menu {

enum class PresetQueryCategory {
  kUnknown = 0,
  kShorten,
  kElaborate,
  kRephrase,
  kFormalize,
  kEmojify,
  kProofread,
};

struct PresetTextQuery {
  PresetTextQuery(std::string_view text_query_id,
                  std::u16string_view name,
                  PresetQueryCategory category);

  std::string text_query_id;
  std::u16string name;
  PresetQueryCategory category;
};

using PresetTextQueries = std::vector<PresetTextQuery>;

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_UTILS_PRESET_TEXT_QUERY_H_
