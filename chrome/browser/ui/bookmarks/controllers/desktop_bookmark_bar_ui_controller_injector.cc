// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/controllers/desktop_bookmark_bar_ui_controller_injector.h"

#include "chrome/browser/ui/bookmarks/controllers/adapters/desktop_bookmark_bar_prefs_adapter.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

DesktopBookmarkBarUIControllerInjector::DesktopBookmarkBarUIControllerInjector(
    BrowserWindowInterface* browser)
    : browser_(browser) {}

DesktopBookmarkBarUIControllerInjector::
    ~DesktopBookmarkBarUIControllerInjector() = default;

BookmarkBarPrefsAdapter*
DesktopBookmarkBarUIControllerInjector::GetPrefsAdapter() {
  if (!prefs_adapter_) {
    prefs_adapter_ = std::make_unique<DesktopBookmarkBarPrefsAdapter>(
        browser_->GetProfile());
  }
  return prefs_adapter_.get();
}
