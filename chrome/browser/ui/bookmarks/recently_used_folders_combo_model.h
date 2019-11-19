// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_RECENTLY_USED_FOLDERS_COMBO_MODEL_H_
#define CHROME_BROWSER_UI_BOOKMARKS_RECENTLY_USED_FOLDERS_COMBO_MODEL_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
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
  ~RecentlyUsedFoldersComboModel() override;

  // Overridden from ui::ComboboxModel:
  int GetItemCount() const override;
  base::string16 GetItemAt(int index) override;
  bool IsItemSeparatorAt(int index) override;
  int GetDefaultIndex() const override;
  void AddObserver(ui::ComboboxModelObserver* observer) override;
  void RemoveObserver(ui::ComboboxModelObserver* observer) override;

  // Overriden from bookmarks::BookmarkModelObserver:
  void BookmarkModelLoaded(bookmarks::BookmarkModel* model,
                           bool ids_reassigned) override;
  void BookmarkModelBeingDeleted(bookmarks::BookmarkModel* model) override;
  void BookmarkNodeMoved(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* old_parent,
                         size_t old_index,
                         const bookmarks::BookmarkNode* new_parent,
                         size_t new_index) override;
  void BookmarkNodeAdded(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* parent,
                         size_t index) override;
  void OnWillRemoveBookmarks(bookmarks::BookmarkModel* model,
                             const bookmarks::BookmarkNode* parent,
                             size_t old_index,
                             const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeRemoved(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& removed_urls) override;
  void BookmarkNodeChanged(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeFaviconChanged(bookmarks::BookmarkModel* model,
                                  const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeChildrenReordered(
      bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* node) override;
  void BookmarkAllUserNodesRemoved(bookmarks::BookmarkModel* model,
                                   const std::set<GURL>& removed_urls) override;

  // If necessary this function moves |node| into the corresponding folder for
  // the given |selected_index|.
  void MaybeChangeParent(const bookmarks::BookmarkNode* node,
                         int selected_index);

 private:
  // Returns the node at the specified |index|.
  const bookmarks::BookmarkNode* GetNodeAt(int index);

  // Removes |node| from |items_|. Does nothing if |node| is not in |items_|.
  void RemoveNode(const bookmarks::BookmarkNode* node);

  struct Item;
  std::vector<Item> items_;

  bookmarks::BookmarkModel* bookmark_model_;

  // The index of the original parent folder.
  int node_parent_index_;

  base::ObserverList<ui::ComboboxModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(RecentlyUsedFoldersComboModel);
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_RECENTLY_USED_FOLDERS_COMBO_MODEL_H_
