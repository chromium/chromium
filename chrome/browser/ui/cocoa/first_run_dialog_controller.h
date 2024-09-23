// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_FIRST_RUN_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_FIRST_RUN_DIALOG_CONTROLLER_H_

#include <Cocoa/Cocoa.h>

// FirstRunDialogViewController is the NSViewController for the first run
// dialog's content view.
@interface FirstRunDialogViewController : NSViewController

- (NSString*)windowTitle;

- (BOOL)isStatsReportingEnabled;
- (BOOL)isMakeDefaultBrowserEnabled;

@end

#endif  // CHROME_BROWSER_UI_COCOA_FIRST_RUN_DIALOG_CONTROLLER_H_
