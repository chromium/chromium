// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_UTILS_PRESET_TEXT_QUERY_H_
#define CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_UTILS_PRESET_TEXT_QUERY_H_

#include <string>
#include <vector>

namespace gfx {
struct VectorIcon;
}

namespace chromeos::editor_menu {

// Categories of preset text prompts to be shown on editor menu chips.
enum class PresetQueryCategory {
  kUnknown = 0,
  kShorten,
  kElaborate,
  kRephrase,
  kFormalize,
  kEmojify,
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

const gfx::VectorIcon& GetIconForPresetQueryCategory(
    PresetQueryCategory category);

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_UTILS_PRESET_TEXT_QUERY_H_
