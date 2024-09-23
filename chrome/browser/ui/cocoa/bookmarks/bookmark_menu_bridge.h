// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_MENU_BRIDGE_H_
#define CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_MENU_BRIDGE_H_

#include <map>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"

class Profile;
@class NSImage;
@class NSMenu;
@class NSMenuItem;
@class BookmarkMenuCocoaController;

namespace bookmarks {
class BookmarkNode;
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

  BookmarkMenuBridge(const BookmarkMenuBridge&) = delete;
  BookmarkMenuBridge& operator=(const BookmarkMenuBridge&) = delete;

  ~BookmarkMenuBridge() override;

  // bookmarks::BookmarkModelObserver:
  void BookmarkModelLoaded(bool ids_reassigned) override;
  void BookmarkModelBeingDeleted() override;
  void BookmarkNodeMoved(const bookmarks::BookmarkNode* old_parent,
                         size_t old_index,
                         const bookmarks::BookmarkNode* new_parent,
                         size_t new_index) override;
  void BookmarkNodeAdded(const bookmarks::BookmarkNode* parent,
                         size_t index,
                         bool added_by_user) override;
  void BookmarkNodeRemoved(const bookmarks::BookmarkNode* parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& removed_urls,
                           const base::Location& location) override;
  void BookmarkAllUserNodesRemoved(const std::set<GURL>& removed_urls,
                                   const base::Location& location) override;
  void BookmarkNodeChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeFaviconChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeChildrenReordered(
      const bookmarks::BookmarkNode* node) override;

  // Rebuilds the main bookmark menu, if it has been marked invalid. Or builds
  // a bookmark folder submenu on demand. If |recurse| is true, also fills all
  // submenus recursively.
  void UpdateMenu(NSMenu* menu,
                  const bookmarks::BookmarkNode* node,
                  bool recurse);

  // I wish I had a "friend @class" construct.
  bookmarks::BookmarkModel* GetBookmarkModel();
  Profile* GetProfile();
  const base::FilePath& GetProfileDir() const;

  // Return the Bookmark menu.
  NSMenu* BookmarkMenu();

  // Clear all bookmarks from |menu_root_|.
  void ClearBookmarkMenu();

  // Resets |profile_| to nullptr. Called before the Profile is destroyed, if
  // this bridge is still needed. Rebuilds the entire menu recursively, so it
  // remains functional after the Profile is destroyed.
  //
  // Also performs some internal cleanup, like resetting observers and pointers
  // to the Profile and KeyedServices.
  void OnProfileWillBeDestroyed();

  // Returns the GUID of the BookmarkNode for |tag|. If |tag| is not the tag of
  // an NSMenuItem in this menu, returns the invalid GUID.
  base::Uuid TagToGUID(int64_t tag) const;

  // Returns the NSMenuItem for a given BookmarkNode, exposed publicly for
  // testing.
  NSMenuItem* MenuItemForNodeForTest(const bookmarks::BookmarkNode* node);

 private:
  friend class BookmarkMenuBridgeTest;

  void BuildRootMenu(bool recurse);

  // Mark the bookmark menu as being invalid.
  void InvalidateMenu()  { menuIsValid_ = false; }
  bool IsMenuValid() const { return menuIsValid_; }

  // Helper for adding the node as a submenu to the menu with the |node|'s title
  // and the given |image| as its icon. If |recurse| is true, also fills all
  // submenus recursively.
  void AddNodeAsSubmenu(NSMenu* menu,
                        const bookmarks::BookmarkNode* node,
                        NSImage* image,
                        bool recurse);

  // Helper for adding items to our bookmark menu. All children of |node| will
  // be added to |menu|. If |recurse| is true, also fills all submenus
  // recursively.
  //
  // TODO(jrg): add a counter to enforce maximum nodes added
  void AddNodeToMenu(const bookmarks::BookmarkNode* node,
                     NSMenu* menu,
                     bool recurse);

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

  raw_ptr<Profile> profile_;  // weak
  BookmarkMenuCocoaController* __strong controller_;
  NSMenu* __strong menu_root_;

  base::FilePath profile_dir_;  // Remembered after OnProfileWillBeDestroyed().

  // The folder image so we can use one copy for all.
  NSImage* __strong folder_image_;

  // In order to appropriately update items in the bookmark menu, without
  // forcing a rebuild, map the model's nodes to menu items.
  std::map<const bookmarks::BookmarkNode*, NSMenuItem*> bookmark_nodes_;

  // Tags are NSIntegers, so they're not necessarily large enough to hold a
  // GUID. Instead, map the tags to the corresponding GUIDs.
  std::map<int64_t, base::Uuid> tag_to_guid_;

  base::ScopedObservation<bookmarks::BookmarkModel,
                          bookmarks::BookmarkModelObserver>
      bookmark_model_observation_{this};
};

#endif  // CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_MENU_BRIDGE_H_
