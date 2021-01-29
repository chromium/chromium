// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_EDITOR_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_EDITOR_H_

#include <utility>
#include <vector>

#include "base/strings/string16.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "ui/gfx/native_widget_types.h"

class GURL;
class Profile;

namespace bookmarks {
class BookmarkModel;
}

// Small, cross platform interface that shows the correct platform specific
// bookmark editor dialog.
class BookmarkEditor {
 public:
  // An enumeration of the possible configurations offered.
  enum Configuration {
    // If Configuration is SHOW_TREE, a tree is shown allowing the user to
    // choose the parent of the node.
    SHOW_TREE,
    NO_TREE
  };

  // Describes what the user is editing.
  class EditDetails {
   public:
    // Returns the type of the existing or new node.
    bookmarks::BookmarkNode::Type GetNodeType() const;

    // Returns the resource id for the string resource to use on the window
    // title for this edit operation.
    int GetWindowTitleId() const;

    // Returns an EditDetails instance for the user editing the given bookmark.
    static EditDetails EditNode(const bookmarks::BookmarkNode* node);

    // Returns an EditDetails instance for the user adding a bookmark within
    // a given parent node with a specified index.
    static EditDetails AddNodeInFolder(
        const bookmarks::BookmarkNode* parent_node,
        size_t index,
        const GURL& url,
        const base::string16& title);

    // Returns an EditDetails instance for the user adding a folder within a
    // given parent node with a specified index.
    static EditDetails AddFolder(const bookmarks::BookmarkNode* parent_node,
                                 size_t index);

    enum Type {
      // The user is editing an existing node in the model. The node the user
      // is editing is set in |existing_node|.
      EXISTING_NODE,

      // A new bookmark should be created if the user accepts the edit.
      // |existing_node| is null in this case.
      NEW_URL,

      // A new folder bookmark should be created if the user accepts the edit.
      // The contents of the folder should be that of |urls|.
      // |existing_node| is null in this case.
      NEW_FOLDER
    };

    EditDetails(const EditDetails& other);
    ~EditDetails();

    // See description of enum value for details.
    const Type type;

    // If type == EXISTING_NODE this gives the existing node.
    const bookmarks::BookmarkNode* existing_node = nullptr;

    // If type == NEW_URL or type == NEW_FOLDER this gives the initial parent
    // node to place the new node in.
    const bookmarks::BookmarkNode* parent_node = nullptr;

    // If type == NEW_URL or type == NEW_FOLDER this gives the index to insert
    // the new node at.
    base::Optional<size_t> index;

    // If type == NEW_URL this gives the URL/title.
    GURL url;
    base::string16 title;

    // If type == NEW_FOLDER, this is the urls/title pairs to add to the
    // folder.
    std::vector<std::pair<GURL, base::string16> > urls;

   private:
    explicit EditDetails(Type node_type);
  };

  // Shows the bookmark editor. The bookmark editor allows editing an existing
  // node or creating a new bookmark node (as determined by |details.type|).
  // |details.parent_node| is only used if |details.existing_node| is null.
  static void Show(gfx::NativeWindow parent_window,
                   Profile* profile,
                   const EditDetails& details,
                   Configuration configuration);

  // Modifies a bookmark node (assuming that there's no magic that needs to be
  // done regarding moving from one folder to another).  If a new node is
  // explicitly being added, returns a pointer to the new node that was created.
  // Otherwise the return value is identically |node|.
  static const bookmarks::BookmarkNode* ApplyEditsWithNoFolderChange(
      bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* parent,
      const EditDetails& details,
      const base::string16& new_title,
      const GURL& new_url);

  // Modifies a bookmark node assuming that the parent of the node may have
  // changed and the node will need to be removed and reinserted.  If a new node
  // is explicitly being added, returns a pointer to the new node that was
  // created.  Otherwise the return value is identically |node|.
  static const bookmarks::BookmarkNode* ApplyEditsWithPossibleFolderChange(
      bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* new_parent,
      const EditDetails& details,
      const base::string16& new_title,
      const GURL& new_url);
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_EDITOR_H_
