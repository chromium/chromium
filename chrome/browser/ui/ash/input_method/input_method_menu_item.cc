// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/input_method/input_method_menu_item.h"

#include <sstream>

#include "base/check.h"

namespace ui {
namespace ime {

InputMethodMenuItem::InputMethodMenuItem(const std::string& in_key,
                                         const std::string& in_label,
                                         bool in_is_selection_item_checked)
    : key(in_key),
      label(in_label),
      is_selection_item_checked(in_is_selection_item_checked) {
  DCHECK(!key.empty());
}

InputMethodMenuItem::InputMethodMenuItem() : is_selection_item_checked(false) {}

InputMethodMenuItem::~InputMethodMenuItem() {}

std::string InputMethodMenuItem::ToString() const {
  std::stringstream stream;
  stream << "key=" << key << ", label=" << label
         << ", is_selection_item_checked=" << is_selection_item_checked;
  return stream.str();
}

}  // namespace ime
}  // namespace ui
