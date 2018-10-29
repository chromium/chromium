// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <set>

#include "base/stl_util.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/global_keyboard_shortcuts_mac.h"
#include "chrome/browser/ui/views/accelerator_table.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_constants.h"
#import "ui/events/keycodes/keyboard_code_conversion_mac.h"

namespace {

void VerifyTableDoesntHaveDuplicates(
    const std::vector<KeyboardShortcutData>& table,
    const std::string& table_name) {
  const std::vector<AcceleratorMapping> accelerators(GetAcceleratorList());

  for (const auto& e : table) {
    int modifiers = 0;
    if (e.command_key)
      modifiers |= ui::EF_COMMAND_DOWN;
    if (e.shift_key)
      modifiers |= ui::EF_SHIFT_DOWN;
    if (e.cntrl_key)
      modifiers |= ui::EF_CONTROL_DOWN;
    if (e.opt_key)
      modifiers |= ui::EF_ALT_DOWN;

    for (const auto& accelerator_entry : accelerators) {
      unichar character;
      unichar shifted_character;
      const int vkey_code = ui::MacKeyCodeForWindowsKeyCode(
          accelerator_entry.keycode, accelerator_entry.modifiers,
          &shifted_character, &character);

      EXPECT_FALSE(modifiers == accelerator_entry.modifiers &&
                   e.chrome_command == accelerator_entry.command_id &&
                   e.vkey_code == vkey_code)
          << "Duplicate command: " << accelerator_entry.command_id
          << " in table " << table_name;
    }
  }
}

}  // namespace

// On macOS, accelerator handling is done by CommandDispatcher. The only
// accelerators allowed to appear in AcceleratorTable are those that don't
// have any modifiers, and thus cannot be interpreted as macOS
// keyEquivalents.
TEST(AcceleratorTableTest, CheckMacOSAccelerators) {
  for (const auto& entry : GetAcceleratorList())
    EXPECT_EQ(0, entry.modifiers);
}

// Verifies that we're not processing any duplicate accelerators in
// global_keyboard_shortcuts_mac.mm functions. Note that the bulk of
// accelerators are defined in MainMenu.xib. We do not check that there is no
// overlap with that.
TEST(AcceleratorTableTest, CheckNoDuplicatesGlobalKeyboardShortcutsMac) {
  VerifyTableDoesntHaveDuplicates(GetShortcutsNotPresentInMainMenu(),
                                  "ShortcutsNotPresentInMainMenu");
}
