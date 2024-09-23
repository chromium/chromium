// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_menu_delegate.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/bookmarks/bookmark_drag_drop.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/event_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/prefs/pref_service.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/page_navigator.h"
#include "ui/base/accelerators/menu_label_accelerator_util.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/color/color_id.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"

using base::UserMetricsAction;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using bookmarks::BookmarkNodeData;
using content::PageNavigator;
using views::MenuItemView;

namespace {

// Max width of a menu. There does not appear to be an OS value for this, yet
// both IE and FF restrict the max width of a menu.
const int kMaxMenuWidth = 400;

ui::ImageModel GetFaviconForNode(BookmarkModel* model,
                                 const BookmarkNode* node) {
  const gfx::Image& image = model->GetFavicon(node);
  return image.IsEmpty() ? favicon::GetDefaultFaviconModel()
                         : ui::ImageModel::FromImage(image);
}

// The current behavior is that the menu gets closed (see MenuController) after
// a drop is initiated, which deletes BookmarkMenuDelegate before the drop
// callback is run. That's why the drop callback shouldn't be tied to
// BookmarkMenuDelegate and needs a separate class.
class BookmarkModelDropObserver : public bookmarks::BaseBookmarkModelObserver {
 public:
  BookmarkModelDropObserver(Profile* profile,
                            const bookmarks::BookmarkNodeData drop_data,
                            const bookmarks::BookmarkNode* drop_parent,
                            const size_t index_to_drop_at)
      : profile_(profile),
        drop_data_(std::move(drop_data)),
        drop_parent_(drop_parent),
        index_to_drop_at_(index_to_drop_at),
        bookmark_model_(BookmarkModelFactory::GetForBrowserContext(profile)) {
    DCHECK(drop_data_.is_valid());
    DCHECK(drop_parent_);

    DCHECK(bookmark_model_);
    bookmark_model_observation_.Observe(bookmark_model_);
  }

  BookmarkModelDropObserver(const BookmarkModelDropObserver&) = delete;
  void operator=(const BookmarkModelDropObserver&) = delete;

  ~BookmarkModelDropObserver() override { CleanUp(); }

  void Drop(const ui::DropTargetEvent& event,
            ui::mojom::DragOperation& output_drag_op) {
    if (!bookmark_model_)  // Don't drop
      return;

    bool copy = event.source_operations() == ui::DragDropTypes::DRAG_COPY;
    output_drag_op = chrome::DropBookmarks(
        profile_, drop_data_, drop_parent_, index_to_drop_at_, copy,
        chrome::BookmarkReorderDropTarget::kBookmarkMenu);
  }

 private:
  // bookmarks::BaseBookmarkModelObserver:
  void BookmarkModelChanged() override { CleanUp(); }
  void BookmarkModelBeingDeleted() override { CleanUp(); }

  void CleanUp() {
    bookmark_model_observation_.Reset();
    bookmark_model_ = nullptr;
  }

  raw_ptr<Profile> profile_;
  const bookmarks::BookmarkNodeData drop_data_;
  raw_ptr<const bookmarks::BookmarkNode> drop_parent_;
  const size_t index_to_drop_at_;
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  base::ScopedObservation<BookmarkModel, BaseBookmarkModelObserver>
      bookmark_model_observation_{this};
};

}  // namespace

BookmarkMenuDelegate::BookmarkMenuDelegate(Browser* browser,
                                           views::Widget* parent)
    : browser_(browser),
      profile_(browser->profile()),
      parent_(parent),
      menu_(nullptr),
      parent_menu_item_(nullptr),
      next_menu_id_(AppMenuModel::kMinBookmarksCommandId),
      real_delegate_(nullptr),
      is_mutating_model_(false),
      location_(BookmarkLaunchLocation::kNone) {}

BookmarkMenuDelegate::~BookmarkMenuDelegate() {
  bookmark_model_observation_.Reset();
}

void BookmarkMenuDelegate::Init(views::MenuDelegate* real_delegate,
                                MenuItemView* parent,
                                const BookmarkNode* node,
                                size_t start_child_index,
                                ShowOptions show_options,
                                BookmarkLaunchLocation location) {
  bookmark_model_observation_.Observe(GetBookmarkModel());
  real_delegate_ = real_delegate;
  location_ = location;
  // Assume that the menu will only use mnemonics if there's already a parent
  // menu and that parent uses them. In cases where the BookmarkMenuDelegate
  // will be the root, client code does not current enable mnemonics.
  menu_uses_mnemonics_ = parent && parent->GetRootMenuItem()->has_mnemonics();
  if (parent) {
    parent_menu_item_ = parent;

    // Add a separator if there are existing items in the menu, and if the
    // current node has children. If |node| is the bookmark bar then the
    // managed node is shown as its first child, if it's not empty.
    BookmarkModel* model = GetBookmarkModel();
    bookmarks::ManagedBookmarkService* managed = GetManagedBookmarkService();
    bool show_forced_folders = show_options == SHOW_PERMANENT_FOLDERS &&
                               node == model->bookmark_bar_node();
    bool show_managed =
        show_forced_folders && !managed->managed_node()->children().empty();
    bool has_children =
        (start_child_index < node->children().size()) || show_managed;
    if (has_children && parent->GetSubmenu() &&
        !parent->GetSubmenu()->GetMenuItems().empty()) {
      parent->AppendSeparator();
      // Add a "Bookmarks" title.
      parent->AppendTitle(l10n_util::GetStringUTF16(IDS_BOOKMARKS_LIST_TITLE));
    }

    if (show_managed)
      BuildMenuForManagedNode(parent);
    BuildMenu(node, start_child_index, parent);
    if (show_options == SHOW_PERMANENT_FOLDERS)
      BuildMenusForPermanentNodes(parent);
  } else {
    menu_ = CreateMenu(node, start_child_index, show_options);
  }
}

const BookmarkModel* BookmarkMenuDelegate::GetBookmarkModel() const {
  return BookmarkModelFactory::GetForBrowserContext(profile_);
}

bookmarks::ManagedBookmarkService*
BookmarkMenuDelegate::GetManagedBookmarkService() {
  return ManagedBookmarkServiceFactory::GetForProfile(profile_);
}

void BookmarkMenuDelegate::SetActiveMenu(const BookmarkNode* node,
                                         size_t start_index) {
  DCHECK(!parent_menu_item_);
  if (!node_to_menu_map_[node])
    CreateMenu(node, start_index, HIDE_PERMANENT_FOLDERS);
  menu_ = node_to_menu_map_[node];
}

std::u16string BookmarkMenuDelegate::GetTooltipText(
    int id,
    const gfx::Point& screen_loc) const {
  auto i = menu_id_to_node_map_.find(id);
  // Ignore queries about unknown items, e.g. the empty menu item.
  if (i != menu_id_to_node_map_.end()) {
    const BookmarkNode* node = i->second;
    if (node->is_url()) {
      const views::TooltipManager* tooltip_manager =
          parent_->GetTooltipManager();
      return BookmarkBarView::CreateToolTipForURLAndTitle(
          tooltip_manager->GetMaxWidth(screen_loc),
          tooltip_manager->GetFontList(), node->url(), node->GetTitle());
    }
  }
  return std::u16string();
}

bool BookmarkMenuDelegate::IsTriggerableEvent(views::MenuItemView* menu,
                                              const ui::Event& e) {
  return e.type() == ui::EventType::kGestureTap ||
         e.type() == ui::EventType::kGestureTapDown ||
         event_utils::IsPossibleDispositionEvent(e);
}

void BookmarkMenuDelegate::ExecuteCommand(int id, int mouse_event_flags) {
  if (id == IDC_SHOW_BOOKMARK_SIDE_PANEL) {
    browser_->command_controller()->ExecuteCommand(id);
    return;
  }

  DCHECK(menu_id_to_node_map_.find(id) != menu_id_to_node_map_.end());

  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> selection = {
      menu_id_to_node_map_[id]};

  RecordBookmarkLaunch(location_,
                       profile_metrics::GetBrowserProfileType(profile_));
  chrome::OpenAllIfAllowed(browser_, selection,
                           ui::DispositionFromEventFlags(mouse_event_flags),
                           false);
}

bool BookmarkMenuDelegate::ShouldExecuteCommandWithoutClosingMenu(
    int id,
    const ui::Event& event) {
  if (!event.IsMouseEvent()) {
    // Restore pre https://crrev.com/c/3820263 behavior, which started calling
    // `ShouldExecuteCommandWithoutClosingMenu` for gesture events and caused
    // https://crbug.com/1498716 regression.
    // Gesture events will be handled via `MenuController::Accept()` -> ... ->
    // `BookmarkMenuDelegate::ExecuteCommand()` instead (as it was before).
    return false;
  }
  if (id == IDC_SHOW_BOOKMARK_SIDE_PANEL) {
    return false;
  }
  if (ui::DispositionFromEventFlags(event.flags()) ==
      WindowOpenDisposition::NEW_BACKGROUND_TAB) {
    DCHECK(menu_id_to_node_map_.find(id) != menu_id_to_node_map_.end());
    const BookmarkNode* node = menu_id_to_node_map_[id];
    // Close the menu before opening a folder since this may pop up a dialog
    // over the menu. See https://crbug.com/1105587 for details.
    return node->type() != BookmarkNode::FOLDER;
  }
  return false;
}

bool BookmarkMenuDelegate::GetDropFormats(
    MenuItemView* menu,
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  *formats = ui::OSExchangeData::URL;
  format_types->insert(BookmarkNodeData::GetBookmarkFormatType());
  return true;
}

bool BookmarkMenuDelegate::AreDropTypesRequired(MenuItemView* menu) {
  return true;
}

bool BookmarkMenuDelegate::CanDrop(MenuItemView* menu,
                                   const ui::OSExchangeData& data) {
  if (menu->GetCommand() == IDC_SHOW_BOOKMARK_SIDE_PANEL) {
    return false;
  }

  // Only accept drops of 1 node, which is the case for all data dragged from
  // bookmark bar and menus.
  if (!drop_data_.Read(data) || drop_data_.size() != 1 ||
      !profile_->GetPrefs()->GetBoolean(
          bookmarks::prefs::kEditBookmarksEnabled))
    return false;

  if (drop_data_.has_single_url())
    return true;

  const BookmarkNode* drag_node =
      drop_data_.GetFirstNode(GetBookmarkModel(), profile_->GetPath());
  if (!drag_node) {
    // Dragging a folder from another profile, always accept.
    return true;
  }

  // Drag originated from same profile and is not a URL. Only accept it if
  // the dragged node is not a parent of the node menu represents.
  if (menu_id_to_node_map_.find(menu->GetCommand()) ==
      menu_id_to_node_map_.end()) {
    // If we don't know the menu assume its because we're embedded. We'll
    // figure out the real operation when GetDropOperation is invoked.
    return true;
  }
  const BookmarkNode* drop_node = menu_id_to_node_map_[menu->GetCommand()];
  DCHECK(drop_node);
  while (drop_node && drop_node != drag_node)
    drop_node = drop_node->parent();
  return (drop_node == nullptr);
}

ui::mojom::DragOperation BookmarkMenuDelegate::GetDropOperation(
    MenuItemView* item,
    const ui::DropTargetEvent& event,
    views::MenuDelegate::DropPosition* position) {
  // Should only get here if we have drop data.
  DCHECK(drop_data_.is_valid());
  BookmarkModel* const model = GetBookmarkModel();

  if (item->GetCommand() == IDC_SHOW_BOOKMARK_SIDE_PANEL) {
    return ui::mojom::DragOperation::kNone;
  }

  const BookmarkNode* node = menu_id_to_node_map_[item->GetCommand()];
  const BookmarkNode* drop_parent = node->parent();
  size_t index_to_drop_at = drop_parent->GetIndexOf(node).value();
  switch (*position) {
    case views::MenuDelegate::DropPosition::kAfter:
      if (node == model->other_node() || node == model->mobile_node()) {
        // Dropping after these nodes makes no sense.
        *position = views::MenuDelegate::DropPosition::kNone;
      }
      index_to_drop_at++;
      break;

    case views::MenuDelegate::DropPosition::kBefore:
      if (node == model->mobile_node()) {
        // Dropping before this node makes no sense.
        *position = views::MenuDelegate::DropPosition::kNone;
      }
      break;

    case views::MenuDelegate::DropPosition::kOn:
      drop_parent = node;
      index_to_drop_at = node->children().size();
      break;

    default:
      break;
  }
  DCHECK(drop_parent);
  return chrome::GetBookmarkDropOperation(
      profile_, event, drop_data_, drop_parent, index_to_drop_at);
}

views::View::DropCallback BookmarkMenuDelegate::GetDropCallback(
    views::MenuItemView* menu,
    views::MenuDelegate::DropPosition position,
    const ui::DropTargetEvent& event) {
  const BookmarkNode* drop_node = menu_id_to_node_map_[menu->GetCommand()];
  DCHECK(drop_node);
  BookmarkModel* model = GetBookmarkModel();
  DCHECK(model);
  const BookmarkNode* drop_parent = drop_node->parent();
  DCHECK(drop_parent);
  size_t index_to_drop_at = drop_parent->GetIndexOf(drop_node).value();
  switch (position) {
    case views::MenuDelegate::DropPosition::kAfter:
      index_to_drop_at++;
      break;

    case views::MenuDelegate::DropPosition::kOn:
      DCHECK(drop_node->is_folder());
      drop_parent = drop_node;
      index_to_drop_at = drop_node->children().size();
      break;

    case views::MenuDelegate::DropPosition::kBefore:
      if (drop_node == model->other_node() ||
          drop_node == model->mobile_node()) {
        // This can happen with SHOW_PERMANENT_FOLDERS.
        drop_parent = model->bookmark_bar_node();
        index_to_drop_at = drop_parent->children().size();
      }
      break;

    default:
      break;
  }

  std::unique_ptr<BookmarkModelDropObserver> drop_observer =
      std::make_unique<BookmarkModelDropObserver>(
          profile_, std::move(drop_data_), drop_parent, index_to_drop_at);
  return base::BindOnce(
      [](BookmarkModelDropObserver* drop_observer,
         const ui::DropTargetEvent& event,
         ui::mojom::DragOperation& output_drag_op,
         std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner) {
        drop_observer->Drop(event, output_drag_op);
      },
      base::Owned(std::move(drop_observer)));
}

bool BookmarkMenuDelegate::ShowContextMenu(MenuItemView* source,
                                           int id,
                                           const gfx::Point& p,
                                           ui::MenuSourceType source_type) {
  // The IDC_SHOW_BOOKMARK_SIDE_PANEL menu item does not map to a bookmark node
  // and therefore no context menu for it should be shown.
  if (menu_id_to_node_map_.find(id) == menu_id_to_node_map_.end()) {
    return false;
  }
  const BookmarkNode* node = menu_id_to_node_map_[id];
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes(1, node);
  context_menu_ = std::make_unique<BookmarkContextMenu>(
      parent_, browser_, profile_, location_, node->parent(), nodes,
      ShouldCloseOnRemove(node));
  context_menu_->set_observer(this);
  context_menu_->RunMenuAt(p, source_type);
  return true;
}

bool BookmarkMenuDelegate::CanDrag(MenuItemView* menu) {
  if (menu->GetCommand() == IDC_SHOW_BOOKMARK_SIDE_PANEL) {
    return false;
  }
  const BookmarkNode* node = menu_id_to_node_map_[menu->GetCommand()];
  // Don't let users drag the other folder.
  return node->parent() != GetBookmarkModel()->root_node();
}

void BookmarkMenuDelegate::WriteDragData(MenuItemView* sender,
                                         ui::OSExchangeData* data) {
  DCHECK(sender && data);

  base::RecordAction(UserMetricsAction("BookmarkBar_DragFromFolder"));

  BookmarkNodeData drag_data(menu_id_to_node_map_[sender->GetCommand()]);
  drag_data.Write(profile_->GetPath(), data);
}

int BookmarkMenuDelegate::GetDragOperations(MenuItemView* sender) {
  return chrome::GetBookmarkDragOperation(
      profile_, menu_id_to_node_map_[sender->GetCommand()]);
}

int BookmarkMenuDelegate::GetMaxWidthForMenu(MenuItemView* menu) {
  return kMaxMenuWidth;
}

void BookmarkMenuDelegate::WillShowMenu(MenuItemView* menu) {
  auto iter = menu_id_to_node_map_.find(menu->GetCommand());
  if ((iter != menu_id_to_node_map_.end()) &&
      !iter->second->children().empty() &&
      menu->GetSubmenu()->GetMenuItems().empty())
    BuildMenu(iter->second, 0, menu);
}

void BookmarkMenuDelegate::BookmarkModelChanged() {}

void BookmarkMenuDelegate::BookmarkNodeFaviconChanged(
    const BookmarkNode* node) {
  auto menu_pair = node_to_menu_map_.find(node);
  if (menu_pair == node_to_menu_map_.end())
    return;  // We're not showing a menu item for the node.

  menu_pair->second->SetIcon(
      GetFaviconForNode(bookmark_model_observation_.GetSource(), node));
}

void BookmarkMenuDelegate::WillRemoveBookmarks(
    const std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>&
        bookmarks) {
  DCHECK(!is_mutating_model_);
  is_mutating_model_ = true;  // Set to false in DidRemoveBookmarks().

  // Remove the observer so that when the remove happens we don't prematurely
  // cancel the menu. The observer is added back in DidRemoveBookmarks().
  bookmark_model_observation_.Reset();

  // Remove the menu items.
  std::set<MenuItemView*> changed_parent_menus;
  for (const BookmarkNode* bookmark : bookmarks) {
    auto node_to_menu = node_to_menu_map_.find(bookmark);
    if (node_to_menu != node_to_menu_map_.end()) {
      MenuItemView* menu = node_to_menu->second;
      MenuItemView* parent = menu->GetParentMenuItem();
      // |parent| is NULL when removing a root. This happens when right clicking
      // to delete an empty folder.
      if (parent) {
        changed_parent_menus.insert(parent);
        parent->RemoveMenuItem(menu);
      }
      node_to_menu_map_.erase(node_to_menu);
      menu_id_to_node_map_.erase(menu->GetCommand());
    }
  }

  // All the bookmarks in |bookmarks| should have the same parent. It's possible
  // to support different parents, but this would need to prune any nodes whose
  // parent has been removed. As all nodes currently have the same parent, there
  // is the DCHECK.
  DCHECK_LE(changed_parent_menus.size(), 1U);

  // Remove any descendants of the removed nodes in |node_to_menu_map_|.
  for (auto i(node_to_menu_map_.begin()); i != node_to_menu_map_.end();) {
    bool ancestor_removed = false;
    for (const BookmarkNode* bookmark : bookmarks) {
      if (i->first->HasAncestor(bookmark)) {
        ancestor_removed = true;
        break;
      }
    }
    if (ancestor_removed) {
      menu_id_to_node_map_.erase(i->second->GetCommand());
      node_to_menu_map_.erase(i++);
    } else {
      ++i;
    }
  }

  for (MenuItemView* changed_parent_menu : changed_parent_menus)
    changed_parent_menu->ChildrenChanged();
}

void BookmarkMenuDelegate::DidRemoveBookmarks() {
  // Balances remove in WillRemoveBookmarksImpl.
  bookmark_model_observation_.Observe(GetBookmarkModel());
  DCHECK(is_mutating_model_);
  is_mutating_model_ = false;
}

void BookmarkMenuDelegate::OnContextMenuClosed() {
  context_menu_.reset();
}

bool BookmarkMenuDelegate::ShouldCloseOnRemove(const BookmarkNode* node) const {
  // We never need to close when embedded in the app menu.
  const bool is_shown_from_app_menu = parent_menu_item_ != nullptr;
  if (is_shown_from_app_menu)
    return false;

  const bool is_only_child_of_other_folder =
      node->parent() == GetBookmarkModel()->other_node() &&
      node->parent()->children().size() == 1;
  const bool is_child_of_bookmark_bar =
      node->parent() == GetBookmarkModel()->bookmark_bar_node();
  // The 'other' bookmarks folder hides when it has no more items, so we need
  // to exit the menu when the last node is removed.
  // If the parent is the bookmark bar, then the menu is showing for an item on
  // the bookmark bar. When removing this item we need to close the menu (as
  // there is no longer anything to anchor the menu to).
  return is_only_child_of_other_folder || is_child_of_bookmark_bar;
}

MenuItemView* BookmarkMenuDelegate::CreateMenu(const BookmarkNode* parent,
                                               size_t start_child_index,
                                               ShowOptions show_options) {
  MenuItemView* menu = new MenuItemView(real_delegate_);
  menu->SetCommand(GetAndIncrementNextMenuID());
  AddMenuToMaps(menu, parent);
  bool show_permanent = show_options == SHOW_PERMANENT_FOLDERS;
  if (show_permanent && parent == GetBookmarkModel()->bookmark_bar_node())
    BuildMenuForManagedNode(menu);
  BuildMenu(parent, start_child_index, menu);
  if (show_permanent)
    BuildMenusForPermanentNodes(menu);
  return menu;
}

void BookmarkMenuDelegate::BuildMenusForPermanentNodes(
    views::MenuItemView* menu) {
  BookmarkModel* model = GetBookmarkModel();
  bool added_separator = false;
  BuildMenuForPermanentNode(
      model->other_node(),
      chrome::GetBookmarkFolderIcon(chrome::BookmarkFolderIconType::kNormal,
                                    ui::kColorMenuIcon),
      menu, &added_separator);
  BuildMenuForPermanentNode(
      model->mobile_node(),
      chrome::GetBookmarkFolderIcon(chrome::BookmarkFolderIconType::kNormal,
                                    ui::kColorMenuIcon),
      menu, &added_separator);
}

void BookmarkMenuDelegate::BuildMenuForPermanentNode(const BookmarkNode* node,
                                                     const ui::ImageModel& icon,
                                                     MenuItemView* menu,
                                                     bool* added_separator) {
  if (!node->IsVisible() || node->GetTotalNodeCount() == 1)
    return;  // No children, don't create a menu.

  if (!*added_separator) {
    *added_separator = true;
    menu->AppendSeparator();
  }

  AddMenuToMaps(menu->AppendSubMenu(GetAndIncrementNextMenuID(),
                                    MaybeEscapeLabel(node->GetTitle()), icon),
                node);
}

void BookmarkMenuDelegate::BuildMenuForManagedNode(MenuItemView* menu) {
  // Don't add a separator for this menu.
  bool added_separator = true;
  const BookmarkNode* node = GetManagedBookmarkService()->managed_node();
  BuildMenuForPermanentNode(
      node,
      chrome::GetBookmarkFolderIcon(chrome::BookmarkFolderIconType::kManaged,
                                    ui::kColorMenuIcon),
      menu, &added_separator);
}

void BookmarkMenuDelegate::BuildMenu(const BookmarkNode* parent,
                                     size_t start_child_index,
                                     MenuItemView* menu) {
  DCHECK_LE(start_child_index, parent->children().size());
  if (parent == GetBookmarkModel()->other_node()) {
    ui::ImageModel bookmarks_side_panel_icon = ui::ImageModel::FromVectorIcon(
        kBookmarksSidePanelIcon, ui::kColorMenuIcon,
        ui::SimpleMenuModel::kDefaultIconSize);
    menu->AppendMenuItem(
        IDC_SHOW_BOOKMARK_SIDE_PANEL,
        l10n_util::GetStringUTF16(IDS_BOOKMARKS_ALL_BOOKMARKS_OPEN_SIDE_PANEL),
        bookmarks_side_panel_icon);
    if (!parent->children().empty()) {
      menu->AppendSeparator();
    }
  }
  const ui::ImageModel folder_icon = chrome::GetBookmarkFolderIcon(
      chrome::BookmarkFolderIconType::kNormal, ui::kColorMenuIcon);
  for (auto i = parent->children().cbegin() + start_child_index;
       i != parent->children().cend(); ++i) {
    const BookmarkNode* node = i->get();
    const int id = GetAndIncrementNextMenuID();
    MenuItemView* child_menu_item;
    if (node->is_url()) {
      child_menu_item =
          menu->AppendMenuItem(id, MaybeEscapeLabel(node->GetTitle()),
                               GetFaviconForNode(GetBookmarkModel(), node));
      child_menu_item->GetViewAccessibility().SetDescription(
          url_formatter::FormatUrl(
              node->url(), url_formatter::kFormatUrlOmitDefaults,
              base::UnescapeRule::SPACES, nullptr, nullptr, nullptr));
    } else {
      DCHECK(node->is_folder());
      child_menu_item = menu->AppendSubMenu(
          id, MaybeEscapeLabel(node->GetTitle()), folder_icon);
    }
    AddMenuToMaps(child_menu_item, node);
  }
}

void BookmarkMenuDelegate::AddMenuToMaps(MenuItemView* menu,
                                         const BookmarkNode* node) {
  menu_id_to_node_map_[menu->GetCommand()] = node;
  node_to_menu_map_[node] = menu;
}

std::u16string BookmarkMenuDelegate::MaybeEscapeLabel(
    const std::u16string& label) {
  return menu_uses_mnemonics_ ? ui::EscapeMenuLabelAmpersands(label) : label;
}

int BookmarkMenuDelegate::GetAndIncrementNextMenuID() {
  const int current_id = next_menu_id_;
  next_menu_id_ += AppMenuModel::kNumUnboundedMenuTypes;
  return current_id;
}
