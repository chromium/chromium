// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ui_controller_factory.h"

#include "chrome/browser/ui/bookmarks/controllers/bookmark_bar_ui_controller_impl.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

DEFINE_USER_DATA(UIControllerFactory);

// static
UIControllerFactory* UIControllerFactory::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

UIControllerFactory::UIControllerFactory(BrowserWindowInterface* browser)
    : browser_(browser),
      scoped_unowned_user_data_(browser->GetUnownedUserDataHost(), *this) {}

UIControllerFactory::~UIControllerFactory() = default;

std::unique_ptr<BookmarkBarUIController>
UIControllerFactory::CreateBookmarkBarController() {
  return std::make_unique<BookmarkBarUIControllerImpl>(browser_);
}
