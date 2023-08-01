// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/scoped_menu_bar_lock.h"

#import <AppKit/AppKit.h>

@interface NSMenu (PrivateAPI)
- (void)_lockMenuPosition;
- (void)_unlockMenuPosition;
@end

ScopedMenuBarLock::ScopedMenuBarLock() {
  if ([NSMenu instancesRespondToSelector:@selector(_lockMenuPosition)])
    [NSApp.mainMenu _lockMenuPosition];
}

ScopedMenuBarLock::~ScopedMenuBarLock() {
  if ([NSMenu instancesRespondToSelector:@selector(_unlockMenuPosition)])
    [NSApp.mainMenu _unlockMenuPosition];
}
