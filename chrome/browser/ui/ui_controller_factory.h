// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_UI_CONTROLLER_FACTORY_H_
#define CHROME_BROWSER_UI_UI_CONTROLLER_FACTORY_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BookmarkBarUIController;
class BrowserWindowInterface;

class UIControllerFactory {
 public:
  DECLARE_USER_DATA(UIControllerFactory);

  // Returns the factory for `browser`, or null if it does not have one.
  static UIControllerFactory* From(BrowserWindowInterface* browser);

  explicit UIControllerFactory(BrowserWindowInterface* browser);
  ~UIControllerFactory();

  std::unique_ptr<BookmarkBarUIController> CreateBookmarkBarController();

 private:
  raw_ptr<BrowserWindowInterface> browser_;
  ui::ScopedUnownedUserData<UIControllerFactory> scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_UI_CONTROLLER_FACTORY_H_
