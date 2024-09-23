// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_INPUT_METHOD_INPUT_METHOD_MENU_ITEM_H_
#define CHROME_BROWSER_UI_ASH_INPUT_METHOD_INPUT_METHOD_MENU_ITEM_H_

#include <string>
#include <vector>

#include "ui/chromeos/ui_chromeos_export.h"

namespace ui {
namespace ime {

// A structure which represents a property for an input method engine.
struct UI_CHROMEOS_EXPORT InputMethodMenuItem {
  InputMethodMenuItem(const std::string& in_key,
                      const std::string& in_label,
                      bool in_is_selection_item_checked);

  InputMethodMenuItem();
  ~InputMethodMenuItem();

  // Debug print function.
  std::string ToString() const;

  std::string key;    // A key which identifies the property. Non-empty string.
                      // (e.g. "InputMode.HalfWidthKatakana")
  std::string label;  // A description of the property. Non-empty string.
                      // (e.g. "Switch to full punctuation mode", "Hiragana")
  bool is_selection_item_checked;  // true if the selection_item is selected.
};
typedef std::vector<InputMethodMenuItem> InputMethodMenuItemList;

}  // namespace ime
}  // namespace ui

#endif  // CHROME_BROWSER_UI_ASH_INPUT_METHOD_INPUT_METHOD_MENU_ITEM_H_
