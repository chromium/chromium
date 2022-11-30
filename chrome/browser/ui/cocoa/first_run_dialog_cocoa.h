// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_FIRST_RUN_DIALOG_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_FIRST_RUN_DIALOG_COCOA_H_

#import <Cocoa/Cocoa.h>

// Class that acts as a controller for the modal first run dialog.
// The dialog asks the user's explicit permission for reporting stats to help
// us improve Chromium.
@interface FirstRunDialogController : NSWindowController

- (BOOL)isStatsReportingEnabled;
- (BOOL)isMakeDefaultBrowserEnabled;

@end

#endif  // CHROME_BROWSER_UI_COCOA_FIRST_RUN_DIALOG_COCOA_H_
