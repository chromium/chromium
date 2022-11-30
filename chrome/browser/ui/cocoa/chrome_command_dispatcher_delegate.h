// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_CHROME_COMMAND_DISPATCHER_DELEGATE_H_
#define CHROME_BROWSER_UI_COCOA_CHROME_COMMAND_DISPATCHER_DELEGATE_H_

#import <Cocoa/Cocoa.h>

#import "ui/base/cocoa/command_dispatcher.h"

// Implement CommandDispatcherDelegate by intercepting browser window keyboard
// shortcuts and executing them with chrome::ExecuteCommand.
@interface ChromeCommandDispatcherDelegate : NSObject<CommandDispatcherDelegate>

@end

#endif  // CHROME_BROWSER_UI_COCOA_CHROME_COMMAND_DISPATCHER_DELEGATE_H_
