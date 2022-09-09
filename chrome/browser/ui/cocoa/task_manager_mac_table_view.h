// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TASK_MANAGER_MAC_TABLE_VIEW_H_
#define CHROME_BROWSER_UI_COCOA_TASK_MANAGER_MAC_TABLE_VIEW_H_

#import <Cocoa/Cocoa.h>

@class TaskManagerMacTableView;

@interface TaskManagerMacTableView : NSTableView
- (void)keyDown:(NSEvent*)event;
@end

#endif  // CHROME_BROWSER_UI_COCOA_TASK_MANAGER_MAC_TABLE_VIEW_H_
