// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_editor_view.h"

#include <set>
#include <string>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/history/core/browser/history_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_fixer.h"
#include "components/user_prefs/user_prefs.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/tree/tree_view.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

using bookmarks::BookmarkExpandedStateTracker;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace {

std::unique_ptr<views::LabelButton> CreateNewFolderButton(
    views::ButtonListener* listener,
    bool enabled) {
  auto new_folder_button = views::MdTextButton::CreateSecondaryUiButton(
      listener,
      l10n_util::GetStringUTF16(IDS_BOOKMARK_EDITOR_NEW_FOLDER_BUTTON));
  new_folder_button->SetEnabled(enabled);
  return new_folder_button;
}

}  // namespace

BookmarkEditorView::BookmarkEditorView(
    Profile* profile,
    const BookmarkNode* parent,
    const EditDetails& details,
    BookmarkEditor::Configuration configuration)
    : profile_(profile),
      parent_(parent),
      details_(details),
      bb_model_(BookmarkModelFactory::GetForBrowserContext(profile)),
      show_tree_(configuration == SHOW_TREE) {
  DCHECK(profile);
  DCHECK(bb_model_);
  DCHECK(bb_model_->client()->CanBeEditedByUser(parent));
  DialogDelegate::set_button_label(ui::DIALOG_BUTTON_OK,
                                   l10n_util::GetStringUTF16(IDS_SAVE));
  if (show_tree_) {
    new_folder_button_ = DialogDelegate::SetExtraView(
        CreateNewFolderButton(this, bb_model_->loaded()));
  }
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::CONTROL, views::CONTROL));
  Init();
  chrome::RecordDialogCreation(chrome::DialogIdentifier::BOOKMARK_EDITOR);
}

BookmarkEditorView::~BookmarkEditorView() {
  // The tree model is deleted before the view. Reset the model otherwise the
  // tree will reference a deleted model.
  if (tree_view_)
    tree_view_->SetModel(nullptr);
  bb_model_->RemoveObserver(this);
}

bool BookmarkEditorView::IsDialogButtonEnabled(ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK) {
    if (!bb_model_->loaded())
      return false;

    if (details_.GetNodeType() != BookmarkNode::FOLDER)
      return GetInputURL().is_valid();
  }
  return true;
}

ui::ModalType BookmarkEditorView::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

bool BookmarkEditorView::CanResize() const {
  return true;
}

bool BookmarkEditorView::ShouldShowCloseButton() const {
  return false;
}

base::string16 BookmarkEditorView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(details_.GetWindowTitleId());
}

bool BookmarkEditorView::Accept() {
  if (!IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK)) {
    if (details_.GetNodeType() != BookmarkNode::FOLDER) {
      // The url is invalid, focus the url field.
      url_tf_->SelectAll(true);
      url_tf_->RequestFocus();
    }
    return false;
  }
  // Otherwise save changes and close the dialog box.
  ApplyEdits();
  return true;
}

gfx::Size BookmarkEditorView::CalculatePreferredSize() const {
  if (!show_tree_)
    return views::View::CalculatePreferredSize();

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
                                         const base::string16& new_contents) {
  UserInputChanged();
}

bool BookmarkEditorView::HandleKeyEvent(views::Textfield* sender,
                                        const ui::KeyEvent& key_event) {
    return false;
}

void BookmarkEditorView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  views::DialogDelegateView::GetAccessibleNodeData(node_data);
  node_data->SetName(l10n_util::GetStringUTF8(IDS_BOOKMARK_EDITOR_TITLE));
}

void BookmarkEditorView::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  DCHECK_EQ(new_folder_button_, sender);
  NewFolder();
}

bool BookmarkEditorView::IsCommandIdChecked(int command_id) const {
  return false;
}

bool BookmarkEditorView::IsCommandIdEnabled(int command_id) const {
  switch (command_id) {
    case IDS_EDIT:
    case IDS_DELETE:
      return !running_menu_for_root_;
    case IDS_BOOKMARK_EDITOR_NEW_FOLDER_MENU_ITEM:
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

bool BookmarkEditorView::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  return GetWidget()->GetAccelerator(command_id, accelerator);
}

void BookmarkEditorView::ExecuteCommand(int command_id, int event_flags) {
  DCHECK(tree_view_->GetSelectedNode());
  if (command_id == IDS_EDIT) {
    tree_view_->StartEditing(tree_view_->GetSelectedNode());
  } else if (command_id == IDS_DELETE) {
    EditorNode* node = tree_model_->AsNode(tree_view_->GetSelectedNode());
    if (!node)
      return;
    if (node->value != 0) {
      const BookmarkNode* b_node =
          bookmarks::GetBookmarkNodeByID(bb_model_, node->value);
      if (!b_node->children().empty() &&
          !chrome::ConfirmDeleteBookmarkNode(b_node,
                                             GetWidget()->GetNativeWindow())) {
        // The folder is not empty and the user didn't confirm.
        return;
      }
      deletes_.push_back(node->value);
    }
    tree_model_->Remove(node->parent(), node);
  } else {
    DCHECK_EQ(IDS_BOOKMARK_EDITOR_NEW_FOLDER_MENU_ITEM, command_id);
    NewFolder();
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
  if (!tree_view_->GetSelectedNode())
    return;
  running_menu_for_root_ =
      (tree_model_->GetParent(tree_view_->GetSelectedNode()) ==
       tree_model_->GetRoot());

  context_menu_runner_ = std::make_unique<views::MenuRunner>(
      GetMenuModel(),
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU);

  context_menu_runner_->RunMenuAt(source->GetWidget()->GetTopLevelWidget(),
                                  nullptr, gfx::Rect(point, gfx::Size()),
                                  views::MenuAnchorPosition::kTopRight,
                                  source_type);
}

const char* BookmarkEditorView::GetClassName() const {
  return "BookmarkEditorView";
}

void BookmarkEditorView::BookmarkNodeMoved(BookmarkModel* model,
                                           const BookmarkNode* old_parent,
                                           size_t old_index,
                                           const BookmarkNode* new_parent,
                                           size_t new_index) {
  Reset();
}

void BookmarkEditorView::BookmarkNodeAdded(BookmarkModel* model,
                                           const BookmarkNode* parent,
                                           size_t index) {
  Reset();
}

void BookmarkEditorView::BookmarkNodeRemoved(
    BookmarkModel* model,
    const BookmarkNode* parent,
    size_t index,
    const BookmarkNode* node,
    const std::set<GURL>& removed_urls) {
  if ((details_.type == EditDetails::EXISTING_NODE &&
       details_.existing_node->HasAncestor(node)) ||
      (parent_ && parent_->HasAncestor(node))) {
    // The node, or its parent was removed. Close the dialog.
    GetWidget()->Close();
  } else {
    Reset();
  }
}

void BookmarkEditorView::BookmarkAllUserNodesRemoved(
    BookmarkModel* model,
    const std::set<GURL>& removed_urls) {
  Reset();
}

void BookmarkEditorView::BookmarkNodeChildrenReordered(
    BookmarkModel* model,
    const BookmarkNode* node) {
  Reset();
}

void BookmarkEditorView::Init() {
  bb_model_->AddObserver(this);

  base::string16 title;
  GURL url;
  if (details_.type == EditDetails::EXISTING_NODE) {
    title = details_.existing_node->GetTitle();
    url = details_.existing_node->url();
  } else if (details_.type == EditDetails::NEW_FOLDER) {
    title = l10n_util::GetStringUTF16(IDS_BOOKMARK_EDITOR_NEW_FOLDER_NAME);
  } else if (details_.type == EditDetails::NEW_URL) {
    url = details_.url;
    title = details_.title;
  }
  auto title_tf = std::make_unique<views::Textfield>();
  title_tf->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_AX_EDITOR_NAME_LABEL));
  title_tf->SetText(title);
  title_tf->set_controller(this);

  std::unique_ptr<views::TreeView> tree_view;
  if (show_tree_) {
    tree_view = std::make_unique<views::TreeView>();
    tree_view->SetRootShown(false);
    tree_view->set_context_menu_controller(this);
  }

  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  const int labels_column_set_id = 0;
  const int single_column_view_set_id = 1;
  const int buttons_column_set_id = 2;

  views::ColumnSet* column_set = layout->AddColumnSet(labels_column_set_id);
  column_set->AddColumn(
      provider->GetControlLabelGridAlignment(), views::GridLayout::CENTER,
      views::GridLayout::kFixedSize, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(
      views::GridLayout::kFixedSize,
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_HORIZONTAL));
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 1.0,
                        views::GridLayout::USE_PREF, 0, 0);

  column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1.0,
                        views::GridLayout::USE_PREF, 0, 0);

  column_set = layout->AddColumnSet(buttons_column_set_id);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::LEADING,
                        views::GridLayout::kFixedSize,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(
      1.0,
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_HORIZONTAL));
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::LEADING,
                        views::GridLayout::kFixedSize,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(
      views::GridLayout::kFixedSize,
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_HORIZONTAL));
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::LEADING,
                        views::GridLayout::kFixedSize,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->LinkColumnSizes({0, 2, 4});

  layout->StartRow(views::GridLayout::kFixedSize, labels_column_set_id);
  layout->AddView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_EDITOR_NAME_LABEL)));
  title_tf_ = layout->AddView(std::move(title_tf));

  if (details_.GetNodeType() != BookmarkNode::FOLDER) {
    auto url_tf = std::make_unique<views::Textfield>();
    url_tf->SetText(chrome::FormatBookmarkURLForDisplay(url));
    url_tf->set_controller(this);
    url_tf->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_BOOKMARK_AX_EDITOR_URL_LABEL));
    url_tf->SetTextInputType(ui::TextInputType::TEXT_INPUT_TYPE_URL);

    layout->AddPaddingRow(
        views::GridLayout::kFixedSize,
        provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL));

    layout->StartRow(views::GridLayout::kFixedSize, labels_column_set_id);
    layout->AddView(std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(IDS_BOOKMARK_EDITOR_URL_LABEL)));
    url_tf_ = layout->AddView(std::move(url_tf));
  }

  if (show_tree_) {
    layout->AddPaddingRow(
        views::GridLayout::kFixedSize,
        provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL));
    layout->StartRow(1.0, single_column_view_set_id);
    tree_view_ = tree_view.get();
    layout->AddView(
        views::TreeView::CreateScrollViewWithTree(std::move(tree_view)));
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

void BookmarkEditorView::NewFolder() {
  // Create a new entry parented to the selected item, or the bookmark
  // bar if nothing is selected.
  EditorNode* parent = tree_model_->AsNode(tree_view_->GetSelectedNode());
  if (!parent) {
    NOTREACHED();
    return;
  }

  tree_view_->StartEditing(AddNewFolder(parent));
}

BookmarkEditorView::EditorNode* BookmarkEditorView::AddNewFolder(
    EditorNode* parent) {
  return tree_model_->Add(
      parent,
      std::make_unique<EditorNode>(
          l10n_util::GetStringUTF16(IDS_BOOKMARK_EDITOR_NEW_FOLDER_NAME), 0));
}

void BookmarkEditorView::ExpandAndSelect() {
  BookmarkExpandedStateTracker::Nodes expanded_nodes =
      bb_model_->expanded_state_tracker()->GetExpandedNodes();
  for (auto i(expanded_nodes.begin()); i != expanded_nodes.end(); ++i) {
    EditorNode* editor_node =
        FindNodeWithID(tree_model_->GetRoot(), (*i)->id());
    if (editor_node)
      tree_view_->Expand(editor_node);
  }

  const BookmarkNode* to_select = parent_;
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
      std::make_unique<EditorNode>(base::string16(), 0);
  const BookmarkNode* bb_root_node = bb_model_->root_node();
  CreateNodes(bb_root_node, root_node.get());
  DCHECK_GE(root_node->children().size(), 2u);
  DCHECK_LE(root_node->children().size(), 4u);
  DCHECK_EQ(BookmarkNode::BOOKMARK_BAR, bb_root_node->children()[0]->type());
  DCHECK_EQ(BookmarkNode::OTHER_NODE, bb_root_node->children()[1]->type());
  if (root_node->children().size() >= 3)
    DCHECK_EQ(BookmarkNode::MOBILE, bb_root_node->children()[2]->type());
  return root_node;
}

void BookmarkEditorView::CreateNodes(const BookmarkNode* bb_node,
                                     BookmarkEditorView::EditorNode* b_node) {
  for (const auto& child_bb_node : bb_node->children()) {
    if (child_bb_node->IsVisible() && child_bb_node->is_folder() &&
        bb_model_->client()->CanBeEditedByUser(child_bb_node.get())) {
      EditorNode* new_b_node = b_node->Add(std::make_unique<EditorNode>(
          child_bb_node->GetTitle(), child_bb_node->id()));
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

void BookmarkEditorView::ApplyEdits() {
  DCHECK(bb_model_->loaded());

  if (tree_view_)
    tree_view_->CommitEdit();

  EditorNode* parent =
      show_tree_ ? tree_model_->AsNode(tree_view_->GetSelectedNode()) : nullptr;
  if (show_tree_ && !parent) {
    NOTREACHED();
    return;
  }
  ApplyEdits(parent);
}

void BookmarkEditorView::ApplyEdits(EditorNode* parent) {
  DCHECK(!show_tree_ || parent);

  // We're going to apply edits to the bookmark bar model, which will call us
  // back. Normally when a structural edit occurs we reset the tree model.
  // We don't want to do that here, so we remove ourselves as an observer.
  bb_model_->RemoveObserver(this);

  GURL new_url(GetInputURL());
  base::string16 new_title(title_tf_->GetText());

  if (!show_tree_) {
    BookmarkEditor::ApplyEditsWithNoFolderChange(
        bb_model_, parent_, details_, new_title, new_url);
    return;
  }

  // Create the new folders and update the titles.
  const BookmarkNode* new_parent = nullptr;
  ApplyNameChangesAndCreateNewFolders(
      bb_model_->root_node(), tree_model_->GetRoot(), parent, &new_parent);

  BookmarkEditor::ApplyEditsWithPossibleFolderChange(
      bb_model_, new_parent, details_, new_title, new_url);

  BookmarkExpandedStateTracker::Nodes expanded_nodes;
  UpdateExpandedNodes(tree_model_->GetRoot(), &expanded_nodes);
  bb_model_->expanded_state_tracker()->SetExpandedNodes(expanded_nodes);

  // Remove the folders that were removed. This has to be done after all the
  // other changes have been committed.
  bookmarks::DeleteBookmarkFolders(bb_model_, deletes_);
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
      const auto i = std::find_if(
          bb_node->children().cbegin(), bb_node->children().cend(),
          [&child_b_node](const auto& node) {
            return node->is_folder() && node->id() == child_b_node->value;
          });
      DCHECK(i != bb_node->children().cend());
      child_bb_node = i->get();
      bb_model_->SetTitle(child_bb_node, child_b_node->GetTitle());
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
    context_menu_model_->AddItemWithStringId(IDS_EDIT, IDS_EDIT);
    context_menu_model_->AddItemWithStringId(IDS_DELETE, IDS_DELETE);
    context_menu_model_->AddItemWithStringId(
        IDS_BOOKMARK_EDITOR_NEW_FOLDER_MENU_ITEM,
        IDS_BOOKMARK_EDITOR_NEW_FOLDER_MENU_ITEM);
  }
  return context_menu_model_.get();
}

void BookmarkEditorView::EditorTreeModel::SetTitle(
    ui::TreeModelNode* node,
    const base::string16& title) {
  if (!title.empty())
    ui::TreeNodeModel<EditorNode>::SetTitle(node, title);
}
