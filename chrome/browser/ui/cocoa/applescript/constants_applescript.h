// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_APPLESCRIPT_CONSTANTS_APPLESCRIPT_H_
#define CHROME_BROWSER_UI_COCOA_APPLESCRIPT_CONSTANTS_APPLESCRIPT_H_

#import <Cocoa/Cocoa.h>

// This file contains the constant that are use to set the property of an
// applescript scriptable item.
namespace AppleScript {
// Property to access windows.
extern NSString* const kWindowsProperty;

// Property to access tabs.
extern NSString* const kTabsProperty;

// Property to access bookmarks folders.
extern NSString* const kBookmarkFoldersProperty;

// Property to access bookmark items.
extern NSString* const kBookmarkItemsProperty;

// To indicate a window in normal mode.
extern NSString* const kNormalWindowMode;

// To indicate a window in incognito mode.
extern NSString* const kIncognitoWindowMode;
}
#endif  // CHROME_BROWSER_UI_COCOA_APPLESCRIPT_CONSTANTS_APPLESCRIPT_H_
