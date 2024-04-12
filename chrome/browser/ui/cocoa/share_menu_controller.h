// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_SHARE_MENU_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_SHARE_MENU_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

// Set this as the delegate of a menu to populate with potential sharing service
// items. Handles performing share actions chosen by the user and opening the
// sharing service pref pane so that the user can enable or disable services.
@interface ShareMenuController
    : NSObject <NSMenuDelegate, NSMenuItemValidation, NSSharingServiceDelegate>
@end

#endif  // CHROME_BROWSER_UI_COCOA_SHARE_MENU_CONTROLLER_H_
