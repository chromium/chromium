// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ANDROID_ANDROID_BOOKMARK_BAR_INJECTOR_H_
#define CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ANDROID_ANDROID_BOOKMARK_BAR_INJECTOR_H_

#include <memory>

#include "chrome/browser/ui/bookmarks/controllers/android/android_bookmark_bar_action_adapter.h"
#include "chrome/browser/ui/bookmarks/controllers/android/android_bookmark_bar_model_adapter.h"
#include "chrome/browser/ui/bookmarks/controllers/bookmark_bar_preferences_adapter.h"
#include "components/browser_apis/ui_controllers/bookmark_bar/adapters/platform_adapters_provider.h"

class BrowserWindowInterface;

namespace bookmark_bar_api {
class BookmarkBarUIController;
}  // namespace bookmark_bar_api

// Owns the Android adapters for a single bookmark bar and provides them to
// `BookmarkBarUIControllerImpl`. `browser` must outlive the controller.
class AndroidBookmarkBarInjector
    : public bookmark_bar_api::PlatformAdaptersProvider {
 public:
  static std::unique_ptr<bookmark_bar_api::BookmarkBarUIController>
  CreateBookmarkBarUIController(BrowserWindowInterface* browser);

  explicit AndroidBookmarkBarInjector(BrowserWindowInterface* browser);
  AndroidBookmarkBarInjector(const AndroidBookmarkBarInjector&) = delete;
  AndroidBookmarkBarInjector& operator=(const AndroidBookmarkBarInjector&) =
      delete;
  ~AndroidBookmarkBarInjector() override;

  // bookmark_bar_api::PlatformAdaptersProvider:
  bookmark_bar_api::ActionAdapter& action_adapter() override;
  bookmark_bar_api::PreferencesAdapter& preferences_adapter() override;
  bookmark_bar_api::ModelAdapter& model_adapter() override;

 private:
  AndroidBookmarkBarActionAdapter action_adapter_;
  BookmarkBarPreferencesAdapter preferences_adapter_;
  AndroidBookmarkBarModelAdapter model_adapter_;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ANDROID_ANDROID_BOOKMARK_BAR_INJECTOR_H_
