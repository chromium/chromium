// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/controllers/desktop_bookmark_bar_injector.h"

#include "chrome/browser/bookmarks/bookmark_merged_surface_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/controllers/desktop_bookmark_bar_action_adapter.h"
#include "chrome/browser/ui/bookmarks/controllers/desktop_bookmark_bar_model_adapter.h"
#include "chrome/browser/ui/bookmarks/controllers/desktop_bookmark_bar_preferences_adapter.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/browser_apis/ui_controllers/bookmark_bar/bookmark_bar_ui_controller_impl.h"

DesktopBookmarkBarInjector::DesktopBookmarkBarInjector(
    BrowserWindowInterface* browser)
    : browser_(browser),
      preferences_adapter_(
          std::make_unique<DesktopBookmarkBarPreferencesAdapter>(
              browser_->GetProfile())),
      action_adapter_(
          std::make_unique<DesktopBookmarkBarActionAdapter>(browser_)),
      model_adapter_(std::make_unique<DesktopBookmarkBarModelAdapter>(
          BookmarkMergedSurfaceServiceFactory::GetForProfile(
              browser_->GetProfile()))) {}

DesktopBookmarkBarInjector::~DesktopBookmarkBarInjector() = default;

// static
std::unique_ptr<bookmark_bar_api::BookmarkBarUIController>
DesktopBookmarkBarInjector::CreateBookmarkBarUIController(
    BrowserWindowInterface* browser) {
  return std::make_unique<bookmark_bar_api::BookmarkBarUIControllerImpl>(
      std::make_unique<DesktopBookmarkBarInjector>(browser));
}

bookmark_bar_api::ActionAdapter& DesktopBookmarkBarInjector::action_adapter() {
  return *action_adapter_;
}

bookmark_bar_api::ModelAdapter& DesktopBookmarkBarInjector::model_adapter() {
  return *model_adapter_;
}

bookmark_bar_api::PreferencesAdapter&
DesktopBookmarkBarInjector::preferences_adapter() {
  return *preferences_adapter_;
}
