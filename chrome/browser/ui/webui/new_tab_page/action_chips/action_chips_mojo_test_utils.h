// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_ACTION_CHIPS_ACTION_CHIPS_MOJO_TEST_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_ACTION_CHIPS_ACTION_CHIPS_MOJO_TEST_UTILS_H_

#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom.h"

namespace action_chips::mojom {
class TabInfo;
class ActionChip;
class FormattedString;
class SuggestTemplateInfo;

// Debug-printing functions for the mojo objects.
void PrintTo(const TabInfo& tab, std::ostream* os);
void PrintTo(const TabInfoPtr& tab, std::ostream* os);
void PrintTo(const FormattedString& str, std::ostream* os);
void PrintTo(const FormattedStringPtr& str, std::ostream* os);
void PrintTo(const SuggestTemplateInfo& info, std::ostream* os);
void PrintTo(const SuggestTemplateInfoPtr& info, std::ostream* os);
void PrintTo(const ActionChip& chip, std::ostream* os);
void PrintTo(const ActionChipPtr& chip, std::ostream* os);

// Creates a FormattedStringPtr.
FormattedStringPtr CreateFormattedString(
    const std::string& text,
    const std::optional<std::string>& a11y_text = std::nullopt);

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
