// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_BOOKMARK_BAR_UI_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_BOOKMARK_BAR_UI_CONTROLLER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/bookmarks/controllers/bookmark_bar_ui_controller.h"

class BookmarkBarUIControllerInjector;

class BookmarkBarUIControllerImpl : public BookmarkBarUIController {
 public:
  explicit BookmarkBarUIControllerImpl(
      std::unique_ptr<BookmarkBarUIControllerInjector> injector);
  BookmarkBarUIControllerImpl(const BookmarkBarUIControllerImpl&) = delete;
  BookmarkBarUIControllerImpl& operator=(const BookmarkBarUIControllerImpl&) =
      delete;
  ~BookmarkBarUIControllerImpl() override;

  // BookmarkBarUIController overrides:
  void Bind(BookmarkBarUIClient* client) override;
  void OpenAppsPage(WindowOpenDisposition disposition) override;
  void OpenBookmark(int64_t node_id,
                    WindowOpenDisposition disposition) override;
  void OpenFolder(const bookmarks::BookmarkNodeId& folder,
                  WindowOpenDisposition disposition) override;

 private:
  void OnAppsPageShortcutVisibilityPrefChanged();
  void OnTabGroupsVisibilityPrefChanged();
  void OnShowManagedBookmarksPrefChanged();

  std::unique_ptr<BookmarkBarUIControllerInjector> injector_;
  raw_ptr<BookmarkBarUIClient> client_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_BOOKMARK_BAR_UI_CONTROLLER_IMPL_H_
