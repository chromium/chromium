// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips_mojo_test_utils.h"

#include <ostream>

#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom.h"

namespace action_chips::mojom {
void PrintTo(const TabInfo& tab, std::ostream* os) {
  *os << "TabInfo{\n"
      << "  tab_id: " << tab.tab_id << ",\n"
      << "  title: \"" << tab.title << "\",\n"
      << "  url: \"" << tab.url << "\",\n"
      << "  last_active_time: " << tab.last_active_time << "\n}"
      << "\n}";
}

void PrintTo(const TabInfoPtr& tab, std::ostream* os) {
  if (tab.is_null()) {
    *os << "nullptr";
  } else {
    PrintTo(*tab, os);
  }
}

void PrintTo(const FormattedString& str, std::ostream* os) {
  *os << "FormattedString{\n"
      << "  text: \"" << str.text << "\",\n";
  if (str.a11y_text) {
    *os << "  a11y_text: \"" << *str.a11y_text << "\"\n";
  } else {
    *os << "  a11y_text: null\n";
  }
  *os << "}";
}

void PrintTo(const FormattedStringPtr& str, std::ostream* os) {
  if (str.is_null()) {
    *os << "nullptr";
  } else {
    PrintTo(*str, os);
  }
}

void PrintTo(const SuggestTemplateInfo& info, std::ostream* os) {
  *os << "SuggestTemplateInfo{\n"
      << "  type_icon: " << info.type_icon << ",\n"
      << "  primary_text: ";
  if (info.primary_text.is_null()) {
    *os << "nullptr";
  } else {
    PrintTo(*info.primary_text, os);
  }
  *os << ",\n"
      << "  secondary_text: ";
  if (info.secondary_text.is_null()) {
    *os << "nullptr";
  } else {
    PrintTo(*info.secondary_text, os);
  }
  *os << "\n}";
}

void PrintTo(const SuggestTemplateInfoPtr& info, std::ostream* os) {
  if (info.is_null()) {
    *os << "nullptr";
  } else {
    PrintTo(*info, os);
  }
}

void PrintTo(const ActionChip& chip, std::ostream* os) {
  *os << "ActionChip{\n"
      << "  suggestion: \"" << chip.suggestion << "\",\n"
      << "  suggest_template_info: ";
  PrintTo(chip.suggest_template_info, os);
  *os << ",\n"
      << "  tab_info: ";
  if (chip.tab.is_null()) {
    *os << "nullptr";
  } else {
    PrintTo(*chip.tab, os);
  }
  *os << ",\n"
      << "  icon_type: " << chip.suggest_template_info->type_icon << "\n}";
}

void PrintTo(const ActionChipPtr& chip, std::ostream* os) {
  if (chip.is_null()) {
    *os << "nullptr";
  } else {
    PrintTo(*chip, os);
  }
}

FormattedStringPtr CreateFormattedString(
    const std::string& text,
    const std::optional<std::string>& a11y_text) {
  return FormattedString::New(text, a11y_text);
}

}  // namespace action_chips::mojom
