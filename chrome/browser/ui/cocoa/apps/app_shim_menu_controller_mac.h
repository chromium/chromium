// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_APPS_APP_SHIM_MENU_CONTROLLER_MAC_H_
#define CHROME_BROWSER_UI_COCOA_APPS_APP_SHIM_MENU_CONTROLLER_MAC_H_

#import <Cocoa/Cocoa.h>
#include <string>

#include "base/mac/scoped_nsobject.h"

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
  base::scoped_nsobject<DoppelgangerMenuItem> _aboutDoppelganger;
  base::scoped_nsobject<DoppelgangerMenuItem> _hideDoppelganger;
  base::scoped_nsobject<DoppelgangerMenuItem> _quitDoppelganger;
  base::scoped_nsobject<DoppelgangerMenuItem> _newDoppelganger;
  base::scoped_nsobject<DoppelgangerMenuItem> _openDoppelganger;
  base::scoped_nsobject<DoppelgangerMenuItem> _closeWindowDoppelganger;
  base::scoped_nsobject<DoppelgangerMenuItem> _allToFrontDoppelganger;
  // Menu items for the currently focused packaged app.
  base::scoped_nsobject<NSMenuItem> _appMenuItem;
  base::scoped_nsobject<NSMenuItem> _fileMenuItem;
  base::scoped_nsobject<NSMenuItem> _editMenuItem;
  base::scoped_nsobject<NSMenuItem> _windowMenuItem;
  // Additional menu items for hosted apps.
  base::scoped_nsobject<NSMenuItem> _viewMenuItem;
  base::scoped_nsobject<NSMenuItem> _historyMenuItem;
}

@end

#endif  // CHROME_BROWSER_UI_COCOA_APPS_APP_SHIM_MENU_CONTROLLER_MAC_H_
