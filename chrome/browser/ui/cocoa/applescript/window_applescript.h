// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_APPLESCRIPT_WINDOW_APPLESCRIPT_H_
#define CHROME_BROWSER_UI_COCOA_APPLESCRIPT_WINDOW_APPLESCRIPT_H_

#import <Foundation/Foundation.h>

#import "chrome/browser/ui/cocoa/applescript/element_applescript.h"

class Browser;
class Profile;
@class TabAppleScript;

// Represents a window class.
@interface WindowAppleScript : ElementAppleScript

// Creates a new window, returns nil if there is an error.
- (instancetype)init;

// Creates a new window with a particular profile.
- (instancetype)initWithProfile:(Profile*)aProfile;

// Does not create a new window but uses an existing one.
- (instancetype)initWithBrowser:(Browser*)aBrowser;

// Sets and gets the index of the currently selected tab. 1-based because
// this is intended for use by AppleScript.
@property(copy) NSNumber* activeTabIndex;

// Sets and get the given name of a window.
@property(copy) NSString* givenName;

// Mode refers to whether a window is a normal window or an incognito window
// it can be set only once while creating the window.
@property(copy) NSString* mode;

// Returns the currently selected tab.
@property(readonly) TabAppleScript* activeTab;

// Tab manipulation functions.
// The tabs inside the window.
// Returns |TabAppleScript*| of all the tabs contained
// within this particular folder.
@property(readonly) NSArray<TabAppleScript*>* tabs;

// Insert a tab at the end.
- (void)insertInTabs:(TabAppleScript*)aTab;

// Insert a tab at some position in the list.
// Called by AppleScript which takes care of bounds checking, make sure of it
// before calling directly.
- (void)insertInTabs:(TabAppleScript*)aTab atIndex:(int)index;

// Remove a window from the list.
// Called by AppleScript which takes care of bounds checking, make sure of it
// before calling directly.
- (void)removeFromTabsAtIndex:(int)index;

// The index of the window, windows are ordered front to back.
@property(copy) NSNumber* orderedIndex;

// For standard window functions like zoomable, bounds etc, we dont handle it
// but instead pass it onto the NSWindow associated with the window.
- (id)valueForUndefinedKey:(NSString*)key;
- (void)setValue:(id)value forUndefinedKey:(NSString*)key;

// Used to close window.
- (void)handlesCloseScriptCommand:(NSCloseCommand*)command;

@end

#endif  // CHROME_BROWSER_UI_COCOA_APPLESCRIPT_WINDOW_APPLESCRIPT_H_
