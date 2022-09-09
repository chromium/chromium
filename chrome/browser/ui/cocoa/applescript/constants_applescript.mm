// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/applescript/constants_applescript.h"

namespace AppleScript {
// Property to access windows.
NSString* const kWindowsProperty = @"appleScriptWindows";

// Property to access tabs.
NSString* const kTabsProperty = @"tabs";

// Property to access bookmarks folders.
NSString* const kBookmarkFoldersProperty = @"bookmarkFolders";

// Property to access bookmark items.
NSString* const kBookmarkItemsProperty = @"bookmarkItems";

// To indicate a window in normal mode.
NSString* const kNormalWindowMode = @"normal";

// To indicate a window in incognito mode.
NSString* const kIncognitoWindowMode = @"incognito";
}
