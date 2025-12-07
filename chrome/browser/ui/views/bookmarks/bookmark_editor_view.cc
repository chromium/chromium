// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_editor_view.h"

#include <algorithm>
#include <set>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_expanded_state_tracker_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/history/core/browser/history_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_fixer.h"
#include "components/user_prefs/user_prefs.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/menu_source_type.mojom-forward.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/events/event.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/tree/tree_view.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_MAC)
#include "ui/aura/window.h"
#endif  // !BUILDFLAG(IS_MAC)

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

BookmarkEditorView::BookmarkEditorView(
    Profile* profile,
    const EditDetails& details,
    BookmarkEditor::Configuration configuration,
    BookmarkEditor::OnSaveCallback on_save_callback)
    : profile_(profile),
      details_(details),
      bb_model_(BookmarkModelFactory::GetForBrowserContext(profile)),
      expanded_state_tracker_(
          BookmarkExpandedStateTrackerFactory::GetForProfile(profile)),
      show_tree_(configuration == SHOW_TREE),
      on_save_callback_(std::move(on_save_callback)) {
  DCHECK(profile);
  DCHECK(bb_model_);
  DCHECK(expanded_state_tracker_);
  DCHECK(!bb_model_->client()->IsNodeManaged(details_.parent_node));
  SetCanResize(true);
  SetModalType(ui::mojom::ModalType::kWindow);
  SetShowCloseButton(false);
  SetAcceptCallback(base::BindOnce(&BookmarkEditorView::ApplyEdits,
                                   base::Unretained(this), nullptr));
  SetTitle(details_.GetWindowTitleId());
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(IDS_SAVE));
  if (show_tree_) {
    new_folder_button_ = SetExtraView(std::make_unique<views::MdTextButton>(
        base::BindRepeating(&BookmarkEditorView::NewFolderButtonPressed,
                            base::Unretained(this)),
        l10n_util::GetStringUTF16(IDS_BOOKMARK_EDITOR_NEW_FOLDER_BUTTON)));
    new_folder_button_->SetEnabled(bb_model_->loaded());
  }
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kControl, views::DialogContentType::kControl));
  Init();

  // TODO(crbug.com/40863584):  We need this View to have a role before setting
  // its name, but if we set it to dialog, we'll wind up with a dialog (this
  // view) inside of a dialog (RootView). Note that both views also share the
  // same accessible name. In the meantime, give it a generic role.
  GetViewAccessibility().SetRole(ax::mojom::Role::kPane);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF8(IDS_BOOKMARK_EDITOR_TITLE));
}

BookmarkEditorView::~BookmarkEditorView() {
  // The tree model is deleted before the view. Reset the model otherwise the
  // tree will reference a deleted model.
  if (tree_view_) {
    tree_view_->SetModel(nullptr);
  }
  bb_model_->RemoveObserver(this);
}

bool BookmarkEditorView::IsBookmarkFolderSelected() const {
  // This question doesn't make sense for configurations where there's no folder
  // picker. Instead of responding no and having unintended side effects, make
  // sure we're never called.
  CHECK(show_tree_);
  CHECK(tree_model_);
  return tree_model_->AsNode(tree_view_->GetSelectedNode())->value.type ==
         EditorNodeData::Type::kFolder;
}

bool BookmarkEditorView::IsDialogButtonEnabled(
    ui::mojom::DialogButton button) const {
  if (button == ui::mojom::DialogButton::kOk) {
    if (!bb_model_->loaded()) {
      return false;
    }

    // Can't move bookmark to non-bookmark node.
    if (show_tree_ && !IsBookmarkFolderSelected()) {
      return false;
    }

    // Disable if the selected node is a descendant of any of the existing
    // nodes to move.
    if (details_.type == BookmarkEditor::EditDetails::MOVE) {
      const BookmarkNode* selected_node = GetBookmarkNodeByID(
          bb_model_, tree_model_->AsNode(tree_view_->GetSelectedNode())
                         ->value.bookmark_node_id);

      while (!bb_model_->is_permanent_node(selected_node)) {
        if (details_.existing_nodes_to_move.contains(selected_node)) {
          return false;
        }
        selected_node = selected_node->parent();
      }
    }

    if (details_.CanChangeUrl()) {
      return GetInputURL().is_valid();
    }
  }
  return true;
}

gfx::Size BookmarkEditorView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  if (!show_tree_) {
    return views::View::CalculatePreferredSize(available_size);
  }

  return gfx::Size(views::Widget::GetLocalizedContentsSize(
      IDS_EDITBOOKMARK_DIALOG_WIDTH_CHARS,
      IDS_EDITBOOKMARK_DIALOG_HEIGHT_LINES));
}

void BookmarkEditorView::OnTreeViewSelectionChanged(
    views::TreeView* tree_view) {
  // Make sure the dialog can only be accepted (edits applied) if we've selected
  // a bookmark node (polled in ::IsDialogButtonEnabled()).
  //
  // The save button is updated through
  // DialogModelChanged()->IsDialogButtonEnabled().
  DialogModelChanged();
  if (new_folder_button_) {
    new_folder_button_->SetEnabled(IsBookmarkFolderSelected());
  }
}

bool BookmarkEditorView::CanEdit(views::TreeView* tree_view,
                                 ui::TreeModelNode* node) {
  return CanEdit(node);
}

bool BookmarkEditorView::CanEdit(ui::TreeModelNode* node) const {
  // Only allow editing of children of the bookmark bar node and other node.
  EditorNode* bb_node = tree_model_->AsNode(node);
  return (bb_node->value.type == EditorNodeData::Type::kFolder &&
          bb_node->parent() &&
          bb_node->parent()->value.type == EditorNodeData::Type::kFolder);
}

void BookmarkEditorView::ContentsChanged(views::Textfield* sender,
                                         const std::u16string& new_contents) {
  UserInputChanged();
}

bool BookmarkEditorView::HandleKeyEvent(views::Textfield* sender,
                                        const ui::KeyEvent& key_event) {
  return false;
}

bool BookmarkEditorView::IsCommandIdChecked(int command_id) const {
  return false;
}

bool BookmarkEditorView::IsCommandIdEnabled(int command_id) const {
  ui::TreeModelNode* const node = tree_view_->GetActiveNode();
  switch (command_id) {
    case kContextMenuItemEdit:
    case kContextMenuItemDelete:
      return CanEdit(node);
    case kContextMenuItemNewFolder:
      return tree_model_->AsNode(node)->value.type ==
             EditorNodeData::Type::kFolder;
    default:
      NOTREACHED();
  }
}

bool BookmarkEditorView::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  return GetWidget()->GetAccelerator(command_id, accelerator);
}

void BookmarkEditorView::ExecuteCommand(int command_id, int event_flags) {
  DCHECK(tree_view_->GetActiveNode());
  if (command_id == kContextMenuItemEdit) {
    tree_view_->StartEditing(tree_view_->GetActiveNode());
  } else if (command_id == kContextMenuItemDelete) {
    ExecuteCommandDelete(base::BindOnce(&bookmarks::ConfirmDeleteBookmarkNode,
                                        GetWidget()->GetNativeWindow()));
  } else {
    DCHECK_EQ(kContextMenuItemNewFolder, command_id);
    NewFolder(tree_model_->AsNode(tree_view_->GetActiveNode()));
  }
}

void BookmarkEditorView::Show(gfx::NativeWindow parent) {
  constrained_window::CreateBrowserModalDialogViews(this, parent);
  UserInputChanged();
  if (show_tree_ && bb_model_->loaded()) {
    ExpandAndSelect();
  }
  GetWidget()->Show();

  if (title_tf_) {
    // Select all the text in the name Textfield.
    title_tf_->SelectAll(true);
    // Give focus to the name Textfield.
    title_tf_->RequestFocus();
  }
}

void BookmarkEditorView::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  DCHECK_EQ(tree_view_, source);
  if (!tree_view_->GetActiveNode()) {
    return;
  }

  context_menu_runner_ = std::make_unique<views::MenuRunner>(
      GetMenuModel(),
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU);

  context_menu_runner_->RunMenuAt(source->GetWidget()->GetTopLevelWidget(),
                                  nullptr, gfx::Rect(point, gfx::Size()),
                                  views::MenuAnchorPosition::kTopRight,
                                  source_type);
}

bool BookmarkEditorView::IsNewFolderButtonEnabledForTesting() const {
  CHECK(new_folder_button_);
  return new_folder_button_->GetEnabled();
}

void BookmarkEditorView::BookmarkNodeMoved(const BookmarkNode* old_parent,
                                           size_t old_index,
                                           const BookmarkNode* new_parent,
                                           size_t new_index) {
  Reset();
}

void BookmarkEditorView::BookmarkNodeAdded(const BookmarkNode* parent,
                                           size_t index,
                                           bool added_by_user) {
  Reset();
}

void BookmarkEditorView::BookmarkNodeRemoved(const BookmarkNode* parent,
                                             size_t index,
                                             const BookmarkNode* node,
                                             const std::set<GURL>& removed_urls,
                                             const base::Location& location) {
  // Either `existing_nodes_to_move` or `existing_node` has the removed `node`
  // as an ancestor, and can therefore not be moved/edited anymore.
  // TODO(crbug.com/407025895): This algorithm has squared complexity. This is
  // acceptable considering the circumstances that deleting a node while the
  // move dialog is open is an edge case, and it is not unlikely that the user
  // moves fewer nodes than can be found in the tree structure of a deleted
  // folder node. But we should still consider using DFS instead.
  bool selected_node_removed =
      (details_.type == EditDetails::MOVE &&
       std::ranges::any_of(details_.existing_nodes_to_move,
                           [&node](const BookmarkNode* existing_node) {
                             return existing_node->HasAncestor(node);
                           })) ||
      (details_.type == EditDetails::EXISTING_NODE &&
       details_.existing_node->HasAncestor(node));
  if (selected_node_removed ||
      (details_.parent_node && details_.parent_node->HasAncestor(node))) {
    // A node, or its parent was removed. Close the dialog.
    GetWidget()->Close();
  } else {
    Reset();
  }
}

void BookmarkEditorView::BookmarkNodeChanged(
    const bookmarks::BookmarkNode* node) {
  // Only reset for folders, since bookmarks (urls) are not shown in the dialog.
  if (node->is_folder()) {
    Reset();
  }
}

void BookmarkEditorView::BookmarkAllUserNodesRemoved(
    const std::set<GURL>& removed_urls,
    const base::Location& location) {
  Reset();
}

void BookmarkEditorView::BookmarkNodeChildrenReordered(
    const BookmarkNode* node) {
  Reset();
}

void BookmarkEditorView::Init() {
  bb_model_->AddObserver(this);

  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, gfx::Insets(),
          ChromeLayoutProvider::Get()->GetDistanceMetric(
              views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  // The `MOVE` version does not have a title or URL text field.
  if (details_.type != BookmarkEditor::EditDetails::MOVE) {
    AddLabels();
  }

  if (show_tree_) {
    auto tree_view = std::make_unique<views::TreeView>();
    tree_view->SetRootShown(false);
    tree_view->set_context_menu_controller(this);
    tree_view_ = tree_view.get();
    auto* scroll_view = AddChildView(
        views::TreeView::CreateScrollViewWithTree(std::move(tree_view)));
    layout->SetFlexForView(scroll_view, 1);
  }

  if (!show_tree_ || bb_model_->loaded()) {
    Reset();
  }
}

void BookmarkEditorView::Reset() {
  if (!show_tree_) {
    if (parent()) {
      UserInputChanged();
    }
    return;
  }

  // Do this first, otherwise when we invoke SetModel with the real one
  // tree_view will try to invoke something on the model we just deleted.
  tree_view_->SetModel(nullptr);

  tree_model_ = std::make_unique<EditorTreeModel>(CreateRootNode());

  tree_view_->SetModel(tree_model_.get());
  tree_view_->SetController(this);

  if (new_folder_button_) {
    new_folder_button_->SetEnabled(IsBookmarkFolderSelected());
  }

  context_menu_runner_.reset();

  if (parent()) {
    ExpandAndSelect();
  }
}

GURL BookmarkEditorView::GetInputURL() const {
  if (!url_tf_) {
    return GURL();
  }
  return url_formatter::FixupURL(base::UTF16ToUTF8(url_tf_->GetText()),
                                 std::string());
}

void BookmarkEditorView::UserInputChanged() {
  if (url_tf_) {
    const GURL url(GetInputURL());
    url_tf_->SetInvalid(!url.is_valid());
  }
  DialogModelChanged();
}

void BookmarkEditorView::NewFolderButtonPressed() {
  DCHECK(tree_view_->GetSelectedNode());
  NewFolder(tree_model_->AsNode(tree_view_->GetSelectedNode()));
}

void BookmarkEditorView::NewFolder(EditorNode* parent) {
  // Create a new entry parented to the given item.
  DCHECK(parent);
  tree_view_->StartEditing(AddNewFolder(parent));
}

BookmarkEditorView::EditorNode* BookmarkEditorView::AddNewFolder(
    EditorNode* parent) {
  // bookmark_node_id will get populated when applying changes.
  auto new_folder_node = std::make_unique<EditorNode>(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_EDITOR_NEW_FOLDER_NAME),
      EditorNodeData{EditorNodeData::Type::kFolder});
  new_folder_node->SetPlaceholderAccessibleTitle(
      l10n_util::GetStringUTF16(IDS_UNNAMED_BOOKMARK_FOLDER));
  return tree_model_->Add(parent, std::move(new_folder_node));
}

void BookmarkEditorView::AddLabels() {
  std::u16string title;
  GURL url;
  if (details_.type == EditDetails::EXISTING_NODE) {
    title = details_.existing_node->GetTitle();
    url = details_.existing_node->url();
  } else if (details_.type == EditDetails::NEW_FOLDER) {
    title = l10n_util::GetStringUTF16(IDS_BOOKMARK_EDITOR_NEW_FOLDER_NAME);
  } else if (details_.type == EditDetails::CONVERT_TAB_GROUP_TO_FOLDER) {
    title = details_.bookmark_data.title.empty()
                ? l10n_util::GetStringUTF16(IDS_BOOKMARK_EDITOR_NEW_FOLDER_NAME)
                : details_.bookmark_data.title;
  } else if (details_.type == EditDetails::NEW_URL) {
    url = details_.bookmark_data.url.value();
    title = details_.bookmark_data.title;
  }

  // Add a subtitle to tab group to folder dialog.
  if (details_.type == EditDetails::CONVERT_TAB_GROUP_TO_FOLDER) {
    views::Label* subtitle = AddChildView(std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(IDS_TAB_GROUP_TO_BOOKMARK_FOLDER_SUBTITLE)));
    subtitle->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    subtitle->SetMultiLine(true);
  }

  auto* labels = AddChildView(std::make_unique<views::View>());
  auto* label_layout =
      labels->SetLayoutManager(std::make_unique<views::TableLayout>());

  label_layout
      ->AddColumn(views::LayoutAlignment::kStart,
                  views::LayoutAlignment::kCenter,
                  views::TableLayout::kFixedSize,
                  views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize,
                        ChromeLayoutProvider::Get()->GetDistanceMetric(
                            views::DISTANCE_RELATED_CONTROL_HORIZONTAL))
      .AddColumn(views::LayoutAlignment::kStretch,
                 views::LayoutAlignment::kCenter, 1.0f,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddRows(1, views::TableLayout::kFixedSize);

  labels->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_EDITOR_NAME_LABEL)));
  title_tf_ = labels->AddChildView(std::make_unique<views::Textfield>());
  title_tf_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_AX_EDITOR_NAME_LABEL));
  title_tf_->SetText(title);
  title_tf_->set_controller(this);

  // The URL field does not exist for folders.
  if (details_.CanChangeUrl()) {
    label_layout
        ->AddPaddingRow(views::TableLayout::kFixedSize,
                        ChromeLayoutProvider::Get()->GetDistanceMetric(
                            views::DISTANCE_RELATED_CONTROL_VERTICAL))
        .AddRows(1, views::TableLayout::kFixedSize);

    labels->AddChildView(std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(IDS_BOOKMARK_EDITOR_URL_LABEL)));
    url_tf_ = labels->AddChildView(std::make_unique<views::Textfield>());
    url_tf_->SetText(chrome::FormatBookmarkURLForDisplay(url));
    url_tf_->set_controller(this);
    url_tf_->GetViewAccessibility().SetName(
        l10n_util::GetStringUTF16(IDS_BOOKMARK_AX_EDITOR_URL_LABEL));
    url_tf_->SetTextInputType(ui::TextInputType::TEXT_INPUT_TYPE_URL);
  }
}

void BookmarkEditorView::ExpandAndSelect() {
  // Only expand tracked nodes if the feature flag is disabled. With the flag
  // enabled, only the nodes leading up to the selected node's parent should be
  // expanded.
  if (!base::FeatureList::IsEnabled(
          switches::kSyncEnableBookmarksInTransportMode)) {
    BookmarkExpandedStateTracker::Nodes expanded_nodes =
        expanded_state_tracker_->GetExpandedNodes();
    for (const BookmarkNode* node : expanded_nodes) {
      EditorNode* editor_node =
          FindNodeWithID(tree_model_->GetRoot(), node->id());
      if (editor_node) {
        tree_view_->Expand(editor_node);
      }
    }
  }

  const BookmarkNode* to_select = details_.parent_node;
  if (details_.type == EditDetails::EXISTING_NODE) {
    to_select = details_.existing_node->parent();
  }
  int64_t folder_id_to_select = to_select->id();
  EditorNode* b_node =
      FindNodeWithID(tree_model_->GetRoot(), folder_id_to_select);
  if (!b_node) {
    // Default to the bookmark bar folder.
    bool has_title_node =
        tree_model_->GetRoot()->children().front()->value.type ==
        EditorNodeData::Type::kTitle;
    b_node = FindNodeWithID(tree_model_->GetRoot(),
                            has_title_node
                                ? bb_model_->account_bookmark_bar_node()->id()
                                : bb_model_->bookmark_bar_node()->id());
    CHECK(b_node);
  }
  tree_view_->SetSelectedNode(b_node);
}

std::unique_ptr<BookmarkEditorView::EditorNode>
BookmarkEditorView::CreateRootNode() {
  std::unique_ptr<EditorNode> root_node = std::make_unique<EditorNode>(
      std::u16string(), EditorNodeData{EditorNodeData::Type::kRoot});
  const bookmarks::BookmarkNodesSplitByAccountAndLocal permanent_nodes =
      bookmarks::GetPermanentNodesForDisplay(bb_model_);
  if (!permanent_nodes.account_nodes.empty()) {
    auto add_nodes = [this](EditorNode* parent,
                            std::vector<const BookmarkNode*> nodes) {
      for (const BookmarkNode* node : nodes) {
        EditorNode* const new_b_node = parent->Add(std::make_unique<EditorNode>(
            node->GetTitle(),
            EditorNodeData{EditorNodeData::Type::kFolder, node->id()}));
        CreateNodes(node, new_b_node);
      }
    };

    if (permanent_nodes.local_nodes.empty()) {
      add_nodes(root_node.get(), permanent_nodes.account_nodes);
    } else {
      EditorNode* const account_nodes =
          root_node->Add(std::make_unique<EditorNode>(
              l10n_util::GetStringUTF16(IDS_BOOKMARKS_ACCOUNT_BOOKMARKS),
              EditorNodeData{EditorNodeData::Type::kTitle}));
      add_nodes(account_nodes, permanent_nodes.account_nodes);
      EditorNode* const local_nodes =
          root_node->Add(std::make_unique<EditorNode>(
              l10n_util::GetStringUTF16(IDS_BOOKMARKS_DEVICE_BOOKMARKS),
              EditorNodeData{EditorNodeData::Type::kTitle}));
      add_nodes(local_nodes, permanent_nodes.local_nodes);
    }
  } else {
    const BookmarkNode* bb_root_node = bb_model_->root_node();
    CreateNodes(bb_root_node, root_node.get());
    DCHECK_GE(root_node->children().size(), 2u);
    DCHECK_LE(root_node->children().size(), 4u);
    DCHECK_EQ(BookmarkNode::BOOKMARK_BAR, bb_root_node->children()[0]->type());
    DCHECK_EQ(BookmarkNode::OTHER_NODE, bb_root_node->children()[1]->type());
    if (root_node->children().size() >= 3) {
      DCHECK_EQ(BookmarkNode::MOBILE, bb_root_node->children()[2]->type());
    }
  }
  return root_node;
}

void BookmarkEditorView::CreateNodes(const BookmarkNode* bb_node,
                                     BookmarkEditorView::EditorNode* b_node) {
  for (const auto& child_bb_node : bb_node->children()) {
    if (bookmarks::PruneFoldersForDisplay(bb_model_, child_bb_node.get())) {
      continue;
    }
    EditorNode* const new_b_node = b_node->Add(std::make_unique<EditorNode>(
        child_bb_node->GetTitle(),
        EditorNodeData{EditorNodeData::Type::kFolder, child_bb_node->id()}));
    new_b_node->SetPlaceholderAccessibleTitle(
        l10n_util::GetStringUTF16(IDS_UNNAMED_BOOKMARK_FOLDER));
    CreateNodes(child_bb_node.get(), new_b_node);
  }
}

BookmarkEditorView::EditorNode* BookmarkEditorView::FindNodeWithID(
    BookmarkEditorView::EditorNode* node,
    int64_t id) {
  if (node->value.bookmark_node_id == id) {
    return node;
  }
  for (const auto& child : node->children()) {
    EditorNode* const result = FindNodeWithID(child.get(), id);
    if (result) {
      return result;
    }
  }
  return nullptr;
}

void BookmarkEditorView::ApplyEdits(EditorNode* parent) {
  DCHECK(bb_model_->loaded());

  if (!parent) {
    if (tree_view_) {
      tree_view_->CommitEdit();
    }

    if (show_tree_) {
      parent = tree_model_->AsNode(tree_view_->GetSelectedNode());
      DCHECK(parent);
    }
  }
  // We're going to apply edits to the bookmark bar model, which will call us
  // back. Normally when a structural edit occurs we reset the tree model.
  // We don't want to do that here, so we remove ourselves as an observer.
  bb_model_->RemoveObserver(this);

  GURL new_url(GetInputURL());
  std::u16string new_title(title_tf_ ? title_tf_->GetText() : u"");

  if (!show_tree_) {
    BookmarkEditor::ApplyEdits(bb_model_, details_.parent_node, details_,
                               new_title, new_url);
  } else {
    // Create the new folders and update the titles.
    const BookmarkNode* new_parent = nullptr;

    // Applying edits only works if we've selected a bookmark node. Accepting
    // the dialog should not be possible when a non-bookmark node is selected.
    CHECK(parent->value.type == EditorNodeData::Type::kFolder);

    ApplyNameChangesAndCreateNewFolders(
        bb_model_->root_node(), tree_model_->GetRoot(), parent, &new_parent);

    BookmarkEditor::ApplyEdits(bb_model_, new_parent, details_, new_title,
                               new_url);

    BookmarkExpandedStateTracker::Nodes expanded_nodes;
    UpdateExpandedNodes(tree_model_->GetRoot(), &expanded_nodes);
    expanded_state_tracker_->SetExpandedNodes(expanded_nodes);

    // Remove the folders that were removed. This has to be done after all the
    // other changes have been committed.
    bookmarks::DeleteBookmarkFolders(bb_model_, deletes_, FROM_HERE);
  }

  // Once all required bookmarks updates have been called, call the configured
  // callback.
  std::move(on_save_callback_).Run();
}

void BookmarkEditorView::ApplyNameChangesAndCreateNewFolders(
    const BookmarkNode* target_node,
    BookmarkEditorView::EditorNode* source_node,
    BookmarkEditorView::EditorNode* parent_source_node,
    const BookmarkNode** parent_target_node) {
  CHECK(target_node);
  if (parent_source_node == source_node) {
    *parent_target_node = target_node;
  }
  for (const auto& child_source_node : source_node->children()) {
    const BookmarkNode* child_target_node = nullptr;
    switch (child_source_node->value.type) {
      case EditorNodeData::Type::kFolder:
        if (child_source_node->value.bookmark_node_id == 0) {
          // New folder.
          child_target_node =
              bb_model_->AddFolder(target_node, target_node->children().size(),
                                   child_source_node->GetTitle());
          child_source_node->value.bookmark_node_id = child_target_node->id();
        } else {
          // Existing node, reset the title (BookmarkModel ignores changes if
          // the title is the same).
          auto i = std::ranges::find_if(
              target_node->children(), [&child_source_node](const auto& node) {
                return node->is_folder() &&
                       node->id() == child_source_node->value.bookmark_node_id;
              });
          CHECK(i != target_node->children().cend());
          child_target_node = i->get();
          bb_model_->SetTitle(child_target_node, child_source_node->GetTitle(),
                              bookmarks::metrics::BookmarkEditSource::kUser);
        }
        break;
      case EditorNodeData::Type::kTitle:
        // Editor nodes of type kTitle (e.g., "In your Google Account", "Only on
        // this device") do not have corresponding persistent nodes in the
        // BookmarkModel.
        //
        // However, the children of these kTitle editor nodes do correspond to
        // actual permanent BookmarkNodes residing directly under the
        // BookmarkModel's root node.
        //
        // BookmarkEditorView:
        //   Root
        //     +-- In your Google Account
        //     |     +-- Bookmark Bar
        //     |     +-- Other Bookmarks
        //     |     +-- etc.
        //     +-- Only on this device
        //           +-- Bookmark Bar
        //           +-- Other Bookmarks
        //           +-- etc.
        //
        // BookmarkModel:
        //   Root
        //     +-- Bookmark Bar (Account)
        //     +-- Other Bookmarks (Account)
        //     +-- Bookmark Bar (Local)
        //     +-- Other Bookmarks (Local)
        //     +-- etc.

        // Pass the model root as the recursive target for correct model
        // anchoring.
        child_target_node = bb_model_->root_node();
        break;
      case EditorNodeData::Type::kRoot:
        NOTREACHED();
    }
    ApplyNameChangesAndCreateNewFolders(child_target_node,
                                        child_source_node.get(),
                                        parent_source_node, parent_target_node);
  }
}

void BookmarkEditorView::UpdateExpandedNodes(
    EditorNode* editor_node,
    BookmarkExpandedStateTracker::Nodes* expanded_nodes) {
  if (!tree_view_->IsExpanded(editor_node)) {
    return;
  }

  // Only insert tree nodes that correspond to a bookmark node. This excludes
  // new folders that have not yet been added to the bookmark model.
  if (editor_node->value.bookmark_node_id != 0) {
    expanded_nodes->insert(bookmarks::GetBookmarkNodeByID(
        bb_model_, editor_node->value.bookmark_node_id));
  }

  for (const auto& child : editor_node->children()) {
    UpdateExpandedNodes(child.get(), expanded_nodes);
  }
}

ui::SimpleMenuModel* BookmarkEditorView::GetMenuModel() {
  if (!context_menu_model_.get()) {
    context_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
    context_menu_model_->AddItemWithStringId(kContextMenuItemEdit, IDS_EDIT);
    context_menu_model_->AddItemWithStringId(kContextMenuItemDelete,
                                             IDS_DELETE);
    context_menu_model_->AddItemWithStringId(
        kContextMenuItemNewFolder, IDS_BOOKMARK_EDITOR_NEW_FOLDER_MENU_ITEM);
  }
  return context_menu_model_.get();
}

void BookmarkEditorView::ExecuteCommandDelete(
    base::OnceCallback<bool(const bookmarks::BookmarkNode* node)>
        non_empty_folder_confirmation_cb) {
  EditorNode* node = tree_model_->AsNode(tree_view_->GetActiveNode());
  if (!node) {
    return;
  }
  // This should only be reachable for editable bookmark folders. See
  // IsCommandIdEnabled().
  CHECK(CanEdit(node));
  const int64_t bookmark_node_id = node->value.bookmark_node_id;
  if (bookmark_node_id != 0) {
    const BookmarkNode* b_node =
        bookmarks::GetBookmarkNodeByID(bb_model_, bookmark_node_id);
    if (!b_node->children().empty()) {
      if (!std::move(non_empty_folder_confirmation_cb).Run(b_node)) {
        // The folder is not empty and the user didn't confirm.
        return;
      }
      // The function above runs a nested loop so it's necessary to guard
      // against |node| having been deleted meanwhile (e.g. via extensions).
      node = tree_model_->AsNode(tree_view_->GetActiveNode());
      if (!node || node->value.bookmark_node_id != bookmark_node_id) {
        // The active node has been deleted or has changed. In theory
        // FindNodeWithID() could be used to look up by |bookmark_node_id|,
        // but it's hard to reason about the desired behavior in this case, so
        // let's err on the safe side and avoid a deletion.
        return;
      }
    }
    deletes_.push_back(bookmark_node_id);
  }
  tree_model_->Remove(node->parent(), node);
}

void BookmarkEditorView::EditorTreeModel::SetTitle(
    ui::TreeModelNode* node,
    const std::u16string& title) {
  if (!title.empty()) {
    ui::TreeNodeModel<EditorNode>::SetTitle(node, title);
  }
}

BEGIN_METADATA(BookmarkEditorView)
ADD_READONLY_PROPERTY_METADATA(GURL, InputURL)
END_METADATA
