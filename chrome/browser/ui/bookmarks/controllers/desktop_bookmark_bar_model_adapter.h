// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_DESKTOP_BOOKMARK_BAR_MODEL_ADAPTER_H_
#define CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_DESKTOP_BOOKMARK_BAR_MODEL_ADAPTER_H_

#include "base/memory/raw_ptr.h"
#include "components/browser_apis/ui_controllers/bookmark_bar/adapters/model_adapter.h"

class BookmarkMergedSurfaceService;

class DesktopBookmarkBarModelAdapter : public bookmark_bar_api::ModelAdapter {
 public:
  explicit DesktopBookmarkBarModelAdapter(
      BookmarkMergedSurfaceService* service);
  ~DesktopBookmarkBarModelAdapter() override;

  // bookmark_bar_api::ModelAdapter overrides:
  const bookmarks::BookmarkNode* GetNodeById(int64_t id) const override;
  std::vector<const bookmarks::BookmarkNode*> GetUnderlyingNodes(
      bookmarks_api::mojom::PermanentFolderType type) const override;
  void CanPasteFromClipboard(
      const std::vector<const bookmarks::BookmarkNode*>& selection,
      base::OnceCallback<void(bool)> callback) override;

 private:
  // Null only in tests without a bookmark service. Profiles that can open
  // browser windows always have one.
  raw_ptr<BookmarkMergedSurfaceService> service_;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_DESKTOP_BOOKMARK_BAR_MODEL_ADAPTER_H_
