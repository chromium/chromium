// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/controllers/desktop_bookmark_bar_ui_controller_injector.h"

#include "chrome/browser/bookmarks/bookmark_merged_surface_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/controllers/adapters/desktop_bookmark_bar_action_adapter.h"
#include "chrome/browser/ui/bookmarks/controllers/adapters/desktop_bookmark_bar_model_adapter.h"
#include "chrome/browser/ui/bookmarks/controllers/adapters/desktop_bookmark_bar_prefs_adapter.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

DesktopBookmarkBarUIControllerInjector::DesktopBookmarkBarUIControllerInjector(
    BrowserWindowInterface* browser)
    : browser_(browser),
      prefs_adapter_(std::make_unique<DesktopBookmarkBarPrefsAdapter>(
          browser_->GetProfile())),
      action_adapter_(
          std::make_unique<DesktopBookmarkBarActionAdapter>(browser_)),
      model_adapter_(std::make_unique<DesktopBookmarkBarModelAdapter>(
          BookmarkMergedSurfaceServiceFactory::GetForProfile(
              browser_->GetProfile()))) {}

DesktopBookmarkBarUIControllerInjector::
    ~DesktopBookmarkBarUIControllerInjector() = default;

BookmarkBarPrefsAdapter*
DesktopBookmarkBarUIControllerInjector::GetPrefsAdapter() {
  return prefs_adapter_.get();
}

BookmarkBarActionAdapter*
DesktopBookmarkBarUIControllerInjector::GetActionAdapter() {
  return action_adapter_.get();
}

BookmarkBarModelAdapter*
DesktopBookmarkBarUIControllerInjector::GetModelAdapter() {
  return model_adapter_.get();
}
