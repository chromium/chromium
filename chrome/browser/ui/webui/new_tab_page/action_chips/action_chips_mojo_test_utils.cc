// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips_mojo_test_utils.h"

#include <ostream>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom.h"
#include "components/omnibox/browser/fusebox_action_mojo_test_utils.h"
#include "components/omnibox/browser/suggest_template_info.mojom.h"

namespace action_chips::mojom {
namespace {

using ::suggest_template_info::mojom::FormattedString;
using ::suggest_template_info::mojom::FormattedStringPtr;
using ::suggest_template_info::mojom::IconType;
using ::suggest_template_info::mojom::SuggestTemplateInfo;
using ::suggest_template_info::mojom::SuggestTemplateInfoPtr;

constexpr int kIndentUnit = 2;

std::string IndentStr(int indent) {
  return std::string(indent, ' ');
}

void PrintImpl(const TabInfo& tab, int indent, std::ostream* os) {
  std::string ind = IndentStr(indent);
  *os << "TabInfo{\n"
      << ind << "  tab_id: " << tab.tab_id << ",\n"
      << ind << "  title: \"" << tab.title << "\",\n"
      << ind << "  url: \"" << tab.url << "\",\n"
      << ind << "  last_active_time: " << tab.last_active_time << "\n"
      << ind << "}";
}

void PrintImpl(const TabInfoPtr& tab, int indent, std::ostream* os) {
  if (tab.is_null()) {
    *os << "nullptr";
  } else {
    PrintImpl(*tab, indent, os);
  }
}

// Prints the FormattedString fields that action chips use.
void PrintImpl(const FormattedString& str, int indent, std::ostream* os) {
  std::string ind = IndentStr(indent);
  *os << "FormattedString{\n"
      << ind << "  text: \"" << base::UTF16ToUTF8(str.text) << "\",\n";
  if (str.a11y_text) {
    *os << ind << "  a11y_text: \"" << base::UTF16ToUTF8(*str.a11y_text)
        << "\"\n";
  } else {
    *os << ind << "  a11y_text: null\n";
  }
  *os << ind << "}";
}

void PrintImpl(const FormattedStringPtr& str, int indent, std::ostream* os) {
  if (str.is_null()) {
    *os << "nullptr";
  } else {
    PrintImpl(*str, indent, os);
  }
}

// Prints the SuggestTemplateInfo fields that action chips use.
void PrintImpl(const SuggestTemplateInfo& info, int indent, std::ostream* os) {
  std::string ind = IndentStr(indent);
  *os << "SuggestTemplateInfo{\n"
      << ind << "  type_icon: " << info.type_icon << ",\n"
      << ind << "  primary_text: ";
  PrintImpl(info.primary_text, indent + kIndentUnit, os);
  *os << ",\n" << ind << "  secondary_text: ";
  PrintImpl(info.secondary_text, indent + kIndentUnit, os);
  *os << ",\n" << ind << "  fusebox_action: ";
  fusebox_action::mojom::PrintTo(info.fusebox_action, os);
  *os << "\n" << ind << "}";
}

void PrintImpl(const SuggestTemplateInfoPtr& info,
               int indent,
               std::ostream* os) {
  if (info.is_null()) {
    *os << "nullptr";
  } else {
    PrintImpl(*info, indent, os);
  }
}

void PrintImpl(const ActionChip& chip, int indent, std::ostream* os) {
  std::string ind = IndentStr(indent);
  *os << "ActionChip{\n"
      << ind << "  suggestion: \"" << chip.suggestion << "\",\n"
      << ind << "  suggest_template_info: ";
  PrintImpl(chip.suggest_template_info, indent + kIndentUnit, os);
  *os << ",\n" << ind << "  tab_info: ";
  PrintImpl(chip.tab, indent + kIndentUnit, os);
  *os << "\n" << ind << "}";
}

void PrintImpl(const ActionChipPtr& chip, int indent, std::ostream* os) {
  if (chip.is_null()) {
    *os << "nullptr";
  } else {
    PrintImpl(*chip, indent, os);
  }
}

}  // namespace

void PrintTo(const TabInfo& tab, std::ostream* os) {
  PrintImpl(tab, 0, os);
}

void PrintTo(const TabInfoPtr& tab, std::ostream* os) {
  PrintImpl(tab, 0, os);
}

void PrintTo(const ActionChip& chip, std::ostream* os) {
  PrintImpl(chip, 0, os);
}

void PrintTo(const ActionChipPtr& chip, std::ostream* os) {
  PrintImpl(chip, 0, os);
}

FormattedStringPtr CreateFormattedString(
    const std::string& text,
    const std::optional<std::string>& a11y_text) {
  FormattedStringPtr str = FormattedString::New();
  str->text = base::UTF8ToUTF16(text);
  if (a11y_text) {
    str->a11y_text = base::UTF8ToUTF16(*a11y_text);
  }
  return str;
}

SuggestTemplateInfoPtr CreateSuggestTemplateInfo(
    IconType type_icon,
    FormattedStringPtr primary_text,
    FormattedStringPtr secondary_text) {
  SuggestTemplateInfoPtr info = SuggestTemplateInfo::New();
  info->type_icon = type_icon;
  info->primary_text = std::move(primary_text);
  info->secondary_text = std::move(secondary_text);
  return info;
}

}  // namespace action_chips::mojom
