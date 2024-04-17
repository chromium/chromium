// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_RECENTLY_USED_FOLDERS_COMBO_MODEL_H_
#define CHROME_BROWSER_UI_BOOKMARKS_RECENTLY_USED_FOLDERS_COMBO_MODEL_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "ui/base/models/combobox_model.h"

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}

// Model for the combobox showing the list of folders to choose from. The
// list always contains the Bookmarks Bar, Other Bookmarks and the parent
// folder. The list also contains an extra item that shows the text
// "Choose Another Folder...".
class RecentlyUsedFoldersComboModel : public ui::ComboboxModel,
                                      public bookmarks::BookmarkModelObserver {
 public:
  RecentlyUsedFoldersComboModel(bookmarks::BookmarkModel* model,
                                const bookmarks::BookmarkNode* node);

  RecentlyUsedFoldersComboModel(const RecentlyUsedFoldersComboModel&) = delete;
  RecentlyUsedFoldersComboModel& operator=(
      const RecentlyUsedFoldersComboModel&) = delete;

  ~RecentlyUsedFoldersComboModel() override;

  // Overridden from ui::ComboboxModel:
  size_t GetItemCount() const override;
  std::u16string GetItemAt(size_t index) const override;
  bool IsItemSeparatorAt(size_t index) const override;
  std::optional<size_t> GetDefaultIndex() const override;

  // Overridden from bookmarks::BookmarkModelObserver:
  void BookmarkModelLoaded(bool ids_reassigned) override;
  void BookmarkModelBeingDeleted() override;
  void BookmarkNodeMoved(const bookmarks::BookmarkNode* old_parent,
                         size_t old_index,
                         const bookmarks::BookmarkNode* new_parent,
                         size_t new_index) override;
  void BookmarkNodeAdded(const bookmarks::BookmarkNode* parent,
                         size_t index,
                         bool added_by_user) override;
  void OnWillRemoveBookmarks(const bookmarks::BookmarkNode* parent,
                             size_t old_index,
                             const bookmarks::BookmarkNode* node,
                             const base::Location& location) override;
  void BookmarkNodeRemoved(const bookmarks::BookmarkNode* parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& removed_urls,
                           const base::Location& location) override;
  void BookmarkNodeChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeFaviconChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeChildrenReordered(
      const bookmarks::BookmarkNode* node) override;
  void BookmarkAllUserNodesRemoved(const std::set<GURL>& removed_urls,
                                   const base::Location& location) override;

  // If necessary this function moves |node| into the corresponding folder for
  // the given |selected_index|.
  void MaybeChangeParent(const bookmarks::BookmarkNode* node,
                         size_t selected_index);

 private:
  // Returns the node at the specified |index|.
  const bookmarks::BookmarkNode* GetNodeAt(size_t index);

  // Removes |node| from |items_|. Does nothing if |node| is not in |items_|.
  void RemoveNode(const bookmarks::BookmarkNode* node);

  struct Item;
  std::vector<Item> items_;

  const raw_ptr<bookmarks::BookmarkModel> bookmark_model_;

  const raw_ptr<const bookmarks::BookmarkNode, DanglingUntriaged> parent_node_;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_RECENTLY_USED_FOLDERS_COMBO_MODEL_H_
