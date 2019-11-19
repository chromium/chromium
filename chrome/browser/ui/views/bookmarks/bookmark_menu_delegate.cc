// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_menu_delegate.h"

#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/bookmarks/bookmark_drag_drop.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/event_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/page_navigator.h"
#include "ui/base/accelerators/menu_label_accelerator_util.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/base/window_open_disposition.h"
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

SkColor TextColorForMenu(MenuItemView* menu, views::Widget* widget) {
#if !defined(OS_MACOSX)
  // macOS incognito currently has a light on dark bookmark bar, but
  // dark on light menus, so using the theme color in the folders is
  // incorrect.
  if (widget && widget->GetThemeProvider()) {
    return widget->GetThemeProvider()->GetColor(
        ThemeProperties::COLOR_BOOKMARK_TEXT);
  }
#endif
  return menu->GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_EnabledMenuItemForegroundColor);
}

}  // namespace

BookmarkMenuDelegate::BookmarkMenuDelegate(Browser* browser,
                                           PageNavigator* navigator,
                                           views::Widget* parent)
    : browser_(browser),
      profile_(browser->profile()),
      page_navigator_(navigator),
      parent_(parent),
      menu_(NULL),
      parent_menu_item_(NULL),
      next_menu_id_(IDC_FIRST_BOOKMARK_MENU),
      real_delegate_(NULL),
      is_mutating_model_(false),
      location_(BOOKMARK_LAUNCH_LOCATION_NONE) {
}

BookmarkMenuDelegate::~BookmarkMenuDelegate() {
  GetBookmarkModel()->RemoveObserver(this);
}

void BookmarkMenuDelegate::Init(views::MenuDelegate* real_delegate,
                                MenuItemView* parent,
                                const BookmarkNode* node,
                                size_t start_child_index,
                                ShowOptions show_options,
                                BookmarkLaunchLocation location) {
  GetBookmarkModel()->AddObserver(this);
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
        !parent->GetSubmenu()->GetMenuItems().empty())
      parent->AppendSeparator();

    if (show_managed)
      BuildMenuForManagedNode(parent);
    BuildMenu(node, start_child_index, parent);
    if (show_options == SHOW_PERMANENT_FOLDERS)
      BuildMenusForPermanentNodes(parent);
  } else {
    menu_ = CreateMenu(node, start_child_index, show_options);
  }
}

void BookmarkMenuDelegate::SetPageNavigator(PageNavigator* navigator) {
  page_navigator_ = navigator;
  if (context_menu_.get())
    context_menu_->SetPageNavigator(navigator);
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

base::string16 BookmarkMenuDelegate::GetTooltipText(
    int id,
    const gfx::Point& screen_loc) const {
  auto i = menu_id_to_node_map_.find(id);
  // When removing bookmarks it may be possible to end up here without a node.
  if (i == menu_id_to_node_map_.end()) {
    DCHECK(is_mutating_model_);
    return base::string16();
  }

  const BookmarkNode* node = i->second;
  if (node->is_url()) {
    const views::TooltipManager* tooltip_manager = parent_->GetTooltipManager();
    return BookmarkBarView::CreateToolTipForURLAndTitle(
        tooltip_manager->GetMaxWidth(screen_loc),
        tooltip_manager->GetFontList(), node->url(), node->GetTitle());
  }
  return base::string16();
}

bool BookmarkMenuDelegate::IsTriggerableEvent(views::MenuItemView* menu,
                                              const ui::Event& e) {
  return e.type() == ui::ET_GESTURE_TAP ||
         e.type() == ui::ET_GESTURE_TAP_DOWN ||
         event_utils::IsPossibleDispositionEvent(e);
}

void BookmarkMenuDelegate::ExecuteCommand(int id, int mouse_event_flags) {
  DCHECK(menu_id_to_node_map_.find(id) != menu_id_to_node_map_.end());

  std::vector<const BookmarkNode*> selection = {menu_id_to_node_map_[id]};

  RecordBookmarkLaunch(location_,
                       ProfileMetrics::GetBrowserProfileType(profile_));
  chrome::OpenAll(parent_->GetNativeWindow(), page_navigator_, selection,
                  ui::DispositionFromEventFlags(mouse_event_flags),
                  profile_);
  // NOTE: |this| may be deleted.
}

bool BookmarkMenuDelegate::ShouldExecuteCommandWithoutClosingMenu(
    int id,
    const ui::Event& event) {
  return (event.flags() & ui::EF_LEFT_MOUSE_BUTTON) &&
         ui::DispositionFromEventFlags(event.flags()) ==
             WindowOpenDisposition::NEW_BACKGROUND_TAB;
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
  return (drop_node == NULL);
}

int BookmarkMenuDelegate::GetDropOperation(
    MenuItemView* item,
    const ui::DropTargetEvent& event,
    views::MenuDelegate::DropPosition* position) {
  // Should only get here if we have drop data.
  DCHECK(drop_data_.is_valid());

  const BookmarkNode* node = menu_id_to_node_map_[item->GetCommand()];
  const BookmarkNode* drop_parent = node->parent();
  size_t index_to_drop_at = size_t{drop_parent->GetIndexOf(node)};
  BookmarkModel* model = GetBookmarkModel();
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

int BookmarkMenuDelegate::OnPerformDrop(
    MenuItemView* menu,
    views::MenuDelegate::DropPosition position,
    const ui::DropTargetEvent& event) {
  const BookmarkNode* drop_node = menu_id_to_node_map_[menu->GetCommand()];
  DCHECK(drop_node);
  BookmarkModel* model = GetBookmarkModel();
  DCHECK(model);
  const BookmarkNode* drop_parent = drop_node->parent();
  DCHECK(drop_parent);
  size_t index_to_drop_at = size_t{drop_parent->GetIndexOf(drop_node)};
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

  bool copy = event.source_operations() == ui::DragDropTypes::DRAG_COPY;
  return chrome::DropBookmarks(profile_, drop_data_,
                               drop_parent, index_to_drop_at, copy);
}

bool BookmarkMenuDelegate::ShowContextMenu(MenuItemView* source,
                                           int id,
                                           const gfx::Point& p,
                                           ui::MenuSourceType source_type) {
  DCHECK(menu_id_to_node_map_.find(id) != menu_id_to_node_map_.end());
  const BookmarkNode* node = menu_id_to_node_map_[id];
  std::vector<const BookmarkNode*> nodes(1, node);
  context_menu_.reset(
      new BookmarkContextMenu(parent_, browser_, profile_, page_navigator_,
                              BOOKMARK_LAUNCH_LOCATION_APP_MENU, node->parent(),
                              nodes, ShouldCloseOnRemove(node)));
  context_menu_->set_observer(this);
  context_menu_->RunMenuAt(p, source_type);
  return true;
}

bool BookmarkMenuDelegate::CanDrag(MenuItemView* menu) {
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

void BookmarkMenuDelegate::BookmarkModelChanged() {
}

void BookmarkMenuDelegate::BookmarkNodeFaviconChanged(
    BookmarkModel* model,
    const BookmarkNode* node) {
  auto menu_pair = node_to_menu_map_.find(node);
  if (menu_pair == node_to_menu_map_.end())
    return;  // We're not showing a menu item for the node.

  const gfx::Image& image = model->GetFavicon(node);
  const gfx::ImageSkia* icon = image.IsEmpty()
                                   ? favicon::GetDefaultFavicon().ToImageSkia()
                                   : image.ToImageSkia();
  menu_pair->second->SetIcon(*icon);
}

void BookmarkMenuDelegate::WillRemoveBookmarks(
    const std::vector<const BookmarkNode*>& bookmarks) {
  DCHECK(!is_mutating_model_);
  is_mutating_model_ = true;  // Set to false in DidRemoveBookmarks().

  // Remove the observer so that when the remove happens we don't prematurely
  // cancel the menu. The observer is added back in DidRemoveBookmarks().
  GetBookmarkModel()->RemoveObserver(this);

  // Remove the menu items.
  std::set<MenuItemView*> changed_parent_menus;
  for (auto i(bookmarks.begin()); i != bookmarks.end(); ++i) {
    auto node_to_menu = node_to_menu_map_.find(*i);
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
    for (auto j(bookmarks.begin()); j != bookmarks.end(); ++j) {
      if (i->first->HasAncestor(*j)) {
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

  for (auto i(changed_parent_menus.begin()); i != changed_parent_menus.end();
       ++i)
    (*i)->ChildrenChanged();
}

void BookmarkMenuDelegate::DidRemoveBookmarks() {
  // Balances remove in WillRemoveBookmarksImpl.
  GetBookmarkModel()->AddObserver(this);
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
  menu->SetCommand(next_menu_id_++);
  AddMenuToMaps(menu, parent);
  menu->set_has_icons(true);
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
      chrome::GetBookmarkFolderIcon(TextColorForMenu(menu, parent())), menu,
      &added_separator);
  BuildMenuForPermanentNode(
      model->mobile_node(),
      chrome::GetBookmarkFolderIcon(TextColorForMenu(menu, parent())), menu,
      &added_separator);
}

void BookmarkMenuDelegate::BuildMenuForPermanentNode(const BookmarkNode* node,
                                                     const gfx::ImageSkia& icon,
                                                     MenuItemView* menu,
                                                     bool* added_separator) {
  if (!node->IsVisible() || node->GetTotalNodeCount() == 1)
    return;  // No children, don't create a menu.

  if (!*added_separator) {
    *added_separator = true;
    menu->AppendSeparator();
  }

  AddMenuToMaps(menu->AppendSubMenu(next_menu_id_++,
                                    MaybeEscapeLabel(node->GetTitle()), icon),
                node);
}

void BookmarkMenuDelegate::BuildMenuForManagedNode(MenuItemView* menu) {
  // Don't add a separator for this menu.
  bool added_separator = true;
  const BookmarkNode* node = GetManagedBookmarkService()->managed_node();
  BuildMenuForPermanentNode(
      node,
      chrome::GetBookmarkManagedFolderIcon(TextColorForMenu(menu, parent())),
      menu, &added_separator);
}

void BookmarkMenuDelegate::BuildMenu(const BookmarkNode* parent,
                                     size_t start_child_index,
                                     MenuItemView* menu) {
  DCHECK_LE(start_child_index, parent->children().size());
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  const gfx::ImageSkia folder_icon =
      chrome::GetBookmarkFolderIcon(TextColorForMenu(menu, parent_));
  for (auto i = parent->children().cbegin() + start_child_index;
       i != parent->children().cend(); ++i) {
    const BookmarkNode* node = i->get();
    const int id = next_menu_id_++;
    MenuItemView* child_menu_item;
    if (node->is_url()) {
      const gfx::Image& image = GetBookmarkModel()->GetFavicon(node);
      const gfx::ImageSkia* icon = image.IsEmpty() ?
          rb->GetImageSkiaNamed(IDR_DEFAULT_FAVICON) : image.ToImageSkia();
      child_menu_item =
          menu->AppendMenuItem(id, MaybeEscapeLabel(node->GetTitle()), *icon);
      child_menu_item->GetViewAccessibility().OverrideDescription(
          url_formatter::FormatUrl(
              node->url(), url_formatter::kFormatUrlOmitDefaults,
              net::UnescapeRule::SPACES, nullptr, nullptr, nullptr));
    } else {
      DCHECK(node->is_folder());
      child_menu_item = menu->AppendSubMenu(
          id, MaybeEscapeLabel(node->GetTitle()), folder_icon);
      child_menu_item->GetViewAccessibility().OverrideDescription("");
    }
    AddMenuToMaps(child_menu_item, node);
  }
}

void BookmarkMenuDelegate::AddMenuToMaps(MenuItemView* menu,
                                         const BookmarkNode* node) {
  menu_id_to_node_map_[menu->GetCommand()] = node;
  node_to_menu_map_[node] = menu;
}

base::string16 BookmarkMenuDelegate::MaybeEscapeLabel(
    const base::string16& label) {
  return menu_uses_mnemonics_ ? ui::EscapeMenuLabelAmpersands(label) : label;
}
