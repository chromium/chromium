// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/i18n/base_i18n_switches.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/ui/cocoa/accelerators_cocoa.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest_mac.h"
#include "ui/base/accelerators/platform_accelerator_cocoa.h"
#include "ui/base/l10n/l10n_util_mac.h"
#import "ui/events/keycodes/keyboard_code_conversion_mac.h"

using AcceleratorsCocoaBrowserTest = InProcessBrowserTest;

namespace {

// Adds all NSMenuItems with an accelerator to the array.
void AddAcceleratorItemsToArray(NSMenu* menu, NSMutableArray* array) {
  for (NSMenuItem* item in menu.itemArray) {
    NSMenu* submenu = item.submenu;
    if (submenu)
      AddAcceleratorItemsToArray(submenu, array);

    // If the tag or key equivalent is zero, then either this is a macOS menu
    // item that we don't care about, or it's a chrome accelerator with non
    // standard selector. We don't have an easy way to distinguish between
    // these, so we just ignore them. Also as of macOS Monterey the AppKit
    // adds a tag to the Start Dictation... menu item - skip it as well.
    if (item.tag == 0 || item.keyEquivalent.length == 0 ||
        item.action == @selector(startDictation:))
      continue;

    [array addObject:item];
  }
}

// Checks that the |item|'s modifier mask matches |modifierMask|. The
// NSEventModifierFlagShift may be stored as part of the key equivalent
// (i.e. "a" + NSEventModifierFlagShift => "A").
inline bool MenuItemHasModifierMask(NSMenuItem* item, NSUInteger modifierMask) {
  return modifierMask == item.keyEquivalentModifierMask ||
         (modifierMask & ~NSEventModifierFlagShift) ==
             item.keyEquivalentModifierMask;
}

// Returns the NSMenuItem that has the given keyEquivalent and modifiers, or
// nil.
NSMenuItem* MenuContainsAccelerator(NSMenu* menu,
                                    NSString* key_equivalent,
                                    NSUInteger modifier_mask) {
  for (NSMenuItem* item in menu.itemArray) {
    NSMenu* submenu = item.submenu;
    if (submenu) {
      NSMenuItem* result =
          MenuContainsAccelerator(submenu, key_equivalent, modifier_mask);
      if (result)
        return result;
    }

    if ([item.keyEquivalent isEqual:key_equivalent]) {
      // We don't want to ignore shift for [cmd + shift + tab] and [cmd + tab],
      // which are special.
      if (item.tag == IDC_SELECT_NEXT_TAB ||
          item.tag == IDC_SELECT_PREVIOUS_TAB) {
        if (modifier_mask == item.keyEquivalentModifierMask)
          return item;
        continue;
      }

      if (MenuItemHasModifierMask(item, modifier_mask))
        return item;
    }
  }
  return nil;
}

}  // namespace

class AcceleratorsCocoaBrowserTestRTL : public AcceleratorsCocoaBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(::switches::kForceUIDirection,
                                    ::switches::kForceDirectionRTL);
    command_line->AppendSwitchASCII(::switches::kForceTextDirection,
                                    ::switches::kForceDirectionRTL);
  }
};

// Checks that each NSMenuItem in the main menu has a corresponding accelerator,
// and the keyEquivalent/modifiers match.
IN_PROC_BROWSER_TEST_F(AcceleratorsCocoaBrowserTest,
                       MainMenuAcceleratorsInMapping) {
  NSMenu* menu = [NSApp mainMenu];
  NSMutableArray* array = [NSMutableArray array];
  AddAcceleratorItemsToArray(menu, array);
  AcceleratorsCocoa* keymap = AcceleratorsCocoa::GetInstance();

  for (NSMenuItem* item in array) {
    const ui::Accelerator* accelerator =
        keymap->GetAcceleratorForCommand(item.tag);
    EXPECT_TRUE(accelerator);
    if (!accelerator) {
      continue;
    }

    // Get the Cocoa key_equivalent associated with the accelerator.
    KeyEquivalentAndModifierMask* equivalent =
        GetKeyEquivalentAndModifierMaskFromAccelerator(*accelerator);

    // Check that the menu item's keyEquivalent matches the one from the
    // Cocoa accelerator map.
    EXPECT_NSEQ(equivalent.keyEquivalent, item.keyEquivalent);

    // Check that the menu item's modifier mask matches the one stored in the
    // accelerator. Ignore the NSEventModifierFlagShift because it's part of
    // the key equivalent (i.e. "a" + NSEventModifierFlagShift = "A").
    EXPECT_TRUE(MenuItemHasModifierMask(item, equivalent.modifierMask));
  }
}

// Check that each accelerator with a command_id has an associated NSMenuItem
// in the main menu. If the selector is commandDispatch:, then the tag must
// match the command_id.
IN_PROC_BROWSER_TEST_F(AcceleratorsCocoaBrowserTest,
                       MappingAcceleratorsInMainMenu) {
  AcceleratorsCocoa* keymap = AcceleratorsCocoa::GetInstance();
  // The "Share" menu is dynamically populated.
  NSMenu* mainMenu = NSApp.mainMenu;
  NSMenu* fileMenu = [[mainMenu itemWithTag:IDC_FILE_MENU] submenu];
  NSMenu* shareMenu =
      [[fileMenu itemWithTitle:l10n_util::GetNSString(IDS_SHARE_MAC)] submenu];
  [[shareMenu delegate] menuNeedsUpdate:shareMenu];

  for (auto& it : keymap->accelerators_) {
    KeyEquivalentAndModifierMask* equivalent =
        GetKeyEquivalentAndModifierMaskFromAccelerator(it.second);

    // Check that there exists a corresponding NSMenuItem.
    NSMenuItem* item = MenuContainsAccelerator(
        [NSApp mainMenu], equivalent.keyEquivalent, equivalent.modifierMask);
    EXPECT_TRUE(item);

    // If the menu uses a commandDispatch:, the tag must match the command id!
    // Added an exception for IDC_TOGGLE_FULLSCREEN_TOOLBAR, which conflicts
    // with IDC_PRESENTATION_MODE.
    if (item.action == @selector(commandDispatch:) &&
        item.tag != IDC_TOGGLE_FULLSCREEN_TOOLBAR) {
      EXPECT_EQ(item.tag, it.first);
    }
  }
}

IN_PROC_BROWSER_TEST_F(AcceleratorsCocoaBrowserTestRTL,
                       HistoryAcceleratorsReversedForRTL) {
  AcceleratorsCocoa* keymap = AcceleratorsCocoa::GetInstance();
  ui::Accelerator history_forward = keymap->accelerators_[IDC_FORWARD];
  ui::Accelerator history_back = keymap->accelerators_[IDC_BACK];

  // In LTR, History -> Forward is VKEY_OEM_6 and Back is VKEY_OEM_4.
  EXPECT_EQ(ui::VKEY_OEM_4, history_forward.key_code());
  EXPECT_EQ(ui::VKEY_OEM_6, history_back.key_code());
}
