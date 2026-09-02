// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_DESKTOP_BOOKMARK_BAR_UI_CONTROLLER_INJECTOR_H_
#define CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_DESKTOP_BOOKMARK_BAR_UI_CONTROLLER_INJECTOR_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/bookmarks/controllers/bookmark_bar_ui_controller_injector.h"

class BrowserWindowInterface;
class DesktopBookmarkBarActionAdapter;
class DesktopBookmarkBarModelAdapter;
class DesktopBookmarkBarPrefsAdapter;

class DesktopBookmarkBarUIControllerInjector
    : public BookmarkBarUIControllerInjector {
 public:
  explicit DesktopBookmarkBarUIControllerInjector(
      BrowserWindowInterface* browser);
  DesktopBookmarkBarUIControllerInjector(
      const DesktopBookmarkBarUIControllerInjector&) = delete;
  DesktopBookmarkBarUIControllerInjector& operator=(
      const DesktopBookmarkBarUIControllerInjector&) = delete;
  ~DesktopBookmarkBarUIControllerInjector() override;

  // BookmarkBarUIControllerInjector overrides:
  BookmarkBarPrefsAdapter* GetPrefsAdapter() override;
  BookmarkBarActionAdapter* GetActionAdapter() override;
  BookmarkBarModelAdapter* GetModelAdapter() override;

 private:
  raw_ptr<BrowserWindowInterface> browser_;
  std::unique_ptr<DesktopBookmarkBarPrefsAdapter> prefs_adapter_;
  std::unique_ptr<DesktopBookmarkBarActionAdapter> action_adapter_;
  std::unique_ptr<DesktopBookmarkBarModelAdapter> model_adapter_;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_DESKTOP_BOOKMARK_BAR_UI_CONTROLLER_INJECTOR_H_
