// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_APPS_APP_SHIM_MENU_CONTROLLER_MAC_H_
#define CHROME_BROWSER_UI_COCOA_APPS_APP_SHIM_MENU_CONTROLLER_MAC_H_

#import <Cocoa/Cocoa.h>

#include <string>

@class DoppelgangerMenuItem;

// This controller listens to NSWindowDidBecomeMainNotification and
// NSWindowDidResignMainNotification and modifies the main menu bar to mimic a
// main menu for the app. When an app window becomes main, all Chrome menu items
// are hidden and menu items for the app are appended to the main menu. When the
// app window resigns main, its menu items are removed and all Chrome menu items
// are unhidden.
@interface AppShimMenuController : NSObject {
 @private
  // The extension id of the currently focused packaged app.
  std::string _appId;

  // Items that need a doppelganger.
  DoppelgangerMenuItem* __strong _aboutDoppelganger;
  DoppelgangerMenuItem* __strong _hideDoppelganger;
  DoppelgangerMenuItem* __strong _quitDoppelganger;
  DoppelgangerMenuItem* __strong _newDoppelganger;
  DoppelgangerMenuItem* __strong _openDoppelganger;
  DoppelgangerMenuItem* __strong _closeWindowDoppelganger;
  DoppelgangerMenuItem* __strong _allToFrontDoppelganger;

  // Menu items for the currently focused packaged app.
  NSMenuItem* __strong _appMenuItem;
  NSMenuItem* __strong _fileMenuItem;
  NSMenuItem* __strong _editMenuItem;
  NSMenuItem* __strong _windowMenuItem;

  // Additional menu items for hosted apps.
  NSMenuItem* __strong _viewMenuItem;
  NSMenuItem* __strong _historyMenuItem;
}

@end

#endif  // CHROME_BROWSER_UI_COCOA_APPS_APP_SHIM_MENU_CONTROLLER_MAC_H_
