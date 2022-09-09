// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/accelerator_utils.h"

#import <Cocoa/Cocoa.h>

#include "chrome/browser/global_keyboard_shortcuts_mac.h"
#include "chrome/browser/ui/cocoa/accelerators_cocoa.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/base/accelerators/accelerator.h"
#import "ui/base/accelerators/platform_accelerator_cocoa.h"
#import "ui/events/keycodes/keyboard_code_conversion_mac.h"

namespace chrome {

bool IsChromeAccelerator(const ui::Accelerator& accelerator) {
  NSUInteger modifiers =
      (accelerator.IsCtrlDown() ? NSEventModifierFlagControl : 0) |
      (accelerator.IsCmdDown() ? NSEventModifierFlagCommand : 0) |
      (accelerator.IsAltDown() ? NSEventModifierFlagOption : 0) |
      (accelerator.IsShiftDown() ? NSEventModifierFlagShift : 0);

  // The |accelerator| passed in contains a Windows key code but no platform
  // accelerator info. The Accelerator list is the opposite: It has accelerators
  // that have key_code() == VKEY_UNKNOWN but they contain a platform
  // accelerator. We find common ground by converting the passed in Windows key
  // code to a character and use that when comparing against the Accelerator
  // list.
  unichar shifted_character;
  unichar character;
  int mac_keycode = ui::MacKeyCodeForWindowsKeyCode(
      accelerator.key_code(), modifiers, &shifted_character, &character);
  if (mac_keycode == -1)
    return false;

  NSString* characters = [NSString stringWithFormat:@"%C", character];
  NSString* charactersIgnoringModifiers =
      [NSString stringWithFormat:@"%C", shifted_character];

  NSEvent* event = [NSEvent keyEventWithType:NSEventTypeKeyDown
                                    location:NSZeroPoint
                               modifierFlags:modifiers
                                   timestamp:0
                                windowNumber:0
                                     context:nil
                                  characters:characters
                 charactersIgnoringModifiers:charactersIgnoringModifiers
                                   isARepeat:NO
                                     keyCode:mac_keycode];

  return CommandForKeyEvent(event).found();
}

ui::AcceleratorProvider* AcceleratorProviderForBrowser(Browser* browser) {
  return BrowserView::GetBrowserViewForBrowser(browser);
}

}  // namespace chrome
