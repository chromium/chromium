// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_MENU_BOOKMARKS_DYNAMIC_MENU_H_
#define CHROME_BROWSER_UI_VIEWS_APP_MENU_BOOKMARKS_DYNAMIC_MENU_H_

#include <variant>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_observer.h"
#include "chrome/browser/bookmarks/bookmark_parent_folder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/app_menu/app_menu_drag_and_drop_delegate.h"
#include "ui/actions/actions.h"

namespace bookmarks {
class BookmarkNode;
}

class BookmarksDynamicMenu : public BookmarkMergedSurfaceServiceObserver {
 public:
  class BookmarkFolderOrURL {
   public:
    explicit BookmarkFolderOrURL(const bookmarks::BookmarkNode* node);
    explicit BookmarkFolderOrURL(const BookmarkParentFolder& folder);
    ~BookmarkFolderOrURL();

    BookmarkFolderOrURL(const BookmarkFolderOrURL& other);
    BookmarkFolderOrURL& operator=(const BookmarkFolderOrURL& other);

    friend bool operator==(const BookmarkFolderOrURL&,
                           const BookmarkFolderOrURL&) = default;

    const BookmarkParentFolder* GetIfBookmarkFolder() const;
    const bookmarks::BookmarkNode* GetIfBookmarkURL() const;
    const bookmarks::BookmarkNode* GetIfNonPermanentNode() const;

   private:
    static std::variant<BookmarkParentFolder,
                        raw_ptr<const bookmarks::BookmarkNode>>
    GetFromNode(const bookmarks::BookmarkNode* node);

    std::variant<BookmarkParentFolder, raw_ptr<const bookmarks::BookmarkNode>>
        folder_or_url_;
  };

  BookmarksDynamicMenu(BrowserWindowInterface* browser,
                       AppMenuDragAndDropDelegate::Host* host);
  BookmarksDynamicMenu(const BookmarksDynamicMenu&) = delete;
  BookmarksDynamicMenu& operator=(const BookmarksDynamicMenu&) = delete;
  ~BookmarksDynamicMenu() override;

  base::WeakPtr<BookmarksDynamicMenu> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void BuildBookmarksActions(actions::BaseAction* parent_item);

  // BookmarkMergedSurfaceServiceObserver:
  void BookmarkMergedSurfaceServiceLoaded() override;
  void BookmarkMergedSurfaceServiceBeingDeleted() override;
  void BookmarkNodeAdded(const BookmarkParentFolder& parent,
                         size_t index) override;
  void BookmarkNodesRemoved(
      const BookmarkParentFolder& parent,
      const base::flat_set<const bookmarks::BookmarkNode*>& nodes) override;
  void BookmarkNodeMoved(const BookmarkParentFolder& old_parent,
                         size_t old_index,
                         const BookmarkParentFolder& new_parent,
                         size_t new_index) override;
  void BookmarkNodeChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeFaviconChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkParentFolderChildrenReordered(
      const BookmarkParentFolder& folder) override;
  void BookmarkAllUserNodesRemoved() override;

 private:
  BookmarkMergedSurfaceService* GetBookmarkMergedSurfaceService() const;
  actions::ActionItem* FindActionForTarget(
      const BookmarkFolderOrURL& target) const;
  actions::ActionItem* FindActionForNode(
      const bookmarks::BookmarkNode* node) const;
  actions::BaseAction* GetParentActionForFolder(
      const BookmarkParentFolder& folder) const;
  actions::BaseAction* GetInsertAfterAction(
      const BookmarkParentFolder& parent_folder,
      size_t index) const;

  actions::ActionItem* AddBookmarkNodeAction(
      actions::BaseAction* parent_item,
      const bookmarks::BookmarkNode* node,
      BookmarkMergedSurfaceService* service);

  actions::ActionItem* AddBookmarkFolderAction(
      actions::BaseAction* parent_item,
      const BookmarkParentFolder& folder,
      BookmarkMergedSurfaceService* service);

  raw_ptr<BrowserWindowInterface> browser_window_interface_;
  raw_ptr<AppMenuDragAndDropDelegate::Host> host_;
  base::WeakPtr<actions::ActionItem> dynamic_section_;
  base::ScopedObservation<BookmarkMergedSurfaceService,
                          BookmarkMergedSurfaceServiceObserver>
      bookmark_service_observation_{this};
  base::WeakPtrFactory<BookmarksDynamicMenu> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_BOOKMARKS_DYNAMIC_MENU_H_
