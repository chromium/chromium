// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_interactive_uitest_base.h"

#include "base/test/bind.h"
#include "chrome/browser/ui/views/profiles/profile_picker_test_base.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/widget/widget.h"

void WithProfilePickerInteractiveUiTestHelpers::
    SendCloseWindowKeyboardCommand() {
  // Close window using keyboard.
#if BUILDFLAG(IS_MAC)
  // Use Cmd-W on Mac.
  bool control = false;
  bool shift = false;
  bool command = true;
#else
  // Use Ctrl-Shift-W on other platforms.
  bool control = true;
  bool shift = true;
  bool command = false;
#endif
  SendKeyPress(ui::VKEY_W, control, shift, /*alt=*/false, command);
}

void WithProfilePickerInteractiveUiTestHelpers::SendBackKeyboardCommand() {
  // Close window using keyboard.
#if BUILDFLAG(IS_MAC)
  // Use Cmd-[ on Mac.
  bool alt = false;
  bool command = true;
  ui::KeyboardCode key = ui::VKEY_OEM_4;
#else
  // Use Ctrl-left on other platforms.
  bool alt = true;
  bool command = false;
  ui::KeyboardCode key = ui::VKEY_LEFT;
#endif
  SendKeyPress(key, /*control=*/false, /*shift=*/false, alt, command);
}

void WithProfilePickerInteractiveUiTestHelpers::
    SendToggleFullscreenKeyboardCommand() {
// Toggle fullscreen with keyboard.
#if BUILDFLAG(IS_MAC)
  // Use Cmd-Ctrl-F on Mac.
  bool control = true;
  bool command = true;
  ui::KeyboardCode key_code = ui::VKEY_F;
#else
  // Use F11 on other platforms.
  bool control = false;
  bool command = false;
  ui::KeyboardCode key_code = ui::VKEY_F11;
#endif
  SendKeyPress(key_code, control, /*shift=*/false, /*alt=*/false, command);
}

#if BUILDFLAG(IS_MAC)
void WithProfilePickerInteractiveUiTestHelpers::SendQuitAppKeyboardCommand() {
  // Send Cmd-Q.
  SendKeyPress(ui::VKEY_Q, /*control=*/false, /*shift=*/false, /*alt=*/false,
               /*command=*/true);
}
#endif

void WithProfilePickerInteractiveUiTestHelpers::SendKeyPress(
    ui::KeyboardCode key,
    bool control,
    bool shift,
    bool alt,
    bool command) {
#if BUILDFLAG(IS_MAC)
  // Mac needs the widget to get focused (once again) for
  // SendKeyPressToWindowSync to work. A test-only particularity, pressing the
  // keybinding manually right in the run of the test actually replaces the
  // need of this call.
  ASSERT_TRUE(
      ui_test_utils::ShowAndFocusNativeWindow(widget()->GetNativeWindow()));
#endif
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      widget()->GetNativeWindow(), key, control, shift, alt, command));
}
