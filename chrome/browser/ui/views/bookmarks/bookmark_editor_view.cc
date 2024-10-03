// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_editor_view.h"

#include <set>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_expanded_state_tracker_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
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
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_fixer.h"
#include "components/user_prefs/user_prefs.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
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
  if (tree_view_)
    tree_view_->SetModel(nullptr);
  bb_model_->RemoveObserver(this);
}

bool BookmarkEditorView::IsDialogButtonEnabled(
    ui::mojom::DialogButton button) const {
  if (button == ui::mojom::DialogButton::kOk) {
    if (!bb_model_->loaded())
      return false;

    if (details_.GetNodeType() != BookmarkNode::FOLDER)
      return GetInputURL().is_valid();
  }
  return true;
}

gfx::Size BookmarkEditorView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  if (!show_tree_)
    return views::View::CalculatePreferredSize(available_size);

  return gfx::Size(views::Widget::GetLocalizedContentsSize(
      IDS_EDITBOOKMARK_DIALOG_WIDTH_CHARS,
      IDS_EDITBOOKMARK_DIALOG_HEIGHT_LINES));
}

void BookmarkEditorView::OnTreeViewSelectionChanged(
    views::TreeView* tree_view) {
}

bool BookmarkEditorView::CanEdit(views::TreeView* tree_view,
                                 ui::TreeModelNode* node) {
  // Only allow editting of children of the bookmark bar node and other node.
  EditorNode* bb_node = tree_model_->AsNode(node);
  return (bb_node->parent() && bb_node->parent()->parent());
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
  switch (command_id) {
    case kContextMenuItemEdit:
    case kContextMenuItemDelete:
      return !running_menu_for_root_;
    case kContextMenuItemNewFolder:
      return true;
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
    ExecuteCommandDelete(base::BindOnce(&chrome::ConfirmDeleteBookmarkNode,
                                        GetWidget()->GetNativeWindow()));
  } else {
    DCHECK_EQ(kContextMenuItemNewFolder, command_id);
    NewFolder(tree_model_->AsNode(tree_view_->GetActiveNode()));
  }
}

void BookmarkEditorView::Show(gfx::NativeWindow parent) {
  constrained_window::CreateBrowserModalDialogViews(this, parent);
  UserInputChanged();
  if (show_tree_ && bb_model_->loaded())
    ExpandAndSelect();
  GetWidget()->Show();
  // Select all the text in the name Textfield.
  title_tf_->SelectAll(true);
  // Give focus to the name Textfield.
  title_tf_->RequestFocus();
}

void BookmarkEditorView::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  DCHECK_EQ(tree_view_, source);
  if (!tree_view_->GetActiveNode())
    return;
  running_menu_for_root_ =
      (tree_model_->GetParent(tree_view_->GetActiveNode()) ==
       tree_model_->GetRoot());

  context_menu_runner_ = std::make_unique<views::MenuRunner>(
      GetMenuModel(),
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU);

  context_menu_runner_->RunMenuAt(source->GetWidget()->GetTopLevelWidget(),
                                  nullptr, gfx::Rect(point, gfx::Size()),
                                  views::MenuAnchorPosition::kTopRight,
                                  source_type);
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
  if ((details_.type == EditDetails::EXISTING_NODE &&
       details_.existing_node->HasAncestor(node)) ||
      (details_.parent_node && details_.parent_node->HasAncestor(node))) {
    // The node, or its parent was removed. Close the dialog.
    GetWidget()->Close();
  } else {
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

  std::u16string title;
  GURL url;
  if (details_.type == EditDetails::EXISTING_NODE) {
    title = details_.existing_node->GetTitle();
    url = details_.existing_node->url();
  } else if (details_.type == EditDetails::NEW_FOLDER) {
    title = l10n_util::GetStringUTF16(IDS_BOOKMARK_EDITOR_NEW_FOLDER_NAME);
  } else if (details_.type == EditDetails::NEW_URL) {
    url = details_.bookmark_data.url.value();
    title = details_.bookmark_data.title;
  }

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, gfx::Insets(),
          provider->GetDistanceMetric(
              views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  auto* labels = AddChildView(std::make_unique<views::View>());
  auto* label_layout =
      labels->SetLayoutManager(std::make_unique<views::TableLayout>());
  label_layout
      ->AddColumn(views::LayoutAlignment::kStart,
                  views::LayoutAlignment::kCenter,
                  views::TableLayout::kFixedSize,
                  views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize,
                        provider->GetDistanceMetric(
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

  if (details_.GetNodeType() != BookmarkNode::FOLDER) {
    label_layout
        ->AddPaddingRow(views::TableLayout::kFixedSize,
                        provider->GetDistanceMetric(
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

  if (show_tree_) {
    auto tree_view = std::make_unique<views::TreeView>();
    tree_view->SetRootShown(false);
    tree_view->set_context_menu_controller(this);
    tree_view_ = tree_view.get();
    auto* scroll_view = AddChildView(
        views::TreeView::CreateScrollViewWithTree(std::move(tree_view)));
    layout->SetFlexForView(scroll_view, 1);
  }

  if (!show_tree_ || bb_model_->loaded())
    Reset();
}

void BookmarkEditorView::Reset() {
  if (!show_tree_) {
    if (parent())
      UserInputChanged();
    return;
  }

  if (new_folder_button_)
    new_folder_button_->SetEnabled(true);

  // Do this first, otherwise when we invoke SetModel with the real one
  // tree_view will try to invoke something on the model we just deleted.
  tree_view_->SetModel(nullptr);

  tree_model_ = std::make_unique<EditorTreeModel>(CreateRootNode());

  tree_view_->SetModel(tree_model_.get());
  tree_view_->SetController(this);

  context_menu_runner_.reset();

  if (parent())
    ExpandAndSelect();
}

GURL BookmarkEditorView::GetInputURL() const {
  if (details_.GetNodeType() == BookmarkNode::FOLDER)
    return GURL();
  return url_formatter::FixupURL(base::UTF16ToUTF8(url_tf_->GetText()),
                                 std::string());
}

void BookmarkEditorView::UserInputChanged() {
  if (details_.GetNodeType() != BookmarkNode::FOLDER) {
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
  auto new_folder_node = std::make_unique<EditorNode>(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_EDITOR_NEW_FOLDER_NAME), 0);
  new_folder_node->SetPlaceholderAccessibleTitle(
      l10n_util::GetStringUTF16(IDS_UNNAMED_BOOKMARK_FOLDER));
  return tree_model_->Add(parent, std::move(new_folder_node));
}

void BookmarkEditorView::ExpandAndSelect() {
  BookmarkExpandedStateTracker::Nodes expanded_nodes =
      expanded_state_tracker_->GetExpandedNodes();
  for (const BookmarkNode* node : expanded_nodes) {
    EditorNode* editor_node =
        FindNodeWithID(tree_model_->GetRoot(), node->id());
    if (editor_node)
      tree_view_->Expand(editor_node);
  }

  const BookmarkNode* to_select = details_.parent_node;
  if (details_.type == EditDetails::EXISTING_NODE)
    to_select = details_.existing_node->parent();
  int64_t folder_id_to_select = to_select->id();
  EditorNode* b_node =
      FindNodeWithID(tree_model_->GetRoot(), folder_id_to_select);
  if (!b_node)
    b_node = tree_model_->GetRoot()->children().front().get();  // Bookmark bar.

  tree_view_->SetSelectedNode(b_node);
}

std::unique_ptr<BookmarkEditorView::EditorNode>
BookmarkEditorView::CreateRootNode() {
  std::unique_ptr<EditorNode> root_node =
      std::make_unique<EditorNode>(std::u16string(), 0);
  const BookmarkNode* bb_root_node = bb_model_->root_node();
  if (bb_model_->account_bookmark_bar_node()) {
    // TODO(crbug.com/370844777): Create fake-node parents for account vs.
    // local. This is checked in in an intermittent state because the DCHECKs
    // below don't apply when there are "account" bookmarks. Creating fake-node
    // parents requires handling non-folder entries in the tree view which needs
    // to be supported first.
    CreateNodes(bb_root_node, root_node.get());
  } else {
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
    if (child_bb_node->IsVisible() && child_bb_node->is_folder() &&
        !bb_model_->client()->IsNodeManaged(child_bb_node.get())) {
      EditorNode* new_b_node = b_node->Add(std::make_unique<EditorNode>(
          child_bb_node->GetTitle(), child_bb_node->id()));
      new_b_node->SetPlaceholderAccessibleTitle(
          l10n_util::GetStringUTF16(IDS_UNNAMED_BOOKMARK_FOLDER));
      CreateNodes(child_bb_node.get(), new_b_node);
    }
  }
}

BookmarkEditorView::EditorNode* BookmarkEditorView::FindNodeWithID(
    BookmarkEditorView::EditorNode* node,
    int64_t id) {
  if (node->value == id)
    return node;
  for (const auto& child : node->children()) {
    EditorNode* result = FindNodeWithID(child.get(), id);
    if (result)
      return result;
  }
  return nullptr;
}

void BookmarkEditorView::ApplyEdits(EditorNode* parent) {
  DCHECK(bb_model_->loaded());

  if (!parent) {
    if (tree_view_)
      tree_view_->CommitEdit();

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
  std::u16string new_title(title_tf_->GetText());

  if (!show_tree_) {
    BookmarkEditor::ApplyEdits(bb_model_, details_.parent_node, details_,
                               new_title, new_url);
  } else {
    // Create the new folders and update the titles.
    const BookmarkNode* new_parent = nullptr;
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
    const BookmarkNode* bb_node,
    BookmarkEditorView::EditorNode* b_node,
    BookmarkEditorView::EditorNode* parent_b_node,
    const BookmarkNode** parent_bb_node) {
  if (parent_b_node == b_node)
    *parent_bb_node = bb_node;
  for (const auto& child_b_node : b_node->children()) {
    const BookmarkNode* child_bb_node = nullptr;
    if (child_b_node->value == 0) {
      // New folder.
      child_bb_node = bb_model_->AddFolder(bb_node, bb_node->children().size(),
                                           child_b_node->GetTitle());
      child_b_node->value = child_bb_node->id();
    } else {
      // Existing node, reset the title (BookmarkModel ignores changes if the
      // title is the same).
      const auto i = base::ranges::find_if(
          bb_node->children(), [&child_b_node](const auto& node) {
            return node->is_folder() && node->id() == child_b_node->value;
          });
      DCHECK(i != bb_node->children().cend());
      child_bb_node = i->get();
      bb_model_->SetTitle(child_bb_node, child_b_node->GetTitle(),
                          bookmarks::metrics::BookmarkEditSource::kUser);
    }
    ApplyNameChangesAndCreateNewFolders(child_bb_node, child_b_node.get(),
                                        parent_b_node, parent_bb_node);
  }
}

void BookmarkEditorView::UpdateExpandedNodes(
    EditorNode* editor_node,
    BookmarkExpandedStateTracker::Nodes* expanded_nodes) {
  if (!tree_view_->IsExpanded(editor_node))
    return;

  // The root is 0.
  if (editor_node->value != 0) {
    expanded_nodes->insert(
        bookmarks::GetBookmarkNodeByID(bb_model_, editor_node->value));
  }

  for (const auto& child : editor_node->children())
    UpdateExpandedNodes(child.get(), expanded_nodes);
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
  if (!node)
    return;
  const int64_t bookmark_node_id = node->value;
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
      if (!node || node->value != bookmark_node_id) {
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
  if (!title.empty())
    ui::TreeNodeModel<EditorNode>::SetTitle(node, title);
}

BEGIN_METADATA(BookmarkEditorView)
ADD_READONLY_PROPERTY_METADATA(GURL, InputURL)
END_METADATA
