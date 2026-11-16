// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_APPLESCRIPT_WINDOW_APPLESCRIPT_H_
#define CHROME_BROWSER_UI_COCOA_APPLESCRIPT_WINDOW_APPLESCRIPT_H_

#import <Foundation/Foundation.h>

#import "chrome/browser/ui/cocoa/applescript/element_applescript.h"

class BrowserWindowInterface;
class Profile;
@class TabAppleScript;

// Represents a window class.
@interface WindowAppleScript : ElementAppleScript

// Creates a new window, returns nil if there is an error.
- (instancetype)init;

// Creates a new window with a particular profile.
- (instancetype)initWithProfile:(Profile*)aProfile;

// Does not create a new window but uses an existing one. The designated
// initializer.
- (instancetype)initWithBrowser:(BrowserWindowInterface*)aBrowser;

// Sets and gets the index of the currently selected tab. 1-based because
// this is intended for use by AppleScript.
@property(copy) NSNumber* activeTabIndex;

// Sets and gets the given name of a window.
@property(copy) NSString* givenName;

// Mode refers to whether a window is a normal window or an incognito window
// it can be set only once while creating the window.
@property(copy) NSString* mode;

// Sets and gets the currently selected tab.
@property(strong) TabAppleScript* activeTab;

// ----- Tabs -----

// The tabs inside the window. Returns `TabAppleScript*` of all the tabs
// contained within this particular window.
@property(readonly) NSArray<TabAppleScript*>* tabs;

// Insert a tab at the end.
- (void)insertInTabs:(TabAppleScript*)aTab;

// Insert a tab at some position in the list.
//
// Called by AppleScript which takes care of bounds checking; caller must do so
// otherwise.
- (void)insertInTabs:(TabAppleScript*)aTab atIndex:(int)index;

// Remove a tab from the list.
//
// Called by AppleScript which takes care of bounds checking; caller must do so
// otherwise.
- (void)removeFromTabsAtIndex:(int)index;

// The index of the window, windows are ordered front to back.
@property(copy) NSNumber* orderedIndex;

// Methods to catch all other standard window properties to be forwarded to the
// underlying window.
- (id)valueForUndefinedKey:(NSString*)key;
- (void)setValue:(id)value forUndefinedKey:(NSString*)key;

// Used to close window.
- (void)handlesCloseScriptCommand:(NSCloseCommand*)command;

@end

namespace AppleScript {

// Returns an array of `WindowAppleScript*` for all open non-closing windows,
// ordered front to back.
NSArray<WindowAppleScript*>* GetWindows();

// Orders `window` at 0-based `index` within `GetWindows()`.
void InsertWindowAtIndex(WindowAppleScript* window, int index);

}  // namespace AppleScript

#endif  // CHROME_BROWSER_UI_COCOA_APPLESCRIPT_WINDOW_APPLESCRIPT_H_
