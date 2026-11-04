// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/controllers/android/android_bookmark_bar_injector.h"

#include "chrome/browser/bookmarks/bookmark_merged_surface_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/browser_apis/ui_controllers/bookmark_bar/bookmark_bar_ui_controller_impl.h"

// static
std::unique_ptr<bookmark_bar_api::BookmarkBarUIController>
AndroidBookmarkBarInjector::CreateBookmarkBarUIController(
    BrowserWindowInterface* browser) {
  return std::make_unique<bookmark_bar_api::BookmarkBarUIControllerImpl>(
      std::make_unique<AndroidBookmarkBarInjector>(browser));
}

AndroidBookmarkBarInjector::AndroidBookmarkBarInjector(
    BrowserWindowInterface* browser)
    : action_adapter_(browser),
      preferences_adapter_(browser->GetProfile()),
      model_adapter_(BookmarkMergedSurfaceServiceFactory::GetForProfile(
          browser->GetProfile())) {}

AndroidBookmarkBarInjector::~AndroidBookmarkBarInjector() = default;

bookmark_bar_api::ActionAdapter& AndroidBookmarkBarInjector::action_adapter() {
  return action_adapter_;
}

bookmark_bar_api::PreferencesAdapter&
AndroidBookmarkBarInjector::preferences_adapter() {
  return preferences_adapter_;
}

bookmark_bar_api::ModelAdapter& AndroidBookmarkBarInjector::model_adapter() {
  return model_adapter_;
}
