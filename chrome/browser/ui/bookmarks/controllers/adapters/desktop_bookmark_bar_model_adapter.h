// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ADAPTERS_DESKTOP_BOOKMARK_BAR_MODEL_ADAPTER_H_
#define CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ADAPTERS_DESKTOP_BOOKMARK_BAR_MODEL_ADAPTER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/bookmarks/controllers/adapters/bookmark_bar_model_adapter.h"

class BookmarkMergedSurfaceService;

class DesktopBookmarkBarModelAdapter : public BookmarkBarModelAdapter {
 public:
  explicit DesktopBookmarkBarModelAdapter(
      BookmarkMergedSurfaceService* service);
  ~DesktopBookmarkBarModelAdapter() override;

  // BookmarkBarModelAdapter overrides:
  bool IsLoaded() const override;
  const bookmarks::BookmarkNode* GetNodeById(int64_t id) const override;
  std::vector<const bookmarks::BookmarkNode*> GetUnderlyingNodes(
      const bookmarks_api::BookmarkParentFolderId& folder) const override;
  void CanPasteFromClipboard(const BookmarkParentFolder* parent,
                             base::OnceCallback<void(bool)> callback) override;

 private:
  raw_ptr<BookmarkMergedSurfaceService> service_;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ADAPTERS_DESKTOP_BOOKMARK_BAR_MODEL_ADAPTER_H_
