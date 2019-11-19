// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/accelerators_cocoa.h"

#import <Cocoa/Cocoa.h>
#include <stddef.h>

#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/stl_util.h"
#include "chrome/app/chrome_command_ids.h"
#include "printing/buildflags/buildflags.h"
#import "ui/base/accelerators/platform_accelerator_cocoa.h"
#import "ui/events/cocoa/cocoa_event_utils.h"
#import "ui/events/keycodes/keyboard_code_conversion_mac.h"

namespace {

const struct AcceleratorMapping {
  int command_id;
  int modifiers;              // The ui::EventFlag modifiers
  ui::KeyboardCode key_code;  // The key used for cross-platform compatibility.
} kAcceleratorMap[] = {
    // Accelerators used in the toolbar menu.
    {IDC_CLEAR_BROWSING_DATA, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
     ui::VKEY_BACK},
    {IDC_COPY, ui::EF_COMMAND_DOWN, ui::VKEY_C},
    {IDC_CUT, ui::EF_COMMAND_DOWN, ui::VKEY_X},
    {IDC_DEV_TOOLS, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::VKEY_I},
    {IDC_DEV_TOOLS_CONSOLE, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::VKEY_J},
    {IDC_DEV_TOOLS_INSPECT, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::VKEY_C},
    {IDC_FIND, ui::EF_COMMAND_DOWN, ui::VKEY_F},
    {IDC_FULLSCREEN, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN, ui::VKEY_F},
    {IDC_NEW_INCOGNITO_WINDOW, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
     ui::VKEY_N},
    {IDC_NEW_TAB, ui::EF_COMMAND_DOWN, ui::VKEY_T},
    {IDC_NEW_WINDOW, ui::EF_COMMAND_DOWN, ui::VKEY_N},
    {IDC_PASTE, ui::EF_COMMAND_DOWN, ui::VKEY_V},
    {IDC_PRINT, ui::EF_COMMAND_DOWN, ui::VKEY_P},
    {IDC_RESTORE_TAB, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN, ui::VKEY_T},
    {IDC_SAVE_PAGE, ui::EF_COMMAND_DOWN, ui::VKEY_S},
    {IDC_SHOW_BOOKMARK_BAR, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
     ui::VKEY_B},
    {IDC_SHOW_BOOKMARK_MANAGER, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
     ui::VKEY_B},
    {IDC_BOOKMARK_THIS_TAB, ui::EF_COMMAND_DOWN, ui::VKEY_D},
    {IDC_SHOW_DOWNLOADS, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN, ui::VKEY_J},
    {IDC_SHOW_HISTORY, ui::EF_COMMAND_DOWN, ui::VKEY_Y},
    {IDC_VIEW_SOURCE, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::VKEY_U},
    {IDC_ZOOM_MINUS, ui::EF_COMMAND_DOWN, ui::VKEY_OEM_MINUS},
    {IDC_ZOOM_PLUS, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN, ui::VKEY_OEM_PLUS},

    // Accelerators used in the Main Menu, but not the toolbar menu.
    {IDC_OPTIONS, ui::EF_COMMAND_DOWN, ui::VKEY_OEM_COMMA},
    {IDC_HIDE_APP, ui::EF_COMMAND_DOWN, ui::VKEY_H},
    {IDC_EXIT, ui::EF_COMMAND_DOWN, ui::VKEY_Q},
    {IDC_OPEN_FILE, ui::EF_COMMAND_DOWN, ui::VKEY_O},
    {IDC_FOCUS_LOCATION, ui::EF_COMMAND_DOWN, ui::VKEY_L},

    // The key combinations for IDC_CLOSE_WINDOW and IDC_CLOSE_TAB are context
    // dependent. A static mapping doesn't make sense. :(
    {IDC_CLOSE_TAB, ui::EF_COMMAND_DOWN, ui::VKEY_W},
    {IDC_CLOSE_WINDOW, ui::EF_COMMAND_DOWN, ui::VKEY_W},

    {IDC_EMAIL_PAGE_LOCATION, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
     ui::VKEY_I},
#if BUILDFLAG(ENABLE_PRINTING)
    {IDC_BASIC_PRINT, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::VKEY_P},
#endif  // ENABLE_PRINTING
    {IDC_CONTENT_CONTEXT_UNDO, ui::EF_COMMAND_DOWN, ui::VKEY_Z},
    {IDC_CONTENT_CONTEXT_REDO, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
     ui::VKEY_Z},
    {IDC_CONTENT_CONTEXT_CUT, ui::EF_COMMAND_DOWN, ui::VKEY_X},
    {IDC_CONTENT_CONTEXT_COPY, ui::EF_COMMAND_DOWN, ui::VKEY_C},
    {IDC_CONTENT_CONTEXT_PASTE, ui::EF_COMMAND_DOWN, ui::VKEY_V},
    {IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE,
     ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN, ui::VKEY_V},
    {IDC_CONTENT_CONTEXT_SELECTALL, ui::EF_COMMAND_DOWN, ui::VKEY_A},
    {IDC_FOCUS_SEARCH, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::VKEY_F},
    {IDC_FIND_NEXT, ui::EF_COMMAND_DOWN, ui::VKEY_G},
    {IDC_FIND_PREVIOUS, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN, ui::VKEY_G},
    {IDC_ZOOM_PLUS, ui::EF_COMMAND_DOWN, ui::VKEY_OEM_PLUS},
    {IDC_ZOOM_MINUS, ui::EF_COMMAND_DOWN, ui::VKEY_OEM_MINUS},
    {IDC_STOP, ui::EF_COMMAND_DOWN, ui::VKEY_OEM_PERIOD},
    {IDC_RELOAD, ui::EF_COMMAND_DOWN, ui::VKEY_R},
    {IDC_RELOAD_BYPASSING_CACHE, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
     ui::VKEY_R},
    {IDC_ZOOM_NORMAL, ui::EF_COMMAND_DOWN, ui::VKEY_0},
    {IDC_HOME, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN, ui::VKEY_H},
    {IDC_BACK, ui::EF_COMMAND_DOWN, ui::VKEY_OEM_4},
    {IDC_FORWARD, ui::EF_COMMAND_DOWN, ui::VKEY_OEM_6},
    {IDC_BOOKMARK_ALL_TABS, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
     ui::VKEY_D},
    {IDC_MINIMIZE_WINDOW, ui::EF_COMMAND_DOWN, ui::VKEY_M},
    {IDC_SELECT_NEXT_TAB, ui::EF_CONTROL_DOWN, ui::VKEY_TAB},
    {IDC_SELECT_PREVIOUS_TAB, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
     ui::VKEY_TAB},
    {IDC_HELP_PAGE_VIA_MENU, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
     ui::VKEY_OEM_2},
    {IDC_TOGGLE_FULLSCREEN_TOOLBAR, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
     ui::VKEY_F},
};

}  // namespace

AcceleratorsCocoa::AcceleratorsCocoa() {
  for (size_t i = 0; i < base::size(kAcceleratorMap); ++i) {
    const AcceleratorMapping& entry = kAcceleratorMap[i];
    ui::Accelerator accelerator(entry.key_code, entry.modifiers);
    accelerators_.insert(std::make_pair(entry.command_id, accelerator));
  }
}

AcceleratorsCocoa::~AcceleratorsCocoa() {}

// static
AcceleratorsCocoa* AcceleratorsCocoa::GetInstance() {
  return base::Singleton<AcceleratorsCocoa>::get();
}

const ui::Accelerator* AcceleratorsCocoa::GetAcceleratorForCommand(
    int command_id) {
  AcceleratorMap::iterator it = accelerators_.find(command_id);
  if (it == accelerators_.end())
    return NULL;
  return &it->second;
}

