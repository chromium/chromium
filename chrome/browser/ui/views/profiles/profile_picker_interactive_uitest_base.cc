// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_interactive_uitest_base.h"

#include "base/notreached.h"
#include "base/test/bind.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/views/profiles/profile_picker_test_base.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/widget/widget.h"

ui::Accelerator WithProfilePickerInteractiveUiTestHelpers::GetAccelerator(
    int command_id) {
  // TODO(crbug.com/40911656): Rely on `AcceleratorProvider` instead of
  // hardcoding the accelerators here.

  switch (command_id) {
    case IDC_CLOSE_WINDOW:
      // Sending accelerators for CLOSE_TAB instead, as although CLOSE_WINDOW is
      // registered on other platforms, it is not on mac
#if BUILDFLAG(IS_MAC)
      return {ui::VKEY_W, ui::EF_COMMAND_DOWN};  // Cmd-W
#else
      return {ui::VKEY_W, ui::EF_CONTROL_DOWN};  // Ctrl-W
#endif

    case IDC_BACK:
#if BUILDFLAG(IS_MAC)
      return {ui::VKEY_OEM_4, ui::EF_COMMAND_DOWN};  // Cmd-[
#else
      return {ui::VKEY_LEFT, ui::EF_ALT_DOWN};   // Alt-left
#endif

    case IDC_FULLSCREEN:
#if BUILDFLAG(IS_MAC)
      return {ui::VKEY_F,
              ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN};  // Cmd-Ctrl-F.
#else
      return {ui::VKEY_F11, ui::EF_NONE};        // F11.
#endif

#if BUILDFLAG(IS_MAC)
    case IDC_EXIT:
      return {ui::VKEY_Q, ui::EF_COMMAND_DOWN};  // Cmd-Q.
#endif

    default:
      NOTREACHED() << "Unexpected command_id: " << command_id;
  }
}

void WithProfilePickerInteractiveUiTestHelpers::
    SendCloseWindowKeyboardCommand() {
  SendKeyPress(GetAccelerator(IDC_CLOSE_WINDOW));
}

void WithProfilePickerInteractiveUiTestHelpers::
    SendToggleFullscreenKeyboardCommand() {
  SendKeyPress(GetAccelerator(IDC_FULLSCREEN));
}

#if BUILDFLAG(IS_MAC)
void WithProfilePickerInteractiveUiTestHelpers::SendQuitAppKeyboardCommand() {
  SendKeyPress(GetAccelerator(IDC_EXIT));
}
#endif

void WithProfilePickerInteractiveUiTestHelpers::SendKeyPress(
    ui::Accelerator accelerator) {
  SendKeyPress(accelerator.key_code(), accelerator.IsCtrlDown(),
               accelerator.IsShiftDown(), accelerator.IsAltDown(),
               accelerator.IsCmdDown());
}

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
