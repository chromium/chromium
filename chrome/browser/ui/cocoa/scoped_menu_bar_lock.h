// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_SCOPED_MENU_BAR_LOCK_H_
#define CHROME_BROWSER_UI_COCOA_SCOPED_MENU_BAR_LOCK_H_

// ScopedMenuBarLock uses an AppKit API to lock the menu bar in its current
// state (visible/hidden). Useful to temporarily keep the menu bar visible
// after appearing when the user moved the mouse to the top of the screen, or
// to temporarily prevent the menu bar from showing automatically.
// ScopedMenuBarLock helps enforce that lock/unlock calls are balanced.
class ScopedMenuBarLock {
 public:
  ScopedMenuBarLock();

  ScopedMenuBarLock(const ScopedMenuBarLock&) = delete;
  ScopedMenuBarLock& operator=(const ScopedMenuBarLock&) = delete;

  ~ScopedMenuBarLock();
};

#endif  // CHROME_BROWSER_UI_COCOA_SCOPED_MENU_BAR_LOCK_H_
