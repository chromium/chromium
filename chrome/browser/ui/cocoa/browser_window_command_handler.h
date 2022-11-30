// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_COMMAND_HANDLER_H_
#define CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_COMMAND_HANDLER_H_

#import <Foundation/Foundation.h>

#import "ui/base/cocoa/user_interface_item_command_handler.h"

// Implement UserInterfaceItemCommandHandler by validating items using global
// chrome:: functions and executing commands with chrome::ExecuteCommand().
@interface BrowserWindowCommandHandler
    : NSObject<UserInterfaceItemCommandHandler>
@end

#endif  // CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_COMMAND_HANDLER_H_
