// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_MENU_BRIDGE_H_
#define CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_MENU_BRIDGE_H_

#include <map>

#import "base/mac/scoped_nsobject.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"

class Profile;
@class NSImage;
@class NSMenu;
@class NSMenuItem;
@class BookmarkMenuCocoaController;

namespace bookmarks {
class BookmarkNode;
}

namespace test {
class AppMenuControllerTest;
}

// C++ controller for the bookmark menu; one per AppController (which
// means there is only one).  When bookmarks are changed, this class
// takes care of updating Cocoa bookmark menus.  This is not named
// BookmarkMenuController to help avoid confusion between languages.
// This class needs to be C++, not ObjC, since it derives from
// BookmarkModelObserver.
//
// Most Chromium Cocoa menu items are static from a nib (e.g. New
// Tab), but may be enabled/disabled under certain circumstances
// (e.g. Cut and Paste).  In addition, most Cocoa menu items have
// firstResponder: as a target.  Unusually, bookmark menu items are
// created dynamically.  They also have a target of
// BookmarkMenuCocoaController instead of firstResponder.
// See BookmarkMenuBridge::AddNodeToMenu()).
class BookmarkMenuBridge : public bookmarks::BookmarkModelObserver {
 public:
  BookmarkMenuBridge(Profile* profile, NSMenu* menu_root);
  ~BookmarkMenuBridge() override;

  // bookmarks::BookmarkModelObserver:
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
  void BookmarkNodeRemoved(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& removed_urls) override;
  void BookmarkAllUserNodesRemoved(bookmarks::BookmarkModel* model,
                                   const std::set<GURL>& removed_urls) override;
  void BookmarkNodeChanged(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeFaviconChanged(bookmarks::BookmarkModel* model,
                                  const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeChildrenReordered(
      bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* node) override;

  // Rebuilds the main bookmark menu, if it has been marked invalid. Or builds
  // a bookmark folder submenu on demand.
  void UpdateMenu(NSMenu* menu, const bookmarks::BookmarkNode* node);

  // I wish I had a "friend @class" construct.
  bookmarks::BookmarkModel* GetBookmarkModel();
  Profile* GetProfile();

  // Return the Bookmark menu.
  NSMenu* BookmarkMenu();

  // Clear all bookmarks from |menu_root_|.
  void ClearBookmarkMenu();

 private:
  friend class BookmarkMenuBridgeTest;
  friend class test::AppMenuControllerTest;

  void BuildRootMenu();

  // Mark the bookmark menu as being invalid.
  void InvalidateMenu()  { menuIsValid_ = false; }
  bool IsMenuValid() const { return menuIsValid_; }

  // Helper for adding the node as a submenu to the menu with the |node|'s title
  // and the given |image| as its icon.
  // If |add_extra_items| is true, also adds extra menu items at bottom of
  // menu, such as "Open All Bookmarks".
  void AddNodeAsSubmenu(NSMenu* menu,
                        const bookmarks::BookmarkNode* node,
                        NSImage* image);

  // Helper for recursively adding items to our bookmark menu.
  // All children of |node| will be added to |menu|.
  // If |add_extra_items| is true, also adds extra menu items at bottom of
  // menu, such as "Open All Bookmarks".
  // TODO(jrg): add a counter to enforce maximum nodes added
  void AddNodeToMenu(const bookmarks::BookmarkNode* node, NSMenu* menu);

  // Helper for adding an item to our bookmark menu. An item which has a
  // localized title specified by |message_id| will be added to |menu|.
  // The item is also bound to |node| by tag. |command_id| selects the action.
  void AddItemToMenu(int command_id,
                     int message_id,
                     const bookmarks::BookmarkNode* node,
                     NSMenu* menu,
                     bool enabled);

  // This configures an NSMenuItem with all the data from a BookmarkNode. This
  // is used to update existing menu items, as well as to configure newly
  // created ones, like in AddNodeToMenu().
  // |set_title| is optional since it is only needed when we get a
  // node changed notification.  On initial build of the menu we set
  // the title as part of alloc/init.
  void ConfigureMenuItem(const bookmarks::BookmarkNode* node,
                         NSMenuItem* item,
                         bool set_title);

  // Returns the NSMenuItem for a given BookmarkNode.
  NSMenuItem* MenuItemForNode(const bookmarks::BookmarkNode* node);

  // Start watching the bookmarks for changes.
  void ObserveBookmarkModel();

  // True iff the menu is up to date with the actual BookmarkModel.
  bool menuIsValid_;

  Profile* const profile_;  // weak
  base::scoped_nsobject<BookmarkMenuCocoaController> controller_;
  base::scoped_nsobject<NSMenu> menu_root_;

  // The folder image so we can use one copy for all.
  base::scoped_nsobject<NSImage> folder_image_;

  // In order to appropriately update items in the bookmark menu, without
  // forcing a rebuild, map the model's nodes to menu items.
  std::map<const bookmarks::BookmarkNode*, NSMenuItem*> bookmark_nodes_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkMenuBridge);
};

#endif  // CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_MENU_BRIDGE_H_
