// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_EDITOR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_EDITOR_VIEW_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/bookmarks/bookmark_expanded_state_tracker.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/models/tree_node_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/controls/tree/tree_view_controller.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class LabelButton;
class MenuRunner;
class TreeView;
}

class BookmarkEditorViewTest;
class GURL;
class Profile;

// View that allows the user to edit a bookmark/starred URL. The user can
// change the URL, title and where the bookmark appears as well as adding
// new folders and changing the name of other folders. The editor is used for
// both editing a url bookmark, as well as editing a folder bookmark when
// created from 'Bookmark all tabs'.
//
// Edits are applied to the BookmarkModel when the user presses 'OK'.
//
// To use BookmarkEditorView invoke the static show method.

class BookmarkEditorView : public BookmarkEditor,
                           public views::TreeViewController,
                           public views::DialogDelegateView,
                           public views::TextfieldController,
                           public views::ContextMenuController,
                           public ui::SimpleMenuModel::Delegate,
                           public bookmarks::BookmarkModelObserver {
  METADATA_HEADER(BookmarkEditorView, views::DialogDelegateView)

 public:
  // Type of node in the tree. Public purely for testing.
  typedef ui::TreeNodeWithValue<int64_t> EditorNode;

  // Model for the TreeView. Trivial subclass that doesn't allow titles with
  // empty strings. Public purely for testing.
  class EditorTreeModel : public ui::TreeNodeModel<EditorNode> {
   public:
    explicit EditorTreeModel(std::unique_ptr<EditorNode> root)
        : ui::TreeNodeModel<EditorNode>(std::move(root)) {}

    EditorTreeModel(const EditorTreeModel&) = delete;
    EditorTreeModel& operator=(const EditorTreeModel&) = delete;

    void SetTitle(ui::TreeModelNode* node,
                  const std::u16string& title) override;
  };

  BookmarkEditorView(Profile* profile,
                     const EditDetails& details,
                     BookmarkEditor::Configuration configuration,
                     BookmarkEditor::OnSaveCallback on_save_callback);
  BookmarkEditorView(const BookmarkEditorView&) = delete;
  BookmarkEditorView& operator=(const BookmarkEditorView&) = delete;
  ~BookmarkEditorView() override;

  // views::DialogDelegateView:
  bool IsDialogButtonEnabled(ui::mojom::DialogButton button) const override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // views::TreeViewController:
  void OnTreeViewSelectionChanged(views::TreeView* tree_view) override;
  bool CanEdit(views::TreeView* tree_view, ui::TreeModelNode* node) override;

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

  // Creates a Window and adds the BookmarkEditorView to it. When the window is
  // closed the BookmarkEditorView is deleted.
  void Show(gfx::NativeWindow parent);

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

 private:
  friend class BookmarkEditorViewTest;

  // bookmarks::BookmarkModelObserver:
  // Any structural change results in resetting the tree model.
  void BookmarkModelLoaded(bool ids_reassigned) override {}
  void BookmarkNodeMoved(const bookmarks::BookmarkNode* old_parent,
                         size_t old_index,
                         const bookmarks::BookmarkNode* new_parent,
                         size_t new_index) override;
  void BookmarkNodeAdded(const bookmarks::BookmarkNode* parent,
                         size_t index,
                         bool added_by_user) override;
  void BookmarkNodeRemoved(const bookmarks::BookmarkNode* parent,
                           size_t index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& removed_urls,
                           const base::Location& location) override;
  void BookmarkAllUserNodesRemoved(const std::set<GURL>& removed_urls,
                                   const base::Location& location) override;
  void BookmarkNodeChanged(const bookmarks::BookmarkNode* node) override {}
  void BookmarkNodeChildrenReordered(
      const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeFaviconChanged(
      const bookmarks::BookmarkNode* node) override {}

  // Creates the necessary sub-views, configures them, adds them to the layout,
  // and requests the entries to display from the database.
  void Init();

  // Resets the model of the tree and updates the various buttons appropriately.
  void Reset();

  // Expands all the nodes in the tree and selects the parent node of the
  // url we're editing or the most recent parent if the url being editted isn't
  // starred.
  void ExpandAndSelect();

  // Creates a returns the new root node. This invokes CreateNodes to do
  // the real work.
  std::unique_ptr<EditorNode> CreateRootNode();

  // Adds and creates a child node in b_node for all children of bb_node that
  // are folders.
  void CreateNodes(const bookmarks::BookmarkNode* bb_node, EditorNode* b_node);

  // Returns the node with the specified id, or NULL if one can't be found.
  EditorNode* FindNodeWithID(BookmarkEditorView::EditorNode* node, int64_t id);

  // Applies the edits done by the user. |parent| gives the parent of the URL
  // being edited. If |parent| is null, the selected node from the treeview's
  // parent is used.
  void ApplyEdits(EditorNode* parent = nullptr);

  // Recursively adds newly created folders and sets the title of nodes to
  // match the user edited title.
  //
  // bb_node gives the BookmarkNode the edits are to be applied to, with b_node
  // the source of the edits.
  //
  // If b_node == parent_b_node, parent_bb_node is set to bb_node. This is
  // used to determine the new BookmarkNode parent based on the EditorNode
  // parent.
  void ApplyNameChangesAndCreateNewFolders(
      const bookmarks::BookmarkNode* bb_node,
      BookmarkEditorView::EditorNode* b_node,
      BookmarkEditorView::EditorNode* parent_b_node,
      const bookmarks::BookmarkNode** parent_bb_node);

  // Returns the current url the user has input.
  GURL GetInputURL() const;

  // Invoked when the url or title has possibly changed. Updates the background
  // of Textfields and ok button appropriately.
  void UserInputChanged();

  void NewFolderButtonPressed();

  // Creates a new folder as a child of the given node. Starts editing on the
  // new group as well.
  void NewFolder(EditorNode* parent);

  // Creates a new EditorNode as the last child of parent. The new node is
  // added to the model and returned. This does NOT start editing. This is used
  // internally by NewFolder and broken into a separate method for testing.
  EditorNode* AddNewFolder(EditorNode* parent);

  // If |editor_node| is expanded it's added to |expanded_nodes| and this is
  // recursively invoked for all the children.
  void UpdateExpandedNodes(EditorNode* editor_node,
                           BookmarkExpandedStateTracker::Nodes* expanded_nodes);

  enum {
    kContextMenuItemEdit = 1,
    kContextMenuItemDelete,
    kContextMenuItemNewFolder,
  };
  ui::SimpleMenuModel* GetMenuModel();

  // Helper functions that implements the IDS_DELETE logic for ExecuteCommand,
  // used in tests to fake the modal dialog.
  void ExecuteCommandDelete(
      base::OnceCallback<bool(const bookmarks::BookmarkNode* node)>
          non_empty_folder_confirmation_cb);

  // Profile the entry is from.
  raw_ptr<Profile> profile_;

  // Model driving the TreeView.
  std::unique_ptr<EditorTreeModel> tree_model_;

  // Displays star folder.
  raw_ptr<views::TreeView> tree_view_ = nullptr;

  // Used to create a new folder.
  raw_ptr<views::LabelButton> new_folder_button_ = nullptr;

  // The text field used for editing the URL.
  raw_ptr<views::Textfield> url_tf_ = nullptr;

  // The text field used for editing the title.
  raw_ptr<views::Textfield> title_tf_ = nullptr;

  const EditDetails details_;

  // The context menu.
  std::unique_ptr<ui::SimpleMenuModel> context_menu_model_;
  std::unique_ptr<views::MenuRunner> context_menu_runner_;

  // Mode used to create nodes from.
  raw_ptr<bookmarks::BookmarkModel> bb_model_;
  // Corresponding expanded state tracker.
  raw_ptr<BookmarkExpandedStateTracker> expanded_state_tracker_;

  // If true, we're running the menu for the bookmark bar or other bookmarks
  // nodes.
  bool running_menu_for_root_ = false;

  // Is the tree shown?
  const bool show_tree_;

  // List of deleted bookmark folders.
  std::vector<int64_t> deletes_;

  // Any extra logic that should be run after the save button is clicked,
  // defined by the caller of BookmarkEditor::Show.
  BookmarkEditor::OnSaveCallback on_save_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_EDITOR_VIEW_H_
