// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_DESKTOP_BOOKMARK_BAR_INJECTOR_H_
#define CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_DESKTOP_BOOKMARK_BAR_INJECTOR_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/browser_apis/ui_controllers/bookmark_bar/adapters/platform_adapters_provider.h"

class BrowserWindowInterface;
class DesktopBookmarkBarActionAdapter;
class DesktopBookmarkBarModelAdapter;
class DesktopBookmarkBarPreferencesAdapter;

namespace bookmark_bar_api {
class BookmarkBarUIController;
}  // namespace bookmark_bar_api

// Provides the desktop adapters for the bookmark bar UI controller of
// `browser`.
class DesktopBookmarkBarInjector
    : public bookmark_bar_api::PlatformAdaptersProvider {
 public:
  explicit DesktopBookmarkBarInjector(BrowserWindowInterface* browser);
  DesktopBookmarkBarInjector(const DesktopBookmarkBarInjector&) = delete;
  DesktopBookmarkBarInjector& operator=(const DesktopBookmarkBarInjector&) =
      delete;
  ~DesktopBookmarkBarInjector() override;

  // Creates a bookmark bar UI controller for `browser` using the desktop
  // adapters. The caller is the bookmark bar UI, which owns the controller.
  static std::unique_ptr<bookmark_bar_api::BookmarkBarUIController>
  CreateBookmarkBarUIController(BrowserWindowInterface* browser);

  // bookmark_bar_api::PlatformAdaptersProvider overrides:
  bookmark_bar_api::ActionAdapter& action_adapter() override;
  bookmark_bar_api::ModelAdapter& model_adapter() override;
  bookmark_bar_api::PreferencesAdapter& preferences_adapter() override;

 private:
  raw_ptr<BrowserWindowInterface> browser_;
  std::unique_ptr<DesktopBookmarkBarPreferencesAdapter> preferences_adapter_;
  std::unique_ptr<DesktopBookmarkBarActionAdapter> action_adapter_;
  std::unique_ptr<DesktopBookmarkBarModelAdapter> model_adapter_;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_DESKTOP_BOOKMARK_BAR_INJECTOR_H_
