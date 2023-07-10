// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_COMMERCE_SHOPPING_UI_HANDLER_DELEGATE_H_
#define CHROME_BROWSER_UI_WEBUI_COMMERCE_SHOPPING_UI_HANDLER_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "components/commerce/core/webui/shopping_list_handler.h"

class ShoppingInsightsSidePanelUI;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

class Profile;

namespace commerce {

class ShoppingUiHandlerDelegate : public ShoppingListHandler::Delegate {
 public:
  ShoppingUiHandlerDelegate(ShoppingInsightsSidePanelUI* insights_side_panel_ui,
                            Profile* profile);
  ShoppingUiHandlerDelegate(const ShoppingUiHandlerDelegate&) = delete;
  ShoppingUiHandlerDelegate& operator=(const ShoppingUiHandlerDelegate&) =
      delete;
  ~ShoppingUiHandlerDelegate() override;

  absl::optional<GURL> GetCurrentTabUrl() override;

  void ShowInsightsSidePanelUI() override;

  const bookmarks::BookmarkNode* GetOrAddBookmarkForCurrentUrl() override;

  void OpenUrlInNewTab(const GURL& url) override;

  void ShowBookmarkEditorForCurrentUrl() override;

  void ShowFeedback() override;

 private:
  // This delegate is owned by |insights_side_panel_ui_| so we expect
  // |insights_side_panel_ui_| to remain valid for the lifetime of |this|.
  raw_ptr<ShoppingInsightsSidePanelUI> insights_side_panel_ui_;
  raw_ptr<Profile> profile_;
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
};

}  // namespace commerce

#endif  // CHROME_BROWSER_UI_WEBUI_COMMERCE_SHOPPING_UI_HANDLER_DELEGATE_H_
