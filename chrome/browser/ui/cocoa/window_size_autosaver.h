// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_WINDOW_SIZE_AUTOSAVER_H_
#define CHROME_BROWSER_UI_COCOA_WINDOW_SIZE_AUTOSAVER_H_

#import <AppKit/AppKit.h>

class PrefService;

// WindowSizeAutosaver is a helper class that makes it easy to let windows
// autoremember their position or position and size in a PrefService object.
// To use this, add a |WindowSizeAutosaver* __strong| to your window
// controller and initialize it in the window controller's init method, passing
// a window and an autosave name. The autosaver will register for "window moved"
// and "window resized" notifications and write the current window state to the
// prefs service every time they fire. The window's size is automatically
// restored when the autosaver's |initWithWindow:...| method is called.
//
// Note: Your xib file should have "Visible at launch" UNCHECKED, so that the
// initial repositioning is not visible.
@interface WindowSizeAutosaver : NSObject

- (instancetype)initWithWindow:(NSWindow*)window
                   prefService:(PrefService*)prefs
                          path:(const char*)path;
@end

#endif  // CHROME_BROWSER_UI_COCOA_WINDOW_SIZE_AUTOSAVER_H_
