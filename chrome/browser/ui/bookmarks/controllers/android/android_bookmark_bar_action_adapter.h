// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ANDROID_ANDROID_BOOKMARK_BAR_ACTION_ADAPTER_H_
#define CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ANDROID_ANDROID_BOOKMARK_BAR_ACTION_ADAPTER_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/browser_apis/ui_controllers/bookmark_bar/adapters/action_adapter.h"
#include "ui/base/mojom/menu_source_type.mojom-forward.h"
#include "ui/base/mojom/window_open_disposition.mojom-forward.h"

class BrowserWindowInterface;

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

namespace gfx {
class Point;
}  // namespace gfx

// Carries out the bookmark bar's browser actions on Android.
class AndroidBookmarkBarActionAdapter : public bookmark_bar_api::ActionAdapter {
 public:
  explicit AndroidBookmarkBarActionAdapter(BrowserWindowInterface* browser);
  AndroidBookmarkBarActionAdapter(const AndroidBookmarkBarActionAdapter&) =
      delete;
  AndroidBookmarkBarActionAdapter& operator=(
      const AndroidBookmarkBarActionAdapter&) = delete;
  ~AndroidBookmarkBarActionAdapter() override;

  // bookmark_bar_api::ActionAdapter:
  void OpenAppsPage(ui::mojom::WindowOpenDisposition disposition) override;
  void OpenBookmark(const bookmarks::BookmarkNode* node,
                    ui::mojom::WindowOpenDisposition disposition) override;
  void RecordFolderOpened() override;
  void OpenFolderNodes(const std::vector<const bookmarks::BookmarkNode*>& nodes,
                       ui::mojom::WindowOpenDisposition disposition) override;
  void CanPasteFromClipboard(
      const std::vector<const bookmarks::BookmarkNode*>& selection,
      base::OnceCallback<void(bool)> callback) override;
  void ShowContextMenu(
      const std::vector<const bookmarks::BookmarkNode*>& selection,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type,
      bool can_paste,
      base::OnceClosure on_close) override;

 private:
  const raw_ptr<BrowserWindowInterface> browser_;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ANDROID_ANDROID_BOOKMARK_BAR_ACTION_ADAPTER_H_
