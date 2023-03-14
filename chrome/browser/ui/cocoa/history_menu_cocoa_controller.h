// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_HISTORY_MENU_COCOA_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_HISTORY_MENU_COCOA_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/history_menu_bridge.h"

// Controller (MVC) for the history menu. All history menu item commands get
// directed here. This class only responds to menu events, but the actual
// creation and maintenance of the menu happens in the Bridge.
@interface HistoryMenuCocoaController
    : NSObject <NSMenuDelegate, NSMenuItemValidation>

- (instancetype)initWithBridge:(HistoryMenuBridge*)bridge;

// Called by any history menu item.
- (IBAction)openHistoryMenuItem:(id)sender;

@end  // HistoryMenuCocoaController

@interface HistoryMenuCocoaController (ExposedForUnitTests)
- (void)openURLForItem:(const HistoryMenuBridge::HistoryItem*)node;
@end  // HistoryMenuCocoaController (ExposedForUnitTests)

#endif  // CHROME_BROWSER_UI_COCOA_HISTORY_MENU_COCOA_CONTROLLER_H_
