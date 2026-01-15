// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_menu_delegate.h"

#include <memory>
#include <optional>

#include "base/containers/to_vector.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_parent_folder_children.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/bookmarks/bookmark_drag_drop.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/bookmarks/bookmark_ui_operations_helper.h"
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
#include "components/bookmarks/browser/bookmark_node.h"
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
#include "ui/base/models/menu_separator_types.h"
#include "ui/base/mojom/menu_source_type.mojom-forward.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/color/color_id.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_separator.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"

using base::UserMetricsAction;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using bookmarks::BookmarkNodeData;
using content::PageNavigator;
using views::MenuItemView;
using PermanentFolderType = BookmarkParentFolder::PermanentFolderType;

namespace {

// Max width of a menu. There does not appear to be an OS value for this, yet
// both IE and FF restrict the max width of a menu.
const int kMaxMenuWidth = 400;

size_t GetSubmenuChildCount(const MenuItemView* menu) {
  return menu->HasSubmenu() ? menu->GetSubmenu()->children().size() : 0;
}

size_t SubmenuIndexOf(const MenuItemView* parent, const views::View* child) {
  std::optional<size_t> index = parent->GetSubmenu()->GetIndexOf(child);
  CHECK(index.has_value());
  return index.value();
}

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
class BookmarkModelDropObserver : public BookmarkMergedSurfaceServiceObserver {
 public:
  BookmarkModelDropObserver(Browser* browser,
                            const bookmarks::BookmarkNodeData drop_data,
                            const BookmarkParentFolder& drop_parent,
                            const size_t index_to_drop_at)
      : browser_(browser->AsWeakPtr()),
        drop_data_(std::move(drop_data)),
        drop_parent_(drop_parent),
        index_to_drop_at_(index_to_drop_at),
        bookmark_service_(BookmarkMergedSurfaceServiceFactory::GetForProfile(
            browser->profile())) {
    DCHECK(drop_data_.is_valid());
    CHECK(bookmark_service_);
    bookmark_merged_service_observation_.Observe(bookmark_service_);
  }

  BookmarkModelDropObserver(const BookmarkModelDropObserver&) = delete;
  void operator=(const BookmarkModelDropObserver&) = delete;

  ~BookmarkModelDropObserver() override { CleanUp(); }

  void Drop(const ui::DropTargetEvent& event,
            ui::mojom::DragOperation& output_drag_op) {
    if (!bookmark_service_ || !browser_) {  // Don't drop
      return;
    }

    bool copy = event.source_operations() == ui::DragDropTypes::DRAG_COPY;
    output_drag_op =
        BookmarkUIOperationsHelperMergedSurfaces(bookmark_service_,
                                                 &drop_parent_)
            .DropBookmarks(browser_->profile(), drop_data_, index_to_drop_at_,
                           copy,
                           chrome::BookmarkReorderDropTarget::kBookmarkMenu,
                           browser_.get());
  }

 private:
  // BookmarkMergedSurfaceServiceObserver:
  void BookmarkMergedSurfaceServiceLoaded() override { CleanUp(); }
  void BookmarkMergedSurfaceServiceBeingDeleted() override { CleanUp(); }
  void BookmarkNodeAdded(const BookmarkParentFolder& parent,
                         size_t index) override {
    CleanUp();
  }
  void BookmarkNodesRemoved(
      const BookmarkParentFolder& parent,
      const base::flat_set<const bookmarks::BookmarkNode*>& nodes) override {
    CleanUp();
  }
  void BookmarkNodeMoved(const BookmarkParentFolder& old_parent,
                         size_t old_index,
                         const BookmarkParentFolder& new_parent,
                         size_t new_index) override {
    CleanUp();
  }
  void BookmarkNodeChanged(const bookmarks::BookmarkNode* node) override {
    CleanUp();
  }
  void BookmarkNodeFaviconChanged(
      const bookmarks::BookmarkNode* node) override {}
  void BookmarkParentFolderChildrenReordered(
      const BookmarkParentFolder& folder) override {
    CleanUp();
  }
  void BookmarkAllUserNodesRemoved() override { CleanUp(); }

  void CleanUp() {
    bookmark_merged_service_observation_.Reset();
    bookmark_service_ = nullptr;
  }

  const base::WeakPtr<Browser> browser_;
  const bookmarks::BookmarkNodeData drop_data_;
  BookmarkParentFolder drop_parent_;
  const size_t index_to_drop_at_;
  raw_ptr<BookmarkMergedSurfaceService> bookmark_service_ = nullptr;
  base::ScopedObservation<BookmarkMergedSurfaceService,
                          BookmarkMergedSurfaceServiceObserver>
      bookmark_merged_service_observation_{this};
};

int IsInvalidDragOrDropCommand(int command_id) {
  std::unordered_set<int> invalid_command_ids = {
      IDC_SHOW_BOOKMARK_SIDE_PANEL, IDC_BOOKMARK_BAR_OPEN_ALL,
      IDC_BOOKMARK_BAR_OPEN_ALL_NEW_TAB_GROUP};
  return invalid_command_ids.contains(command_id);
}

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOpenAllCommandSeperator);

int GetOpenAllCommandsOffset(MenuItemView* menu) {
  if (!menu->HasSubmenu()) {
    return 0;
  }

  views::View* child =
      menu->GetSubmenu()->GetViewByElementId(kOpenAllCommandSeperator);
  if (child) {
    const std::optional<size_t> index = menu->GetSubmenu()->GetIndexOf(child);
    if (index.has_value()) {
      return index.value() + 1;
    }
  }

  return 0;
}
}  // namespace

BookmarkMenuDelegate::BookmarkFolderOrURL::BookmarkFolderOrURL(
    const BookmarkNode* node)
    : folder_or_url_(GetFromNode(node)) {}

BookmarkMenuDelegate::BookmarkFolderOrURL::BookmarkFolderOrURL(
    const BookmarkParentFolder& folder)
    : folder_or_url_(folder) {}

BookmarkMenuDelegate::BookmarkFolderOrURL::~BookmarkFolderOrURL() = default;

BookmarkMenuDelegate::BookmarkFolderOrURL::BookmarkFolderOrURL(
    const BookmarkFolderOrURL& other) = default;

BookmarkMenuDelegate::BookmarkFolderOrURL&
BookmarkMenuDelegate::BookmarkFolderOrURL::operator=(
    const BookmarkFolderOrURL& other) = default;

const BookmarkParentFolder*
BookmarkMenuDelegate::BookmarkFolderOrURL::GetIfBookmarkFolder() const {
  if (folder_or_url_.index() == 0) {
    return &std::get<0>(folder_or_url_);
  }
  return nullptr;
}

const BookmarkNode*
BookmarkMenuDelegate::BookmarkFolderOrURL::GetIfBookmarkURL() const {
  if (folder_or_url_.index() == 0) {
    return nullptr;
  }
  const BookmarkNode* node = std::get<1>(folder_or_url_);
  return node;
}

const BookmarkNode*
BookmarkMenuDelegate::BookmarkFolderOrURL::GetIfNonPermanentNode() const {
  const BookmarkParentFolder* folder = GetIfBookmarkFolder();
  if (folder && folder->as_permanent_folder().has_value()) {
    return nullptr;
  }
  return folder ? folder->as_non_permanent_folder() : GetIfBookmarkURL();
}

std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>
BookmarkMenuDelegate::BookmarkFolderOrURL::GetUnderlyingNodes(
    const BookmarkMergedSurfaceService* bookmark_merged_service) const {
  if (const BookmarkNode* node = GetIfBookmarkURL(); node) {
    return {node};
  }
  std::vector<const BookmarkNode*> nodes =
      bookmark_merged_service->GetUnderlyingNodes(*GetIfBookmarkFolder());
  return base::ToVector(nodes, [](const BookmarkNode* node) {
    return raw_ptr<const BookmarkNode, VectorExperimental>(node);
  });
}

// static
std::variant<BookmarkParentFolder, raw_ptr<const BookmarkNode>>
BookmarkMenuDelegate::BookmarkFolderOrURL::GetFromNode(
    const BookmarkNode* node) {
  CHECK(node);
  if (node->is_url()) {
    return node;
  }
  return BookmarkParentFolder::FromFolderNode(node);
}

BookmarkMenuDelegate::BookmarkMenuDelegate(Browser* browser,
                                           views::Widget* parent,
                                           views::MenuDelegate* real_delegate,
                                           BookmarkLaunchLocation location)
    : browser_(browser),
      profile_(browser->profile()),
      parent_(parent),
      menu_(nullptr),
      parent_menu_item_(nullptr),
      next_menu_id_(AppMenuModel::kMinBookmarksCommandId),
      real_delegate_(real_delegate),
      is_mutating_model_(false),
      location_(location) {
  bookmark_merged_service_observation_.Observe(
      GetBookmarkMergedSurfaceService());
}

BookmarkMenuDelegate::~BookmarkMenuDelegate() {
  bookmark_merged_service_observation_.Reset();
}

void BookmarkMenuDelegate::BuildFullMenu(MenuItemView* parent) {
  CHECK(!parent_menu_item_);
  CHECK(parent);
  CHECK(parent->GetSubmenu());
  parent_menu_item_ = parent;
  // Assume that the menu will only use mnemonics if there's already a parent
  // menu that uses them.
  menu_uses_mnemonics_ = parent_menu_item_->GetRootMenuItem()->has_mnemonics();
  if (ShouldHaveBookmarksTitle()) {
    const size_t title_index = GetSubmenuChildCount(parent_menu_item_);
    BuildBookmarksTitle(title_index);
  }

  const BookmarkParentFolder managed_folder =
      BookmarkParentFolder::ManagedFolder();
  if (ShouldBuildPermanentNode(managed_folder)) {
    BuildMenuForFolder(
        managed_folder,
        chrome::GetBookmarkFolderIcon(chrome::BookmarkFolderIconType::kManaged,
                                      ui::kColorMenuIcon),
        parent_menu_item_);
  }
  BuildMenu(BookmarkParentFolder::BookmarkBarFolder(), 0, parent);
  BuildMenusForPermanentNodes();
}

bookmarks::ManagedBookmarkService*
BookmarkMenuDelegate::GetManagedBookmarkService() {
  return ManagedBookmarkServiceFactory::GetForProfile(profile_);
}

const BookmarkMergedSurfaceService*
BookmarkMenuDelegate::GetBookmarkMergedSurfaceService() const {
  return BookmarkMergedSurfaceServiceFactory::GetForProfile(profile_);
}

void BookmarkMenuDelegate::SetActiveMenu(const BookmarkParentFolder& folder,
                                         size_t start_index) {
  CHECK(!parent_menu_item_);
  BookmarkFolderOrURL node(folder);
  if (!node_to_menu_map_[node]) {
    CreateMenu(folder, start_index);
  }
  menu_ = node_to_menu_map_[node];
}

void BookmarkMenuDelegate::SetMenuStartIndex(const BookmarkParentFolder& folder,
                                             size_t start_index) {
  CHECK(!parent_menu_item_);
  const BookmarkMergedSurfaceService* service =
      GetBookmarkMergedSurfaceService();
  auto node_to_start_idx = node_start_child_idx_map_.find(folder);
  const size_t prev_start_idx =
      node_to_start_idx == node_start_child_idx_map_.end()
          ? 0
          : node_to_start_idx->second;

  if (prev_start_idx == start_index) {
    return;
  }

  // It's possible the menu hasn't been built yet, so no update is necessary.
  auto node_to_menu = node_to_menu_map_.find(BookmarkFolderOrURL(folder));
  if (node_to_menu == node_to_menu_map_.end()) {
    return;
  }

  CHECK_LE(start_index, service->GetChildrenCount(folder));
  node_start_child_idx_map_[folder] = start_index;
  MenuItemView* parent_menu = node_to_menu->second;

  // Remove obsolete bookmark menus if the start index increased.
  BookmarkParentFolderChildren children = service->GetChildren(folder);
  for (size_t idx = prev_start_idx; idx < start_index; ++idx) {
    const BookmarkNode* child_node = children[idx];
    if (auto child_node_to_menu =
            node_to_menu_map_.find(BookmarkFolderOrURL(child_node));
        child_node_to_menu != node_to_menu_map_.end()) {
      RemoveBookmarkNode(child_node, child_node_to_menu->second);
    }
  }

  // Add missing bookmark menus if the start index decreased.
  for (size_t idx = start_index; idx < prev_start_idx; ++idx) {
    const BookmarkNode* child_node = children[idx];
    AddBookmarkNode(child_node, parent_menu, idx);
  }

  parent_menu->ChildrenChanged();
}

std::u16string BookmarkMenuDelegate::GetTooltipText(
    int id,
    const gfx::Point& screen_loc) const {
  auto i = menu_id_to_node_map_.find(id);
  // Ignore queries about unknown items, e.g. the empty menu item.
  if (i != menu_id_to_node_map_.end()) {
    BookmarkFolderOrURL folder_or_url = i->second;
    if (const BookmarkNode* node = folder_or_url.GetIfBookmarkURL(); node) {
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
  const bool is_click = e.type() == ui::EventType::kGestureTap ||
                        e.type() == ui::EventType::kGestureTapDown ||
                        event_utils::IsPossibleDispositionEvent(e);
  const bool is_command_click = is_click && (e.flags() & ui::EF_COMMAND_DOWN);

  // To open all bookmark pages in a submenu:
  // 1. Cmd+Click (or Win+Click on Windows) on the submenu.
  // 2. middle-mouse click the submenu.
  if (menu->GetType() == MenuItemView::Type::kSubMenu) {
    const bool is_middle_mouse =
        e.IsMouseEvent() && (e.flags() & ui::EF_MIDDLE_MOUSE_BUTTON);
    return is_command_click || is_middle_mouse;
  }

  return is_click;
}

void BookmarkMenuDelegate::ExecuteCommand(int id, int mouse_event_flags) {
  if (id == IDC_SHOW_BOOKMARK_SIDE_PANEL) {
    browser_->command_controller()->ExecuteCommand(id);
    return;
  }

  DCHECK(menu_id_to_node_map_.find(id) != menu_id_to_node_map_.end());

  bookmarks::OpenAllBookmarksContext context =
      bookmarks::OpenAllBookmarksContext::kNone;
  WindowOpenDisposition initial_disposition =
      ui::DispositionFromEventFlags(mouse_event_flags);

  if (id == IDC_BOOKMARK_BAR_OPEN_ALL) {
    initial_disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  } else if (id == IDC_BOOKMARK_BAR_OPEN_ALL_NEW_TAB_GROUP) {
    context = bookmarks::OpenAllBookmarksContext::kInGroup;
    initial_disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  }

  RecordBookmarkLaunch(location_,
                       profile_metrics::GetBrowserProfileType(profile_));

  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> selection =
      menu_id_to_node_map_.find(id)->second.GetUnderlyingNodes(
          GetBookmarkMergedSurfaceService());
  bookmarks::OpenAllIfAllowed(browser_, selection, initial_disposition,
                              context);
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
    auto menu_id_to_node = menu_id_to_node_map_.find(id);
    CHECK(menu_id_to_node != menu_id_to_node_map_.end());
    // Close the menu before opening a folder since this may pop up a dialog
    // over the menu. See https://crbug.com/1105587 for details.
    return !menu_id_to_node->second.GetIfBookmarkFolder();
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
          bookmarks::prefs::kEditBookmarksEnabled)) {
    return false;
  }

  if (drop_data_.has_single_url()) {
    return true;
  }

  const BookmarkNode* drag_node = drop_data_.GetFirstNode(
      GetBookmarkMergedSurfaceService()->bookmark_model(), profile_->GetPath());
  if (!drag_node) {
    // Dragging a folder from another profile, always accept.
    return true;
  }

  // Drag originated from same profile and is not a URL. Only accept it if
  // the dragged node is not a parent of the node menu represents.
  auto menu_id_to_node = menu_id_to_node_map_.find(menu->GetCommand());
  if (menu_id_to_node == menu_id_to_node_map_.end()) {
    // If we don't know the menu assume its because we're embedded. We'll
    // figure out the real operation when GetDropOperation is invoked.
    return true;
  }

  const BookmarkNode* non_permanent_drop_node =
      menu_id_to_node->second.GetIfNonPermanentNode();
  if (!non_permanent_drop_node) {
    // Drop on permanent node.
    // `drag_node` can't be a permanent node or a root node.
    return true;
  }

  return !non_permanent_drop_node->HasAncestor(drag_node);
}

ui::mojom::DragOperation BookmarkMenuDelegate::GetDropOperation(
    MenuItemView* item,
    const ui::DropTargetEvent& event,
    views::MenuDelegate::DropPosition* position) {
  // Should only get here if we have drop data.
  DCHECK(drop_data_.is_valid());

  if (IsInvalidDragOrDropCommand(item->GetCommand())) {
    return ui::mojom::DragOperation::kNone;
  }

  std::optional<DropParams> drop_params = GetDropParams(item, position);
  if (!drop_params) {
    return ui::mojom::DragOperation::kNone;
  }
  return chrome::GetBookmarkDropOperation(profile_, event, drop_data_,
                                          drop_params->drop_parent,
                                          drop_params->index_to_drop_at);
}

views::View::DropCallback BookmarkMenuDelegate::GetDropCallback(
    views::MenuItemView* menu,
    views::MenuDelegate::DropPosition position,
    const ui::DropTargetEvent& event) {
  std::optional<BookmarkMenuDelegate::DropParams> drop_params =
      GetDropParams(menu, &position);
  CHECK(drop_params);

  std::unique_ptr<BookmarkModelDropObserver> drop_observer =
      std::make_unique<BookmarkModelDropObserver>(
          browser_, std::move(drop_data_), drop_params->drop_parent,
          drop_params->index_to_drop_at);
  return base::BindOnce(
      [](BookmarkModelDropObserver* drop_observer,
         const ui::DropTargetEvent& event,
         ui::mojom::DragOperation& output_drag_op,
         std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner) {
        drop_observer->Drop(event, output_drag_op);
      },
      base::Owned(std::move(drop_observer)));
}

bool BookmarkMenuDelegate::ShowContextMenu(
    MenuItemView* source,
    int id,
    const gfx::Point& p,
    ui::mojom::MenuSourceType source_type) {
  // The IDC_SHOW_BOOKMARK_SIDE_PANEL menu item does not map to a bookmark node
  // and therefore no context menu for it should be shown.
  auto menu_id_to_node = menu_id_to_node_map_.find(id);
  if (menu_id_to_node == menu_id_to_node_map_.end()) {
    return false;
  }

  const BookmarkFolderOrURL folder_or_url = menu_id_to_node->second;
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes =
      folder_or_url.GetUnderlyingNodes(GetBookmarkMergedSurfaceService());
  context_menu_ = std::make_unique<BookmarkContextMenu>(
      parent_, browser_, profile_, location_, nodes,
      ShouldCloseOnRemove(folder_or_url));
  bookmark_context_menu_observation_.Observe(context_menu_.get());
  context_menu_->RunMenuAt(p, source_type);
  return true;
}

bool BookmarkMenuDelegate::CanDrag(MenuItemView* menu) {
  if (IsInvalidDragOrDropCommand(menu->GetCommand())) {
    return false;
  }
  // Don't let users drag permanent nodes (managed, other or mobile folder).
  return menu_id_to_node_map_.find(menu->GetCommand())
      ->second.GetIfNonPermanentNode();
}

void BookmarkMenuDelegate::WriteDragData(MenuItemView* sender,
                                         ui::OSExchangeData* data) {
  CHECK(sender);
  CHECK(data);

  base::RecordAction(UserMetricsAction("BookmarkBar_DragFromFolder"));

  const BookmarkNode* node = menu_id_to_node_map_.find(sender->GetCommand())
                                 ->second.GetIfNonPermanentNode();
  // Permanent nodes can't be dragged.
  CHECK(node);
  BookmarkNodeData drag_data(node);
  drag_data.Write(profile_->GetPath(), data);
}

int BookmarkMenuDelegate::GetDragOperations(MenuItemView* sender) {
  return chrome::GetBookmarkDragOperation(
      profile_, menu_id_to_node_map_.find(sender->GetCommand())
                    ->second.GetIfNonPermanentNode());
}

int BookmarkMenuDelegate::GetMaxWidthForMenu(MenuItemView* menu) {
  return kMaxMenuWidth;
}

void BookmarkMenuDelegate::WillShowMenu(MenuItemView* menu) {
  auto iter = menu_id_to_node_map_.find(menu->GetCommand());
  if (iter != menu_id_to_node_map_.end() &&
      menu->GetSubmenu()->GetMenuItems().empty()) {
    if (const BookmarkParentFolder* folder = iter->second.GetIfBookmarkFolder();
        folder &&
        GetBookmarkMergedSurfaceService()->GetChildrenCount(*folder)) {
      BuildMenu(*folder, 0, menu);
    }
  }
}

void BookmarkMenuDelegate::BookmarkNodeMoved(
    const BookmarkParentFolder& old_parent,
    size_t old_index,
    const BookmarkParentFolder& new_parent,
    size_t new_index) {
  MenuItemView* old_parent_menu = nullptr;
  BookmarkMergedSurfaceService* service = GetBookmarkMergedSurfaceService();
  const BookmarkFolderOrURL moved_node =
      BookmarkFolderOrURL(service->GetNodeAtIndex(new_parent, new_index));
  // Permanent nodes can't be moved.
  CHECK(moved_node.GetIfNonPermanentNode());
  auto node_to_menu = node_to_menu_map_.find(moved_node);

  // The moved node will not have a menu item if it was moved without being
  // built (e.g., through the bookmark editor).
  if (node_to_menu != node_to_menu_map_.end()) {
    MenuItemView* moved_menu = node_to_menu->second;
    old_parent_menu = moved_menu->GetParentMenuItem();
    RemoveBookmarkNode(moved_node.GetIfNonPermanentNode(), moved_menu);
    if (old_parent_menu) {
      UpdateOpenAllCommands(old_parent_menu, old_parent);
    }
  }

  GetAndUpdateStaleMenuArtifacts();

  const BookmarkFolderOrURL folder_or_url_parent(new_parent);
  auto parent_node_to_menu = node_to_menu_map_.find(folder_or_url_parent);
  MenuItemView* new_parent_menu = parent_node_to_menu != node_to_menu_map_.end()
                                      ? parent_node_to_menu->second
                                      : nullptr;
  // The new parent's menu might not exist from this controller's perspective.
  // E.g., the bookmark is moved from a menu controlled by this, to a
  // different menu controlled by another controller.
  if (new_parent_menu) {
    CHECK(new_parent_menu->HasSubmenu());
    // The parent menu might exist but might not be built yet.
    // E.g., drag and dropping the bookmark onto an empty folder; the folder
    // will not have been built yet because it's empty.
    if (built_nodes_.contains(new_parent)) {
      AddBookmarkNode(moved_node.GetIfNonPermanentNode(), new_parent_menu,
                      new_index);
    }

    UpdateOpenAllCommands(new_parent_menu, new_parent);
  }

  if (old_parent_menu) {
    old_parent_menu->ChildrenChanged();
  }

  if (new_parent_menu) {
    if (!new_parent.HasAncestor(old_parent)) {
      new_parent_menu->ChildrenChanged();
    }

    // Open the new parent menu if the model was changed from a drag.
    views::MenuController* new_menu_controller =
        new_parent_menu ? new_parent_menu->GetMenuController() : nullptr;
    if (new_menu_controller && new_menu_controller->drag_in_progress()) {
      new_menu_controller->SelectItemAndOpenSubmenu(new_parent_menu);
    }
  }
}

void BookmarkMenuDelegate::BookmarkNodeFaviconChanged(
    const BookmarkNode* node) {
  auto menu_pair = node_to_menu_map_.find(BookmarkFolderOrURL(node));
  if (menu_pair == node_to_menu_map_.end()) {
    return;  // We're not showing a menu item for the node.
  }

  menu_pair->second->SetIcon(GetFaviconForNode(
      GetBookmarkMergedSurfaceService()->bookmark_model(), node));
}

void BookmarkMenuDelegate::WillRemoveBookmarks(
    const std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>&
        bookmarks) {
  DCHECK(!is_mutating_model_);
  is_mutating_model_ = true;  // Set to false in DidRemoveBookmarks().

  // Remove the observer so that when the remove happens we don't prematurely
  // cancel the menu. The observer is added back in DidRemoveBookmarks().
  bookmark_merged_service_observation_.Reset();

  // Remove the menu items.
  std::set<MenuItemView*> changed_parent_menus;
  for (const BookmarkNode* bookmark : bookmarks) {
    // Permanent nodes can't be removed from the UI.
    CHECK(!bookmark->is_permanent_node());
    auto node_to_menu = node_to_menu_map_.find(BookmarkFolderOrURL(bookmark));
    if (node_to_menu != node_to_menu_map_.end()) {
      MenuItemView* menu = node_to_menu->second;
      RemoveBookmarkNode(bookmark, menu);
      if (MenuItemView* parent_menu = menu->GetParentMenuItem()) {
        changed_parent_menus.insert(parent_menu);
      }
    }
  }

  // All the bookmarks in |bookmarks| should have the same parent. It's possible
  // to support different parents, but this would need to prune any nodes whose
  // parent has been removed. As all nodes currently have the same parent, there
  // is the DCHECK.
  DCHECK_LE(changed_parent_menus.size(), 1U);

  for (MenuItemView* changed_parent_menu : changed_parent_menus) {
    changed_parent_menu->ChildrenChanged();
  }
}

// E.g, the App menu should remove the "other bookmarks" menu item if this is
// the last item being removed from it.
void BookmarkMenuDelegate::RemoveBookmarkNode(const BookmarkNode* node,
                                              MenuItemView* menu) {
  CHECK(!node->is_root());
  // Remove any descendants of the removed nodes.
  for (auto i = node_to_menu_map_.begin(); i != node_to_menu_map_.end();) {
    auto underlying_nodes =
        i->first.GetUnderlyingNodes(GetBookmarkMergedSurfaceService());
    if (underlying_nodes.size() != 1) {
      // This menu represents more than one node, this is possible for permanent
      // nodes. Given that:
      // - Root node can't be removed
      // - Only one node is removed (not all underlying nodes)
      // This menu shouldn't be removed.
      ++i;
      continue;
    }

    if (!underlying_nodes[0]->HasAncestor(node)) {
      ++i;
      continue;
    }
    if (i->first.GetIfBookmarkFolder()) {
      built_nodes_.erase(*i->first.GetIfBookmarkFolder());
    }
    menu_id_to_node_map_.erase(i->second->GetCommand());
    i = node_to_menu_map_.erase(i);
  }

  // The parent menu is null when removing a root menu item.
  if (MenuItemView* parent_menu = menu->GetParentMenuItem()) {
    parent_menu->RemoveMenuItem(menu);
  }
}

void BookmarkMenuDelegate::AddBookmarkNode(const bookmarks::BookmarkNode* node,
                                           MenuItemView* new_parent_menu,
                                           size_t new_index) {
  const BookmarkParentFolder new_parent_folder =
      BookmarkParentFolder::FromFolderNode(node->parent());
  size_t insertion_idx = new_index;

  // The bookmark bar view creates individual menus for bookmarks in the
  // bookmarks bar. Bookmarks that overflow from the bar belong to a
  // single menu, which uses a node offset. This offset should be applied to
  // `new_index` to ensure the moved node's menu item appears in the right
  // spot in the overflow menu.
  if (auto node_to_start_child_idx =
          node_start_child_idx_map_.find(new_parent_folder);
      node_to_start_child_idx != node_start_child_idx_map_.end()) {
    // If `new_index` is less than the menu's start index, this means that
    // the moved bookmark isn't in its parent's menu. The client will reorder
    // the menu in the bookmarks bar. Therefore, we skip the update.
    if (new_index < node_to_start_child_idx->second) {
      return;
    }
    insertion_idx -= node_to_start_child_idx->second;
  }

  // If the bookmark is embedded in a larger menu not controlled by this (e.g.,
  // App menu), then the bookmark's menu item is inserted relative to the
  // "Bookmarks" title.
  if (new_parent_menu == parent_menu_item_) {
    if (bookmarks_title_) {
      insertion_idx += SubmenuIndexOf(parent_menu_item_, bookmarks_title_) + 1;
    }
    // The managed bookmarks folder is displayed immediately after the
    // "Bookmarks" title.
    if (node_to_menu_map_.contains(
            BookmarkFolderOrURL(BookmarkParentFolder::ManagedFolder()))) {
      ++insertion_idx;
    }
  }

  // The "other" bookmarks folder is built with a header. The new node's menu
  // is inserted relative to that.
  if (new_parent_folder.as_permanent_folder() ==
      BookmarkParentFolder::PermanentFolderType::kOtherNode) {
    CHECK(other_node_menu_separator_);
    insertion_idx +=
        SubmenuIndexOf(new_parent_menu, other_node_menu_separator_) + 1;
  }

  // The menu may start with 'open all' and 'open all as tab group' items
  // and we insert the |node| relative to these.
  insertion_idx += GetOpenAllCommandsOffset(new_parent_menu);
  BuildNodeMenuItemAt(node, new_parent_menu, insertion_idx);
}

// TODO(crbug.com/382711086): This should be updated to also remove
// empty permanent folders of the App menu. Warning: simply removing the menu
// here breaks DnD when the dragged bookmark is a child of the removed permanent
// node folder because DnD clients don't gracefully handle deleting the widget
// running the drag loop.
std::vector<raw_ref<MenuItemView>>
BookmarkMenuDelegate::GetAndUpdateStaleMenuArtifacts() {
  std::vector<raw_ref<MenuItemView>> updated_menus = {};
  if (parent_menu_item_) {
    if (MenuItemView* updated_menu = UpdateBookmarksTitle()) {
      updated_menus.emplace_back(*updated_menu);
    }
  }
  if (MenuItemView* updated_menu = UpdateOtherNodeSeparator()) {
    updated_menus.emplace_back(*updated_menu);
  }
  return updated_menus;
}

void BookmarkMenuDelegate::DidRemoveBookmarks() {
  // Balances remove in WillRemoveBookmarksImpl.
  bookmark_merged_service_observation_.Observe(
      GetBookmarkMergedSurfaceService());
  DCHECK(is_mutating_model_);

  std::vector<raw_ref<MenuItemView>> updated_menus =
      GetAndUpdateStaleMenuArtifacts();
  for (raw_ref<MenuItemView> updated_menu : updated_menus) {
    updated_menu->ChildrenChanged();
  }

  is_mutating_model_ = false;
}

void BookmarkMenuDelegate::OnContextMenuClosed() {
  bookmark_context_menu_observation_.Reset();
  context_menu_.reset();
}

bool BookmarkMenuDelegate::IsDropValid(
    const BookmarkFolderOrURL* target,
    const views::MenuDelegate::DropPosition* position) {
  CHECK(target);
  const BookmarkParentFolder* target_folder = target->GetIfBookmarkFolder();
  bool drop_on_url_node = !target_folder;
  switch (*position) {
    case views::MenuDelegate::DropPosition::kUnknow:
    case views::MenuDelegate::DropPosition::kNone:
      return false;

    case views::MenuDelegate::DropPosition::kBefore:
      if (drop_on_url_node || target_folder->HoldsNonPermanentFolder() ||
          target_folder->as_permanent_folder() ==
              PermanentFolderType::kOtherNode) {
        return true;
      }
      // Dropping before permanent mobile and managed nodes makes no sense.
      return false;

    case views::MenuDelegate::DropPosition::kAfter:
      if (drop_on_url_node || target_folder->HoldsNonPermanentFolder() ||
          target_folder->as_permanent_folder() ==
              PermanentFolderType::kManagedNode) {
        return true;
      }
      // Dropping after permanent other and mobile nodes makes no sense.
      return false;

    case views::MenuDelegate::DropPosition::kOn:
      return !drop_on_url_node;
  }
  NOTREACHED();
}

std::optional<BookmarkMenuDelegate::DropParams>
BookmarkMenuDelegate::GetDropParams(
    views::MenuItemView* menu,
    views::MenuDelegate::DropPosition* position) {
  const BookmarkFolderOrURL drop_node =
      menu_id_to_node_map_.find(menu->GetCommand())->second;
  if (!IsDropValid(&drop_node, position)) {
    return std::nullopt;
  }

  const BookmarkParentFolder* drop_folder = drop_node.GetIfBookmarkFolder();
  // Initial params drop on bookmark bar.
  DropParams drop_params(BookmarkParentFolder::BookmarkBarFolder(), 0);
  const BookmarkMergedSurfaceService* service =
      GetBookmarkMergedSurfaceService();
  switch (*position) {
    case views::MenuDelegate::DropPosition::kAfter:
      if (drop_folder && drop_folder->as_permanent_folder() ==
                             PermanentFolderType::kManagedNode) {
        // This can happen with SHOW_PERMANENT_FOLDERS.
        // Managed folder is shown at the top of the bookmarks menu.
        // Use initial params for `drop_params` with the parent as the bookmark
        // bar and the index is 0.
        CHECK_EQ(*drop_params.drop_parent.as_permanent_folder(),
                 PermanentFolderType::kBookmarkBarNode);
      } else {
        // Drop after a URL or non permanent node.
        const BookmarkNode* node = drop_node.GetIfNonPermanentNode();
        CHECK(node);
        drop_params.drop_parent =
            BookmarkParentFolder::FromFolderNode(node->parent());
        drop_params.index_to_drop_at = service->GetIndexOf(node) + 1;
      }
      break;

    case views::MenuDelegate::DropPosition::kOn:
      CHECK(drop_folder);
      drop_params.drop_parent = *drop_folder;
      drop_params.index_to_drop_at = service->GetChildrenCount(*drop_folder);
      break;

    case views::MenuDelegate::DropPosition::kBefore:
      if (drop_folder && drop_folder->as_permanent_folder() ==
                             PermanentFolderType::kOtherNode) {
        // This can happen with SHOW_PERMANENT_FOLDERS.
        CHECK_EQ(*drop_params.drop_parent.as_permanent_folder(),
                 PermanentFolderType::kBookmarkBarNode);
        drop_params.index_to_drop_at =
            service->GetChildrenCount(drop_params.drop_parent);
      } else {
        // Drop before a URL or non permanent node.
        const BookmarkNode* node = drop_node.GetIfNonPermanentNode();
        CHECK(node);
        drop_params.drop_parent =
            BookmarkParentFolder::FromFolderNode(node->parent());
        drop_params.index_to_drop_at = service->GetIndexOf(node);
      }
      break;

    case views::MenuDelegate::DropPosition::kNone:
    case views::MenuDelegate::DropPosition::kUnknow:
      NOTREACHED();
  }
  return drop_params;
}

bool BookmarkMenuDelegate::ShouldCloseOnRemove(
    const BookmarkFolderOrURL& folder_or_url) const {
  // We never need to close when embedded in the app menu.
  const bool is_shown_from_app_menu = parent_menu_item_ != nullptr;
  if (is_shown_from_app_menu) {
    return false;
  }

  const BookmarkNode* node = folder_or_url.GetIfNonPermanentNode();
  if (!node) {
    // Permanent node.
    return false;
  }

  const bool is_only_child_of_other_folder =
      node->parent()->type() == BookmarkNode::OTHER_NODE &&
      GetBookmarkMergedSurfaceService()->GetChildrenCount(
          BookmarkParentFolder::OtherFolder()) == 1u;
  const bool is_child_of_bookmark_bar =
      node->parent()->type() == BookmarkNode::BOOKMARK_BAR;
  // The 'other' bookmarks folder hides when it has no more items, so we need
  // to exit the menu when the last node is removed.
  // If the parent is the bookmark bar, then the menu is showing for an item on
  // the bookmark bar. When removing this item we need to close the menu (as
  // there is no longer anything to anchor the menu to).
  return is_only_child_of_other_folder || is_child_of_bookmark_bar;
}

MenuItemView* BookmarkMenuDelegate::CreateMenu(
    const BookmarkParentFolder& folder,
    size_t start_child_index) {
  MenuItemView* menu = new MenuItemView(real_delegate_);
  menu->SetCommand(GetAndIncrementNextMenuID());
  AddMenuToMaps(menu, BookmarkFolderOrURL(folder));
  node_start_child_idx_map_[folder] = start_child_index;

  BuildMenu(folder, start_child_index, menu);
  return menu;
}

bool BookmarkMenuDelegate::ShouldBuildPermanentNode(
    const BookmarkParentFolder& folder) const {
  return GetBookmarkMergedSurfaceService()->GetChildrenCount(folder);
}

void BookmarkMenuDelegate::BuildMenusForPermanentNodes() {
  CHECK(parent_menu_item_);
  const BookmarkParentFolder other_folder(BookmarkParentFolder::OtherFolder());
  const BookmarkParentFolder mobile_folder(
      BookmarkParentFolder::MobileFolder());
  const bool should_build_other_node = ShouldBuildPermanentNode(other_folder);
  const bool should_build_mobile_node = ShouldBuildPermanentNode(mobile_folder);

  if (!should_build_other_node && !should_build_mobile_node) {
    return;
  }

  views::SubmenuView* submenu = parent_menu_item_->GetSubmenu();
  CHECK(!permanent_nodes_separator_);
  parent_menu_item_->AppendSeparator();
  permanent_nodes_separator_ = submenu->children().back().get();

  const ui::ImageModel& folder_icon = chrome::GetBookmarkFolderIcon(
      chrome::BookmarkFolderIconType::kNormal, ui::kColorMenuIcon);
  if (should_build_other_node) {
    BuildMenuForFolder(other_folder, folder_icon, parent_menu_item_);
  }

  if (should_build_mobile_node) {
    BuildMenuForFolder(mobile_folder, folder_icon, parent_menu_item_);
  }
}

void BookmarkMenuDelegate::BuildMenuForFolder(
    const BookmarkParentFolder& folder,
    const ui::ImageModel& icon,
    MenuItemView* parent_menu) {
  BuildMenuForFolderAt(folder, icon, parent_menu,
                       GetSubmenuChildCount(parent_menu));
}

void BookmarkMenuDelegate::BuildMenuForFolderAt(
    const BookmarkParentFolder& folder,
    const ui::ImageModel& icon,
    MenuItemView* parent_menu,
    size_t index) {
  // Underlying nodes share the same title.
  std::vector<const BookmarkNode*> nodes =
      GetBookmarkMergedSurfaceService()->GetUnderlyingNodes(folder);
  CHECK(!nodes.empty());
  AddMenuToMaps(parent_menu->AddMenuItemAt(
                    index, GetAndIncrementNextMenuID(),
                    MaybeEscapeLabel(nodes[0]->GetTitle()), std::u16string(),
                    std::u16string(), ui::ImageModel(), icon,
                    MenuItemView::Type::kSubMenu, ui::NORMAL_SEPARATOR),
                BookmarkFolderOrURL(folder));
}

void BookmarkMenuDelegate::BuildMenuForURLAt(const BookmarkNode* node,
                                             MenuItemView* parent_menu,
                                             size_t index) {
  MenuItemView* child_menu_item = parent_menu->AddMenuItemAt(
      index, GetAndIncrementNextMenuID(), MaybeEscapeLabel(node->GetTitle()),
      std::u16string(), std::u16string(), ui::ImageModel(),
      GetFaviconForNode(GetBookmarkMergedSurfaceService()->bookmark_model(),
                        node),
      MenuItemView::Type::kNormal, ui::NORMAL_SEPARATOR);
  child_menu_item->GetViewAccessibility().SetDescription(
      url_formatter::FormatUrl(
          node->url(), url_formatter::kFormatUrlOmitDefaults,
          base::UnescapeRule::SPACES, nullptr, nullptr, nullptr));
  AddMenuToMaps(child_menu_item, BookmarkFolderOrURL(node));
}

void BookmarkMenuDelegate::BuildNodeMenuItem(const BookmarkNode* node,
                                             MenuItemView* parent_menu) {
  BuildNodeMenuItemAt(node, parent_menu, GetSubmenuChildCount(parent_menu));
}

void BookmarkMenuDelegate::BuildNodeMenuItemAt(const BookmarkNode* node,
                                               MenuItemView* parent_menu,
                                               size_t index) {
  if (node->is_url()) {
    BuildMenuForURLAt(node, parent_menu, index);
  } else {
    CHECK(node->is_folder());
    const ui::ImageModel folder_icon = chrome::GetBookmarkFolderIcon(
        chrome::BookmarkFolderIconType::kNormal, ui::kColorMenuIcon);
    BuildMenuForFolderAt(BookmarkParentFolder::FromFolderNode(node),
                         folder_icon, parent_menu, index);
  }
}

void BookmarkMenuDelegate::BuildMenu(const BookmarkParentFolder& folder,
                                     size_t start_child_index,
                                     MenuItemView* menu) {
  const BookmarkMergedSurfaceService* service =
      GetBookmarkMergedSurfaceService();
  DCHECK_LE(start_child_index, service->GetChildrenCount(folder));
  if (folder.as_permanent_folder() ==
      BookmarkParentFolder::PermanentFolderType::kOtherNode) {
    BuildOtherNodeMenuHeader(menu);
  } else if (base::FeatureList::IsEnabled(
                 features::kTabGroupMenuImprovements)) {
    const BookmarkNode* node = folder.as_non_permanent_folder();
    if (node && node->parent() &&
        node->parent()->type() == BookmarkNode::Type::BOOKMARK_BAR) {
      if (parent_menu_item_ == nullptr) {
        MaybeAppendOpenAllCommandItems(menu, folder);
      }
    }
  }
  const ui::ImageModel folder_icon = chrome::GetBookmarkFolderIcon(
      chrome::BookmarkFolderIconType::kNormal, ui::kColorMenuIcon);
  BookmarkParentFolderChildren children =
      GetBookmarkMergedSurfaceService()->GetChildren(folder);
  for (auto i = children.begin() + start_child_index; i != children.end();
       ++i) {
    BuildNodeMenuItem(*i, menu);
  }
  AddMenuToMaps(menu, BookmarkFolderOrURL(folder));
  built_nodes_.insert(folder);
}

void BookmarkMenuDelegate::AddMenuToMaps(MenuItemView* menu,
                                         const BookmarkFolderOrURL& node) {
  menu_id_to_node_map_.insert_or_assign(menu->GetCommand(), node);
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

MenuItemView* BookmarkMenuDelegate::UpdateBookmarksTitle() {
  CHECK(parent_menu_item_);
  CHECK(parent_menu_item_->HasSubmenu());
  // Check if we need to add/remove the bookmarks title. If not, then return
  // null since the parent menu doesn't need to be updated.
  const bool should_have_title = ShouldHaveBookmarksTitle();
  if (!bookmarks_title_ && !should_have_title) {
    return nullptr;
  }
  if (bookmarks_title_ && should_have_title) {
    return nullptr;
  }

  if (bookmarks_title_) {
    RemoveBookmarksTitle();
  } else {
    // If permanent nodes are already built in `parent_menu_item_`, then add the
    // title above them. Otherwise, append the title to the parent menu.
    // E.g., this can happen in the App menu if there are initially no bookmarks
    // in the bookmarks bar, but there are bookmarks in the "other" bookmarks
    // folder, which has its own section. The "Bookmarks" title would need
    // to be inserted above the "other" bookmarks.
    size_t offset =
        permanent_nodes_separator_
            ? SubmenuIndexOf(parent_menu_item_, permanent_nodes_separator_)
            : GetSubmenuChildCount(parent_menu_item_);
    BuildBookmarksTitle(offset);
  }
  return parent_menu_item_;
}

bool BookmarkMenuDelegate::ShouldHaveBookmarksTitle() {
  CHECK(parent_menu_item_);
  // In practice, the parent menu item is never empty.
  // If this assumption is wrong, there may be a redundant "separator" visual
  // artifact, but the code will continue to function correctly (hence why we
  // don't crash here).
  // If this ever changes, then the delegate will need to observe and
  // react to non-bookmark changes in its parent menu, which is currently not
  // supported.
  if (parent_menu_item_->GetSubmenu()->children().empty()) {
    DCHECK(false) << "Expected parent menu item to be empty";
    base::debug::DumpWithoutCrashing();
  }
  const BookmarkMergedSurfaceService* service =
      GetBookmarkMergedSurfaceService();
  const bool bookmark_bar_has_children =
      service->GetChildrenCount(BookmarkParentFolder::BookmarkBarFolder());
  return (bookmark_bar_has_children ||
          ShouldBuildPermanentNode(BookmarkParentFolder::ManagedFolder()));
}

void BookmarkMenuDelegate::BuildBookmarksTitle(size_t index) {
  CHECK(!bookmarks_title_);
  CHECK(!bookmarks_title_separator_);
  parent_menu_item_->AddSeparatorAt(index);
  bookmarks_title_separator_ =
      parent_menu_item_->GetSubmenu()->children()[index].get();
  bookmarks_title_ = parent_menu_item_->AddTitleAt(
      l10n_util::GetStringUTF16(IDS_BOOKMARKS_LIST_TITLE), index + 1);
}

void BookmarkMenuDelegate::RemoveBookmarksTitle() {
  CHECK(parent_menu_item_);
  CHECK(bookmarks_title_);
  CHECK(bookmarks_title_separator_);
  views::View* title = bookmarks_title_.get();
  views::View* separator = bookmarks_title_separator_.get();
  bookmarks_title_ = nullptr;
  bookmarks_title_separator_ = nullptr;
  parent_menu_item_->RemoveMenuItem(title);
  parent_menu_item_->RemoveMenuItem(separator);
}

MenuItemView* BookmarkMenuDelegate::UpdateOtherNodeSeparator() {
  const BookmarkParentFolder other_folder = BookmarkParentFolder::OtherFolder();

  // The menu hasn't been built yet, so no update required.
  if (!built_nodes_.contains(other_folder)) {
    return nullptr;
  }

  const bool should_have_separator =
      GetBookmarkMergedSurfaceService()->GetChildrenCount(other_folder);
  // Check if we need to add/remove the separator. If not, then return
  // null since the parent menu doesn't need to be updated.
  if (!should_have_separator && !other_node_menu_separator_) {
    return nullptr;
  }
  if (should_have_separator && other_node_menu_separator_) {
    return nullptr;
  }

  MenuItemView* other_node_menu =
      node_to_menu_map_[BookmarkFolderOrURL(other_folder)];
  if (other_node_menu_separator_) {
    views::View* separator = other_node_menu_separator_.get();
    other_node_menu_separator_ = nullptr;
    other_node_menu->RemoveMenuItem(separator);
  } else {
    other_node_menu->RemoveAllMenuItems();
    BuildOtherNodeMenuHeader(other_node_menu);
  }
  return other_node_menu;
}

void BookmarkMenuDelegate::BuildOtherNodeMenuHeader(MenuItemView* menu) {
  // This menu can be in an inconsistent state when dragging bookmarks, so
  // enforce that it's empty before building its contents.
  other_node_menu_separator_ = nullptr;
  if (menu->HasSubmenu()) {
    menu->RemoveAllMenuItems();
  }
  ui::ImageModel bookmarks_side_panel_icon = ui::ImageModel::FromVectorIcon(
      kBookmarksSidePanelIcon, ui::kColorMenuIcon,
      ui::SimpleMenuModel::kDefaultIconSize);
  menu->AppendMenuItem(
      IDC_SHOW_BOOKMARK_SIDE_PANEL,
      l10n_util::GetStringUTF16(IDS_BOOKMARKS_ALL_BOOKMARKS_OPEN_SIDE_PANEL),
      bookmarks_side_panel_icon);
  bool other_folder_children_count =
      GetBookmarkMergedSurfaceService()->GetChildrenCount(
          BookmarkParentFolder::OtherFolder());
  if (other_folder_children_count) {
    menu->AppendSeparator();
    other_node_menu_separator_ = menu->GetSubmenu()->children().back().get();
  }
}

void BookmarkMenuDelegate::MaybeAppendOpenAllCommandItems(
    views::MenuItemView* menu,
    const BookmarkParentFolder& folder) {
  CHECK_EQ(GetSubmenuChildCount(menu), 0u);
  const bookmarks::BookmarkNode* node =
      GetBookmarkMergedSurfaceService()->GetUnderlyingNodes(folder)[0];
  int count = bookmarks::OpenCount(node);

  if (count > 0) {
    menu->AppendMenuItem(IDC_BOOKMARK_BAR_OPEN_ALL,
                         l10n_util::GetPluralStringFUTF16(
                             IDS_BOOKMARK_BAR_OPEN_ALL_COUNT, count));

    menu->AppendMenuItem(
        IDC_BOOKMARK_BAR_OPEN_ALL_NEW_TAB_GROUP,
        l10n_util::GetPluralStringFUTF16(
            IDS_BOOKMARK_BAR_OPEN_ALL_COUNT_NEW_TAB_GROUP, count));

    menu_id_to_node_map_.insert_or_assign(IDC_BOOKMARK_BAR_OPEN_ALL,
                                          BookmarkFolderOrURL(folder));
    menu_id_to_node_map_.insert_or_assign(
        IDC_BOOKMARK_BAR_OPEN_ALL_NEW_TAB_GROUP, BookmarkFolderOrURL(folder));

    menu->AppendSeparator();

    menu->GetSubmenu()->children().back()->SetProperty(
        views::kElementIdentifierKey, kOpenAllCommandSeperator);
  }
}

void BookmarkMenuDelegate::UpdateOpenAllCommands(
    MenuItemView* menu,
    const BookmarkParentFolder& folder) {
  if (GetOpenAllCommandsOffset(menu) > 0) {
    const bookmarks::BookmarkNode* node =
        GetBookmarkMergedSurfaceService()->GetUnderlyingNodes(folder)[0];
    int open_count = bookmarks::OpenCount(node);
    std::vector<MenuItemView*> menu_items = menu->GetSubmenu()->GetMenuItems();
    bool enable_items = open_count > 0;

    // Update the first two items
    if (enable_items) {
      menu_items[0]->SetTitle(l10n_util::GetPluralStringFUTF16(
          IDS_BOOKMARK_BAR_OPEN_ALL_COUNT, open_count));
      menu_items[1]->SetTitle(l10n_util::GetPluralStringFUTF16(
          IDS_BOOKMARK_BAR_OPEN_ALL_COUNT_NEW_TAB_GROUP, open_count));
    }

    menu_items[0]->SetEnabled(enable_items);
    menu_items[1]->SetEnabled(enable_items);
  }
}
