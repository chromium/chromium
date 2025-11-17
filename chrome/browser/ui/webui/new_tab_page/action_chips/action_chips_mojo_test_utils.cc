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

void PrintTo(const ActionChip& chip, std::ostream* os) {
  *os << "ActionChip{\n"
      << "  title: \"" << chip.title << "\",\n"
      << "  suggestion: \"" << chip.suggestion << "\",\n"
      << "  type: " << chip.type << ",\n"
      << "  tab_info: ";
  if (chip.tab.is_null()) {
    *os << "nullptr";
  } else {
    PrintTo(*chip.tab, os);
  }
  *os << "\n}";
}

void PrintTo(const ActionChipPtr& chip, std::ostream* os) {
  if (chip.is_null()) {
    *os << "nullptr";
  } else {
    PrintTo(*chip, os);
  }
}
}  // namespace action_chips::mojom
