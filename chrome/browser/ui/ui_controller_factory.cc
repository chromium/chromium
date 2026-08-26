// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ui_controller_factory.h"

#include "build/build_config.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/bookmarks/controllers/bookmark_bar_ui_controller_impl.h"
#include "chrome/browser/ui/bookmarks/controllers/desktop_bookmark_bar_ui_controller_injector.h"
#endif

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
#if !BUILDFLAG(IS_ANDROID)
  auto injector =
      std::make_unique<DesktopBookmarkBarUIControllerInjector>(browser_);
  return std::make_unique<BookmarkBarUIControllerImpl>(std::move(injector));
#else
  return nullptr;
#endif
}
