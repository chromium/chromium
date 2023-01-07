// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_MAIN_MENU_ITEM_H_
#define CHROME_BROWSER_UI_COCOA_MAIN_MENU_ITEM_H_

// This interface is implemented by top-level main menubar items that need to
// be dynamically updated based on the profile. The C++ bridge should implement
// this interface so that the AppController can appropriately manage the
// bridge's lifetime and profile information.
class MainMenuItem {
 public:
  // Resets the menu to its initial state. This is called before the Item is
  // destructed and recreated.
  virtual void ResetMenu() = 0;

  // Forces a rebuild of the menu as if the model had changed.
  virtual void BuildMenu() = 0;

 protected:
  virtual ~MainMenuItem() {}
};

#endif  // CHROME_BROWSER_UI_COCOA_MAIN_MENU_ITEM_H_
