// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_ACTION_CHIPS_ACTION_CHIPS_MOJO_TEST_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_ACTION_CHIPS_ACTION_CHIPS_MOJO_TEST_UTILS_H_

#include <iosfwd>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom.h"
#include "components/omnibox/browser/suggest_template_info.mojom.h"

namespace action_chips::mojom {
class TabInfo;
class ActionChip;

// Debug-printing functions for the mojo objects.
void PrintTo(const TabInfo& tab, std::ostream* os);
void PrintTo(const TabInfoPtr& tab, std::ostream* os);
void PrintTo(const ActionChip& chip, std::ostream* os);
void PrintTo(const ActionChipPtr& chip, std::ostream* os);

// Creates an unstyled FormattedStringPtr.
suggest_template_info::mojom::FormattedStringPtr CreateFormattedString(
    const std::string& text,
    const std::optional<std::string>& a11y_text = std::nullopt);

// Creates a SuggestTemplateInfoPtr with the fields that action chips use.
suggest_template_info::mojom::SuggestTemplateInfoPtr CreateSuggestTemplateInfo(
    suggest_template_info::mojom::IconType type_icon,
    suggest_template_info::mojom::FormattedStringPtr primary_text,
    suggest_template_info::mojom::FormattedStringPtr secondary_text);

// Creates a vector of ActionChipPtrs from a variable number of chips.
template <typename... Args>
std::vector<ActionChipPtr> MakeActionChipsVector(Args... chips) {
  std::vector<ActionChipPtr> vec;
  vec.reserve(sizeof...(chips));
  (vec.push_back(std::move(chips)), ...);
  return vec;
}

}  // namespace action_chips::mojom

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_ACTION_CHIPS_ACTION_CHIPS_MOJO_TEST_UTILS_H_
