// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ANDROID_ANDROID_BOOKMARK_BAR_MODEL_ADAPTER_H_
#define CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ANDROID_ANDROID_BOOKMARK_BAR_MODEL_ADAPTER_H_

#include <stdint.h>

#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/browser_apis/ui_controllers/bookmark_bar/adapters/model_adapter.h"

namespace bookmarks {
class BookmarkModel;
class ManagedBookmarkService;
}  // namespace bookmarks

// Android's `ModelAdapter`, backed directly by `BookmarkModel`.
class AndroidBookmarkBarModelAdapter : public bookmark_bar_api::ModelAdapter {
 public:
  // `model` and `managed_service` must outlive `this`.
  AndroidBookmarkBarModelAdapter(
      bookmarks::BookmarkModel* model,
      bookmarks::ManagedBookmarkService* managed_service);
  AndroidBookmarkBarModelAdapter(const AndroidBookmarkBarModelAdapter&) =
      delete;
  AndroidBookmarkBarModelAdapter& operator=(
      const AndroidBookmarkBarModelAdapter&) = delete;
  ~AndroidBookmarkBarModelAdapter() override;

  // bookmark_bar_api::ModelAdapter:
  const bookmarks::BookmarkNode* GetNodeById(int64_t id) const override;
  std::vector<const bookmarks::BookmarkNode*> GetUnderlyingNodes(
      bookmarks_api::mojom::PermanentFolderType type) const override;

 private:
  const raw_ptr<bookmarks::BookmarkModel> model_;
  const raw_ptr<bookmarks::ManagedBookmarkService> managed_service_;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ANDROID_ANDROID_BOOKMARK_BAR_MODEL_ADAPTER_H_
