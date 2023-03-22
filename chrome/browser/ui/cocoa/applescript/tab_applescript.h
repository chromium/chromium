// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_APPLESCRIPT_TAB_APPLESCRIPT_H_
#define CHROME_BROWSER_UI_COCOA_APPLESCRIPT_TAB_APPLESCRIPT_H_

#import <Foundation/Foundation.h>

#import "chrome/browser/ui/cocoa/applescript/element_applescript.h"

namespace content {
class WebContents;
}

// Represents a tab scriptable item in applescript.
@interface TabAppleScript : ElementAppleScript

// Doesn't actually create the tab here but just assigns the ID, tab is created
// when it calls insertInTabs: of a particular window, it is used in cases
// where user assigns a tab to a variable like |set var to make new tab|.
- (instancetype)init;

// Does not create a new tab but uses an existing one.
- (instancetype)initWithWebContents:(content::WebContents*)webContents;

// Assigns a tab, sets its unique ID and also copies temporary values.
- (void)setWebContents:(content::WebContents*)webContents;

// Returns/sets the URL currently visible to the user in the location bar.
@property(copy) NSString* URL;

// The title of the tab.
@property(readonly) NSString* title;

// Is the tab loading any resource?
@property(readonly) NSNumber* loading;

// Standard user commands.
- (void)handlesUndoScriptCommand:(NSScriptCommand*)command;
- (void)handlesRedoScriptCommand:(NSScriptCommand*)command;

// Edit operations on the page.
- (void)handlesCutScriptCommand:(NSScriptCommand*)command;
- (void)handlesCopyScriptCommand:(NSScriptCommand*)command;
- (void)handlesPasteScriptCommand:(NSScriptCommand*)command;

// Selects all contents on the page.
- (void)handlesSelectAllScriptCommand:(NSScriptCommand*)command;

// Navigation operations.
- (void)handlesGoBackScriptCommand:(NSScriptCommand*)command;
- (void)handlesGoForwardScriptCommand:(NSScriptCommand*)command;
- (void)handlesReloadScriptCommand:(NSScriptCommand*)command;
- (void)handlesStopScriptCommand:(NSScriptCommand*)command;

// Used to print a tab.
- (void)handlesPrintScriptCommand:(NSScriptCommand*)command;

// Used to save a tab, if no file is specified, prompts the user to enter it.
- (void)handlesSaveScriptCommand:(NSScriptCommand*)command;

// Used to close a tab.
- (void)handlesCloseScriptCommand:(NSScriptCommand*)command;

// Displays the HTML of the tab in a new tab.
- (void)handlesViewSourceScriptCommand:(NSScriptCommand*)command;

// Executes a piece of javascript in the tab.
- (id)handlesExecuteJavascriptScriptCommand:(NSScriptCommand*)command;

@end

#endif  // CHROME_BROWSER_UI_COCOA_APPLESCRIPT_TAB_APPLESCRIPT_H_
