// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accelerator_table.h"

#include <stddef.h>

#include <vector>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "components/lens/buildflags.h"
#include "components/lens/lens_features.h"
#include "printing/buildflags/buildflags.h"
#include "services/screen_ai/buildflags/buildflags.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/event_constants.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/browser_util.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

// For ChromeOS only: If you plan on adding a new accelerator and want it
// displayed in the Shortcuts app, please follow the instructions at:
// `ash/webui/shortcut_customization_ui/backend/accelerator_layout_table.h`.

// NOTE: Between each ifdef block, keep the list in the same
// (mostly-alphabetical) order as the Windows accelerators in
// ../../app/chrome_dll.rc.
// Do not use Ctrl-Alt as a shortcut modifier, as it is used by i18n keyboards:
// http://blogs.msdn.com/b/oldnewthing/archive/2004/03/29/101121.aspx
const AcceleratorMapping kAcceleratorMap[] = {
// To add an accelerator to macOS that uses modifier keys, either:
//   1) Update the main menu built in main_menu_builder.mm to include a new menu
//      item with the appropriate modifier.
//   2) Update GetShortcutsNotPresentInMainMenu() in
//      global_keyboard_shortcuts_mac.mm.
#if !BUILDFLAG(IS_CHROMEOS)
    {ui::VKEY_F7, ui::EF_NONE, IDC_CARET_BROWSING_TOGGLE},
#endif
    {ui::VKEY_ESCAPE, ui::EF_NONE, IDC_CLOSE_FIND_OR_STOP},

#if !BUILDFLAG(IS_MAC)
    {ui::VKEY_D, ui::EF_PLATFORM_ACCELERATOR, IDC_BOOKMARK_THIS_TAB},
    {ui::VKEY_D, ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR,
     IDC_BOOKMARK_ALL_TABS},
    {ui::VKEY_W, ui::EF_PLATFORM_ACCELERATOR, IDC_CLOSE_TAB},
    {ui::VKEY_W, ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR,
     IDC_CLOSE_WINDOW},
    {ui::VKEY_F, ui::EF_PLATFORM_ACCELERATOR, IDC_FIND},
    {ui::VKEY_A, ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR,
     IDC_TAB_SEARCH},
    {ui::VKEY_G, ui::EF_PLATFORM_ACCELERATOR, IDC_FIND_NEXT},
    {ui::VKEY_G, ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR,
     IDC_FIND_PREVIOUS},
    {ui::VKEY_L, ui::EF_PLATFORM_ACCELERATOR, IDC_FOCUS_LOCATION},
    {ui::VKEY_O, ui::EF_PLATFORM_ACCELERATOR, IDC_OPEN_FILE},
    {ui::VKEY_P, ui::EF_PLATFORM_ACCELERATOR, IDC_PRINT},
    {ui::VKEY_R, ui::EF_PLATFORM_ACCELERATOR, IDC_RELOAD},
    {ui::VKEY_R, ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR,
     IDC_RELOAD_BYPASSING_CACHE},
    {ui::VKEY_S, ui::EF_PLATFORM_ACCELERATOR, IDC_SAVE_PAGE},
    {ui::VKEY_9, ui::EF_PLATFORM_ACCELERATOR, IDC_SELECT_LAST_TAB},
    {ui::VKEY_NUMPAD9, ui::EF_PLATFORM_ACCELERATOR, IDC_SELECT_LAST_TAB},
#if BUILDFLAG(IS_LINUX)
    {ui::VKEY_9, ui::EF_ALT_DOWN, IDC_SELECT_LAST_TAB},
    {ui::VKEY_NUMPAD9, ui::EF_ALT_DOWN, IDC_SELECT_LAST_TAB},
#endif  // BUILDFLAG(IS_LINUX)
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
    {ui::VKEY_NEXT, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN, IDC_MOVE_TAB_NEXT},
    {ui::VKEY_PRIOR, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
     IDC_MOVE_TAB_PREVIOUS},
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
    // Control modifier is rarely used on Mac, so we allow it only in several
    // specific cases.
    {ui::VKEY_TAB, ui::EF_CONTROL_DOWN, IDC_SELECT_NEXT_TAB},
    {ui::VKEY_NEXT, ui::EF_CONTROL_DOWN, IDC_SELECT_NEXT_TAB},
    {ui::VKEY_TAB, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
     IDC_SELECT_PREVIOUS_TAB},
    {ui::VKEY_PRIOR, ui::EF_CONTROL_DOWN, IDC_SELECT_PREVIOUS_TAB},
    {ui::VKEY_1, ui::EF_PLATFORM_ACCELERATOR, IDC_SELECT_TAB_0},
    {ui::VKEY_NUMPAD1, ui::EF_PLATFORM_ACCELERATOR, IDC_SELECT_TAB_0},
    {ui::VKEY_2, ui::EF_PLATFORM_ACCELERATOR, IDC_SELECT_TAB_1},
    {ui::VKEY_NUMPAD2, ui::EF_PLATFORM_ACCELERATOR, IDC_SELECT_TAB_1},
    {ui::VKEY_3, ui::EF_PLATFORM_ACCELERATOR, IDC_SELECT_TAB_2},
    {ui::VKEY_NUMPAD3, ui::EF_PLATFORM_ACCELERATOR, IDC_SELECT_TAB_2},
    {ui::VKEY_4, ui::EF_PLATFORM_ACCELERATOR, IDC_SELECT_TAB_3},
    {ui::VKEY_NUMPAD4, ui::EF_PLATFORM_ACCELERATOR, IDC_SELECT_TAB_3},
    {ui::VKEY_5, ui::EF_PLATFORM_ACCELERATOR, IDC_SELECT_TAB_4},
    {ui::VKEY_NUMPAD5, ui::EF_PLATFORM_ACCELERATOR, IDC_SELECT_TAB_4},
    {ui::VKEY_6, ui::EF_PLATFORM_ACCELERATOR, IDC_SELECT_TAB_5},
    {ui::VKEY_NUMPAD6, ui::EF_PLATFORM_ACCELERATOR, IDC_SELECT_TAB_5},
    {ui::VKEY_7, ui::EF_PLATFORM_ACCELERATOR, IDC_SELECT_TAB_6},
    {ui::VKEY_NUMPAD7, ui::EF_PLATFORM_ACCELERATOR, IDC_SELECT_TAB_6},
    {ui::VKEY_8, ui::EF_PLATFORM_ACCELERATOR, IDC_SELECT_TAB_7},
    {ui::VKEY_NUMPAD8, ui::EF_PLATFORM_ACCELERATOR, IDC_SELECT_TAB_7},
#if BUILDFLAG(IS_LINUX)
    {ui::VKEY_1, ui::EF_ALT_DOWN, IDC_SELECT_TAB_0},
    {ui::VKEY_NUMPAD1, ui::EF_ALT_DOWN, IDC_SELECT_TAB_0},
    {ui::VKEY_2, ui::EF_ALT_DOWN, IDC_SELECT_TAB_1},
    {ui::VKEY_NUMPAD2, ui::EF_ALT_DOWN, IDC_SELECT_TAB_1},
    {ui::VKEY_3, ui::EF_ALT_DOWN, IDC_SELECT_TAB_2},
    {ui::VKEY_NUMPAD3, ui::EF_ALT_DOWN, IDC_SELECT_TAB_2},
    {ui::VKEY_4, ui::EF_ALT_DOWN, IDC_SELECT_TAB_3},
    {ui::VKEY_NUMPAD4, ui::EF_ALT_DOWN, IDC_SELECT_TAB_3},
    {ui::VKEY_5, ui::EF_ALT_DOWN, IDC_SELECT_TAB_4},
    {ui::VKEY_NUMPAD5, ui::EF_ALT_DOWN, IDC_SELECT_TAB_4},
    {ui::VKEY_6, ui::EF_ALT_DOWN, IDC_SELECT_TAB_5},
    {ui::VKEY_NUMPAD6, ui::EF_ALT_DOWN, IDC_SELECT_TAB_5},
    {ui::VKEY_7, ui::EF_ALT_DOWN, IDC_SELECT_TAB_6},
    {ui::VKEY_NUMPAD7, ui::EF_ALT_DOWN, IDC_SELECT_TAB_6},
    {ui::VKEY_8, ui::EF_ALT_DOWN, IDC_SELECT_TAB_7},
    {ui::VKEY_NUMPAD8, ui::EF_ALT_DOWN, IDC_SELECT_TAB_7},
    {ui::VKEY_BROWSER_FAVORITES, ui::EF_NONE, IDC_SHOW_BOOKMARK_BAR},
#endif  // BUILDFLAG(IS_LINUX)
    {ui::VKEY_B, ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR,
     IDC_SHOW_BOOKMARK_BAR},
    {ui::VKEY_OEM_MINUS, ui::EF_PLATFORM_ACCELERATOR, IDC_ZOOM_MINUS},
    {ui::VKEY_SUBTRACT, ui::EF_PLATFORM_ACCELERATOR, IDC_ZOOM_MINUS},
    {ui::VKEY_0, ui::EF_PLATFORM_ACCELERATOR, IDC_ZOOM_NORMAL},
    {ui::VKEY_NUMPAD0, ui::EF_PLATFORM_ACCELERATOR, IDC_ZOOM_NORMAL},
    {ui::VKEY_OEM_PLUS, ui::EF_PLATFORM_ACCELERATOR, IDC_ZOOM_PLUS},
    {ui::VKEY_ADD, ui::EF_PLATFORM_ACCELERATOR, IDC_ZOOM_PLUS},

    {ui::VKEY_F1, ui::EF_NONE, IDC_HELP_PAGE_VIA_KEYBOARD},
    {ui::VKEY_F3, ui::EF_NONE, IDC_FIND_NEXT},
    {ui::VKEY_F3, ui::EF_SHIFT_DOWN, IDC_FIND_PREVIOUS},
    {ui::VKEY_F4, ui::EF_CONTROL_DOWN, IDC_CLOSE_TAB},
    {ui::VKEY_F4, ui::EF_ALT_DOWN, IDC_CLOSE_WINDOW},
    {ui::VKEY_F5, ui::EF_NONE, IDC_RELOAD},
    {ui::VKEY_F5, ui::EF_CONTROL_DOWN, IDC_RELOAD_BYPASSING_CACHE},
    {ui::VKEY_F5, ui::EF_SHIFT_DOWN, IDC_RELOAD_BYPASSING_CACHE},
    {ui::VKEY_F6, ui::EF_NONE, IDC_FOCUS_NEXT_PANE},
    {ui::VKEY_F6, ui::EF_SHIFT_DOWN, IDC_FOCUS_PREVIOUS_PANE},
    {ui::VKEY_F6, ui::EF_CONTROL_DOWN, IDC_FOCUS_WEB_CONTENTS_PANE},
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // On Chrome OS, Control + Search + the seventh key from escape (most
    // commonly Brightness Up) toggles caret browsing.
    // Note that VKEY_F7 is not a typo; Search + the seventh function key maps
    // to F7 for accelerators.
    {ui::VKEY_F7, ui::EF_CONTROL_DOWN, IDC_CARET_BROWSING_TOGGLE},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    {ui::VKEY_F10, ui::EF_NONE, IDC_FOCUS_MENU_BAR},
    {ui::VKEY_F11, ui::EF_NONE, IDC_FULLSCREEN},
    {ui::VKEY_M, ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR,
     IDC_SHOW_AVATAR_MENU},

// Platform-specific key maps.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    {ui::VKEY_BROWSER_BACK, ui::EF_NONE, IDC_BACK},
    {ui::VKEY_BROWSER_FORWARD, ui::EF_NONE, IDC_FORWARD},
    {ui::VKEY_BROWSER_HOME, ui::EF_NONE, IDC_HOME},
    {ui::VKEY_BROWSER_REFRESH, ui::EF_NONE, IDC_RELOAD},
    {ui::VKEY_BROWSER_REFRESH, ui::EF_CONTROL_DOWN, IDC_RELOAD_BYPASSING_CACHE},
    {ui::VKEY_BROWSER_REFRESH, ui::EF_SHIFT_DOWN, IDC_RELOAD_BYPASSING_CACHE},
    {ui::VKEY_CLOSE, ui::EF_NONE, IDC_CLOSE_TAB},
    {ui::VKEY_NEW, ui::EF_NONE, IDC_NEW_TAB},
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
    // Chrome OS supports the print key, however XKB conflates the print
    // and printscreen keys together so it is not supported on Linux.
    // See crbug.com/683097
    {ui::VKEY_PRINT, ui::EF_NONE, IDC_PRINT},
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
    // Chrome OS supports search-based shortcut to open feedback app.
    {ui::VKEY_I, ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN, IDC_FEEDBACK},
#endif  // BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(IS_CHROMEOS)
    // Chrome OS keyboard does not have delete key, so assign it to backspace.
    {ui::VKEY_BACK, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
     IDC_CLEAR_BROWSING_DATA},
#else   // !BUILDFLAG(IS_CHROMEOS)
    {ui::VKEY_DELETE, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
     IDC_CLEAR_BROWSING_DATA},
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
    // On Chrome OS, VKEY_BROWSER_SEARCH is handled in Ash.
    {ui::VKEY_OEM_2, ui::EF_CONTROL_DOWN, IDC_HELP_PAGE_VIA_KEYBOARD},
    {ui::VKEY_OEM_2, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
     IDC_HELP_PAGE_VIA_KEYBOARD},
    {ui::VKEY_BROWSER_FAVORITES, ui::EF_NONE, IDC_SHOW_BOOKMARK_MANAGER},
    {ui::VKEY_BROWSER_STOP, ui::EF_NONE, IDC_STOP},
    // On Chrome OS, Search + Esc is used to call out task manager.
    {ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN, IDC_TASK_MANAGER},
    {ui::VKEY_Z, ui::EF_COMMAND_DOWN, IDC_TOGGLE_MULTITASK_MENU},
#else   // BUILDFLAG(IS_CHROMEOS)
    {ui::VKEY_ESCAPE, ui::EF_SHIFT_DOWN, IDC_TASK_MANAGER},
    {ui::VKEY_LMENU, ui::EF_NONE, IDC_FOCUS_MENU_BAR},
    {ui::VKEY_MENU, ui::EF_NONE, IDC_FOCUS_MENU_BAR},
    {ui::VKEY_RMENU, ui::EF_NONE, IDC_FOCUS_MENU_BAR},
    // On Windows, all VKEY_BROWSER_* keys except VKEY_BROWSER_SEARCH are
    // handled via WM_APPCOMMAND.
    {ui::VKEY_BROWSER_SEARCH, ui::EF_NONE, IDC_FOCUS_SEARCH},
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    {ui::VKEY_I, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, IDC_FEEDBACK},
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
    {ui::VKEY_N, ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR,
     IDC_NEW_INCOGNITO_WINDOW},
    {ui::VKEY_T, ui::EF_PLATFORM_ACCELERATOR, IDC_NEW_TAB},
    {ui::VKEY_N, ui::EF_PLATFORM_ACCELERATOR, IDC_NEW_WINDOW},
    {ui::VKEY_T, ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR,
     IDC_RESTORE_TAB},

    // Alt by itself (or with just shift) is never used on Mac since it's used
    // to generate non-ASCII characters. Such commands are given Mac-specific
    // bindings as well. Mapping with just Alt appear here, and should have an
    // alternative mapping in the block above.
    {ui::VKEY_LEFT, ui::EF_ALT_DOWN, IDC_BACK},
    {ui::VKEY_LEFT, ui::EF_ALTGR_DOWN, IDC_BACK},
#if BUILDFLAG(ENABLE_PRINTING)
    {ui::VKEY_P, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, IDC_BASIC_PRINT},
#endif  // ENABLE_PRINTING
    {ui::VKEY_B, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, IDC_FOCUS_BOOKMARKS},
    {ui::VKEY_A, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
     IDC_FOCUS_INACTIVE_POPUP_FOR_ACCESSIBILITY},
    {ui::VKEY_D, ui::EF_ALT_DOWN, IDC_FOCUS_LOCATION},
    {ui::VKEY_E, ui::EF_CONTROL_DOWN, IDC_FOCUS_SEARCH},
    {ui::VKEY_K, ui::EF_CONTROL_DOWN, IDC_FOCUS_SEARCH},
    {ui::VKEY_T, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, IDC_FOCUS_TOOLBAR},
    {ui::VKEY_RIGHT, ui::EF_ALT_DOWN, IDC_FORWARD},
    {ui::VKEY_RIGHT, ui::EF_ALTGR_DOWN, IDC_FORWARD},
    {ui::VKEY_HOME, ui::EF_ALT_DOWN, IDC_HOME},
    {ui::VKEY_E, ui::EF_ALT_DOWN, IDC_SHOW_APP_MENU},
    {ui::VKEY_F, ui::EF_ALT_DOWN, IDC_SHOW_APP_MENU},
    {ui::VKEY_O, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
     IDC_SHOW_BOOKMARK_MANAGER},
    {ui::VKEY_J, ui::EF_CONTROL_DOWN, IDC_SHOW_DOWNLOADS},
    {ui::VKEY_H, ui::EF_CONTROL_DOWN, IDC_SHOW_HISTORY},
#if !BUILDFLAG(IS_CHROMEOS)
    // On Chrome OS, these keys are assigned to change UI scale.
    {ui::VKEY_OEM_MINUS, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
     IDC_ZOOM_MINUS},
    {ui::VKEY_OEM_PLUS, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, IDC_ZOOM_PLUS},
#endif  // !BUILDFLAG(IS_CHROMEOS)
#endif  // !BUILDFLAG(IS_MAC)
};

const AcceleratorMapping kDevToolsAcceleratorMap[] = {
    {ui::VKEY_F12, ui::EF_NONE, IDC_DEV_TOOLS_TOGGLE},
#if !BUILDFLAG(IS_MAC)
    {ui::VKEY_I, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, IDC_DEV_TOOLS},
    {ui::VKEY_J, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
     IDC_DEV_TOOLS_CONSOLE},
    {ui::VKEY_C, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
     IDC_DEV_TOOLS_INSPECT},
    {ui::VKEY_U, ui::EF_CONTROL_DOWN, IDC_VIEW_SOURCE},
#endif  // !BUILDFLAG(IS_MAC)
};

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
// Accelerators to enable if lens::features::kEnableRegionSearchKeyboardShortcut
// is true.
constexpr AcceleratorMapping kRegionSearchAcceleratorMap[] = {
    // TODO(nguyenbryan): This is a temporary hotkey; update when finalized.
    {ui::VKEY_E, ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR,
     IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH},
};
#endif  // BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)

constexpr int kDebugModifier =
    ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN;

// Accelerators to enable if features::UIDebugTools is true.
constexpr AcceleratorMapping kUIDebugAcceleratorMap[] = {
    {ui::VKEY_T, kDebugModifier, IDC_DEBUG_TOGGLE_TABLET_MODE},
    {ui::VKEY_V, kDebugModifier, IDC_DEBUG_PRINT_VIEW_TREE},
    {ui::VKEY_M, kDebugModifier, IDC_DEBUG_PRINT_VIEW_TREE_DETAILS},
};

const int kRepeatableCommandIds[] = {
    IDC_FIND_NEXT,           IDC_FIND_PREVIOUS,       IDC_FOCUS_NEXT_PANE,
    IDC_FOCUS_PREVIOUS_PANE, IDC_MOVE_TAB_NEXT,       IDC_MOVE_TAB_PREVIOUS,
    IDC_SELECT_NEXT_TAB,     IDC_SELECT_PREVIOUS_TAB,
};

std::vector<AcceleratorMapping>* GetAcceleratorsPointer() {
  static base::NoDestructor<std::vector<AcceleratorMapping>> accelerators;
  return accelerators.get();
}

}  // namespace

std::vector<AcceleratorMapping> GetAcceleratorList() {
  std::vector<AcceleratorMapping>* accelerators = GetAcceleratorsPointer();

  if (accelerators->empty()) {
    accelerators->insert(accelerators->begin(), std::begin(kAcceleratorMap),
                         std::end(kAcceleratorMap));

    bool enable_devtools = true;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // In Ash, DevTools is disabled by default if lacros is the only browser, in
    // order not to confuse users by opening Ash browser windows.
    enable_devtools = crosapi::browser_util::IsAshDevToolEnabled();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    if (enable_devtools) {
      accelerators->insert(accelerators->begin(),
                           std::begin(kDevToolsAcceleratorMap),
                           std::end(kDevToolsAcceleratorMap));
    }

    // See https://devblogs.microsoft.com/oldnewthing/20040329-00/?p=40003
    // Doing this check here and not at the bottom since kUIDebugAcceleratorMap
    // contains Ctrl+Alt keys but we don't enable those for the public.
#if DCHECK_IS_ON()
    constexpr int kCtrlAlt = ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN;
    for (auto& mapping : *accelerators)
      DCHECK((mapping.modifiers & kCtrlAlt) != kCtrlAlt)
          << "Accelerators with Ctrl+Alt are reserved by Windows.";
#endif

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
    if (base::FeatureList::IsEnabled(
            lens::features::kEnableRegionSearchKeyboardShortcut)) {
      accelerators->insert(accelerators->begin(),
                           std::begin(kRegionSearchAcceleratorMap),
                           std::end(kRegionSearchAcceleratorMap));
    }
#endif  // BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)

    if (base::FeatureList::IsEnabled(features::kUIDebugTools)) {
      accelerators->insert(accelerators->begin(),
                           std::begin(kUIDebugAcceleratorMap),
                           std::end(kUIDebugAcceleratorMap));
    }
  }

  return *accelerators;
}

void ClearAcceleratorListForTesting() {
  std::vector<AcceleratorMapping>* accelerators = GetAcceleratorsPointer();
  accelerators->clear();
}

bool GetStandardAcceleratorForCommandId(int command_id,
                                        ui::Accelerator* accelerator) {
#if BUILDFLAG(IS_MAC)
  // On macOS, the cut/copy/paste accelerators are defined in the main menu
  // built in main_menu_builder.mm and the accelerator is user configurable. All
  // of this is handled by CommandDispatcher.
  NOTREACHED();
#else
  // The standard Ctrl-X, Ctrl-V and Ctrl-C are not defined as accelerators
  // anywhere else.
  switch (command_id) {
    case IDC_CUT:
      *accelerator = ui::Accelerator(ui::VKEY_X, ui::EF_PLATFORM_ACCELERATOR);
      return true;
    case IDC_COPY:
      *accelerator = ui::Accelerator(ui::VKEY_C, ui::EF_PLATFORM_ACCELERATOR);
      return true;
    case IDC_PASTE:
      *accelerator = ui::Accelerator(ui::VKEY_V, ui::EF_PLATFORM_ACCELERATOR);
      return true;
  }
  return false;
#endif
}

bool IsCommandRepeatable(int command_id) {
  return base::Contains(kRepeatableCommandIds, command_id);
}
