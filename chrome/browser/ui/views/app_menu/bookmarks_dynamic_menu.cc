// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/bookmarks_dynamic_menu.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_parent_folder_children.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_properties.h"
#include "chrome/browser/ui/bookmarks/bookmark_drag_drop.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/bookmarks/bookmark_ui_operations_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu.h"
#include "chrome/browser/ui/views/app_menu/app_menu_action_item.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "ui/actions/actions.h"
#include "ui/base/class_property.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/window_open_disposition.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer_tree_owner.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(BookmarksDynamicMenu::BookmarkFolderOrURL*)

namespace {

using PermanentFolderType = BookmarkParentFolder::PermanentFolderType;

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(BookmarksDynamicMenu::BookmarkFolderOrURL,
                                   kBookmarkFolderOrURLKey)

// Executes a bookmark drop operation while observing the bookmark service so
// that the drop is aborted if the underlying model changes before the drop
// callback runs.
class BookmarkModelDropObserver : public BookmarkMergedSurfaceServiceObserver {
 public:
  BookmarkModelDropObserver(BrowserWindowInterface* browser,
                            bookmarks::BookmarkNodeData drop_data,
                            const BookmarkParentFolder& drop_parent,
                            size_t index_to_drop_at)
      : browser_(browser),
        drop_data_(std::move(drop_data)),
        drop_parent_(drop_parent),
        index_to_drop_at_(index_to_drop_at),
        bookmark_service_(BookmarkMergedSurfaceServiceFactory::GetForProfile(
            browser->GetProfile())) {
    DCHECK(drop_data_.is_valid());
    CHECK(bookmark_service_);
    bookmark_service_observation_.Observe(bookmark_service_);
  }

  BookmarkModelDropObserver(const BookmarkModelDropObserver&) = delete;
  BookmarkModelDropObserver& operator=(const BookmarkModelDropObserver&) =
      delete;

  ~BookmarkModelDropObserver() override { CleanUp(); }

  void Drop(const ui::DropTargetEvent& event,
            ui::mojom::DragOperation& output_drag_op) {
    if (!bookmark_service_) {
      return;
    }

    const bool copy = event.source_operations() == ui::DragDropTypes::DRAG_COPY;
    output_drag_op =
        BookmarkUIOperationsHelperMergedSurfaces(bookmark_service_,
                                                 &drop_parent_)
            .DropBookmarks(
                browser_->GetProfile(), drop_data_, index_to_drop_at_, copy,
                chrome::BookmarkReorderDropTarget::kBookmarkMenu, browser_);
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
    bookmark_service_observation_.Reset();
    bookmark_service_ = nullptr;
  }

  const raw_ptr<BrowserWindowInterface> browser_;
  const bookmarks::BookmarkNodeData drop_data_;
  BookmarkParentFolder drop_parent_;
  const size_t index_to_drop_at_;
  raw_ptr<BookmarkMergedSurfaceService> bookmark_service_ = nullptr;
  base::ScopedObservation<BookmarkMergedSurfaceService,
                          BookmarkMergedSurfaceServiceObserver>
      bookmark_service_observation_{this};
};

}  // namespace

BookmarksDynamicMenu::BookmarkFolderOrURL::BookmarkFolderOrURL(
    const bookmarks::BookmarkNode* node)
    : folder_or_url_(GetFromNode(node)) {}

BookmarksDynamicMenu::BookmarkFolderOrURL::BookmarkFolderOrURL(
    const BookmarkParentFolder& folder)
    : folder_or_url_(folder) {}

BookmarksDynamicMenu::BookmarkFolderOrURL::~BookmarkFolderOrURL() = default;

BookmarksDynamicMenu::BookmarkFolderOrURL::BookmarkFolderOrURL(
    const BookmarkFolderOrURL& other) = default;

BookmarksDynamicMenu::BookmarkFolderOrURL&
BookmarksDynamicMenu::BookmarkFolderOrURL::operator=(
    const BookmarkFolderOrURL& other) = default;

const BookmarkParentFolder*
BookmarksDynamicMenu::BookmarkFolderOrURL::GetIfBookmarkFolder() const {
  if (folder_or_url_.index() == 0) {
    return &std::get<0>(folder_or_url_);
  }
  return nullptr;
}

const bookmarks::BookmarkNode*
BookmarksDynamicMenu::BookmarkFolderOrURL::GetIfBookmarkURL() const {
  if (folder_or_url_.index() == 0) {
    return nullptr;
  }
  return std::get<1>(folder_or_url_);
}

const bookmarks::BookmarkNode*
BookmarksDynamicMenu::BookmarkFolderOrURL::GetIfNonPermanentNode() const {
  const BookmarkParentFolder* folder = GetIfBookmarkFolder();
  if (folder && folder->as_permanent_folder().has_value()) {
    return nullptr;
  }
  return folder ? folder->as_non_permanent_folder() : GetIfBookmarkURL();
}

// static
std::variant<BookmarkParentFolder, raw_ptr<const bookmarks::BookmarkNode>>
BookmarksDynamicMenu::BookmarkFolderOrURL::GetFromNode(
    const bookmarks::BookmarkNode* node) {
  CHECK(node);
  if (node->is_url()) {
    return node;
  }
  return BookmarkParentFolder::FromFolderNode(node);
}

BookmarksDynamicMenu::BookmarksDynamicMenu(BrowserWindowInterface* browser,
                                           Host* host)
    : browser_window_interface_(browser), host_(host) {
  CHECK(browser_window_interface_);
  CHECK(host_);
}

BookmarksDynamicMenu::~BookmarksDynamicMenu() {
  if (dynamic_section_ && dynamic_section_->GetParent()) {
    dynamic_section_->GetParent()->SetProperty(
        AppMenuActionItem::kDragAndDropDelegateKey,
        static_cast<AppMenuDragAndDropDelegate*>(nullptr));
  }
}

void BookmarksDynamicMenu::BuildBookmarksActions(
    actions::BaseAction* parent_item) {
  CHECK(parent_item);
  bookmark_service_observation_.Reset();

  BookmarkMergedSurfaceService* service = GetBookmarkMergedSurfaceService();
  if (!service) {
    return;
  }

  if (dynamic_section_) {
    dynamic_section_->ResetActionList();
  }

  if (!dynamic_section_ || dynamic_section_->GetParent() != parent_item) {
    parent_item->SetProperty(AppMenuActionItem::kDragAndDropDelegateKey,
                             static_cast<AppMenuDragAndDropDelegate*>(this));
    parent_item->SetProperty(kBookmarkFolderOrURLKey,
                             std::make_unique<BookmarkFolderOrURL>(
                                 BookmarkParentFolder::BookmarkBarFolder()));
    actions::ActionItem* section = parent_item->AddChild(
        actions::ActionItem::Builder()
            .SetProperty(AppMenuActionItem::kDisplayTypeKey,
                         AppMenuActionItem::DisplayType::kSection)
            .Build());
    dynamic_section_ = section->GetAsWeakPtr();
  }

  if (!bookmark_service_observation_.IsObserving()) {
    bookmark_service_observation_.Observe(service);
  }

  if (!service->loaded()) {
    return;
  }

  BookmarkParentFolder managed_folder = BookmarkParentFolder::ManagedFolder();
  const bool has_managed = service->GetChildrenCount(managed_folder) > 0;
  BookmarkParentFolder bookmark_bar_folder =
      BookmarkParentFolder::BookmarkBarFolder();
  BookmarkParentFolderChildren bookmark_bar_children =
      service->GetChildren(bookmark_bar_folder);

  if (bookmark_bar_children.size() > 0 || has_managed) {
    dynamic_section_->AddChild(AppMenuActionItem::CreateDivider());
    dynamic_section_->AddChild(AppMenuActionItem::CreateHeader(
        l10n_util::GetStringUTF16(IDS_BOOKMARKS_LIST_TITLE)));

    if (has_managed) {
      AddBookmarkFolderAction(dynamic_section_.get(), managed_folder, service);
    }

    for (const auto* node : bookmark_bar_children) {
      AddBookmarkNodeAction(dynamic_section_.get(), node, service);
    }
  }

  BookmarkParentFolder other_folder = BookmarkParentFolder::OtherFolder();
  BookmarkParentFolder mobile_folder = BookmarkParentFolder::MobileFolder();
  const bool has_other = service->GetChildrenCount(other_folder) > 0;
  const bool has_mobile = service->GetChildrenCount(mobile_folder) > 0;

  if (has_other || has_mobile) {
    dynamic_section_->AddChild(AppMenuActionItem::CreateDivider());
    if (has_other) {
      AddBookmarkFolderAction(dynamic_section_.get(), other_folder, service);
    }
    if (has_mobile) {
      AddBookmarkFolderAction(dynamic_section_.get(), mobile_folder, service);
    }
  }
}

bool BookmarksDynamicMenu::GetDropFormats(
    actions::BaseAction* action,
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  *formats = ui::OSExchangeData::URL;
  format_types->insert(ui::ClipboardFormatType::BookmarkEntriesType());
  return true;
}

bool BookmarksDynamicMenu::AreDropTypesRequired(actions::BaseAction* action) {
  return true;
}

bool BookmarksDynamicMenu::CanDrop(actions::BaseAction* action,
                                   const ui::OSExchangeData& data) {
  const BookmarkFolderOrURL* target_node = FindNodeForAction(action);
  if (!target_node) {
    return false;
  }

  Profile* profile = browser_window_interface_->GetProfile();
  BookmarkMergedSurfaceService* service = GetBookmarkMergedSurfaceService();
  if (!profile || !service || !service->loaded() || !drop_data_.Read(data) ||
      drop_data_.size() != 1 ||
      !profile->GetPrefs()->GetBoolean(
          bookmarks::prefs::kEditBookmarksEnabled)) {
    return false;
  }

  if (drop_data_.has_single_url()) {
    return true;
  }

  const bookmarks::BookmarkNode* drag_node =
      drop_data_.GetFirstNode(service->bookmark_model(), profile->GetPath());
  if (!drag_node) {
    // Dragging a folder from another profile, always accept.
    return true;
  }

  // Drag originated from same profile and is not a URL. Only accept it if
  // the dragged node is not a parent of the node `action` represents.
  const bookmarks::BookmarkNode* non_permanent_drop_node =
      target_node->GetIfNonPermanentNode();
  if (!non_permanent_drop_node) {
    // Drop on permanent node.
    // `drag_node` can't be a permanent node or a root node.
    return true;
  }

  return !non_permanent_drop_node->HasAncestor(drag_node);
}

ui::mojom::DragOperation BookmarksDynamicMenu::GetDropOperation(
    actions::BaseAction* action,
    const ui::DropTargetEvent& event,
    views::MenuDelegate::DropPosition* position) {
  if (!drop_data_.is_valid()) {
    return ui::mojom::DragOperation::kNone;
  }

  std::optional<DropParams> drop_params = GetDropParams(action, position);
  if (!drop_params) {
    return ui::mojom::DragOperation::kNone;
  }
  return chrome::GetBookmarkDropOperation(
      browser_window_interface_->GetProfile(), event, drop_data_,
      drop_params->drop_parent, drop_params->index_to_drop_at);
}

views::View::DropCallback BookmarksDynamicMenu::GetDropCallback(
    actions::BaseAction* action,
    views::MenuDelegate::DropPosition position,
    const ui::DropTargetEvent& event) {
  std::optional<DropParams> drop_params = GetDropParams(action, &position);
  CHECK(drop_params);

  auto drop_observer = std::make_unique<BookmarkModelDropObserver>(
      browser_window_interface_, std::move(drop_data_),
      drop_params->drop_parent, drop_params->index_to_drop_at);
  return base::BindOnce(
      [](BookmarkModelDropObserver* drop_observer,
         const ui::DropTargetEvent& event,
         ui::mojom::DragOperation& output_drag_op,
         std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner) {
        drop_observer->Drop(event, output_drag_op);
      },
      base::Owned(std::move(drop_observer)));
}

bool BookmarksDynamicMenu::CanDrag(actions::BaseAction* action) {
  const BookmarkFolderOrURL* target = FindNodeForAction(action);
  // Don't let users drag permanent nodes (managed, other or mobile folder).
  return target && target->GetIfNonPermanentNode() != nullptr;
}

void BookmarksDynamicMenu::WriteDragData(actions::BaseAction* action,
                                         ui::OSExchangeData* data) {
  CHECK(action);
  CHECK(data);

  base::RecordAction(base::UserMetricsAction("BookmarkBar_DragFromFolder"));

  const BookmarkFolderOrURL* target = FindNodeForAction(action);
  CHECK(target);
  const bookmarks::BookmarkNode* node = target->GetIfNonPermanentNode();
  // Permanent nodes can't be dragged.
  CHECK(node);
  bookmarks::BookmarkNodeData drag_data(node);
  drag_data.Write(browser_window_interface_->GetProfile()->GetPath(), data);
}

int BookmarksDynamicMenu::GetDragOperations(actions::BaseAction* action) {
  const BookmarkFolderOrURL* target = FindNodeForAction(action);
  if (!target || !target->GetIfNonPermanentNode()) {
    return ui::DragDropTypes::DRAG_NONE;
  }
  return chrome::GetBookmarkDragOperation(
      browser_window_interface_->GetProfile(), target->GetIfNonPermanentNode());
}

void BookmarksDynamicMenu::BookmarkMergedSurfaceServiceLoaded() {
  if (dynamic_section_) {
    BuildBookmarksActions(dynamic_section_->GetParent());
  }
}

void BookmarksDynamicMenu::BookmarkMergedSurfaceServiceBeingDeleted() {
  bookmark_service_observation_.Reset();
}

void BookmarksDynamicMenu::BookmarkNodeAdded(const BookmarkParentFolder& parent,
                                             size_t index) {
  actions::BaseAction* target_parent_action = GetParentActionForFolder(parent);
  if (!target_parent_action) {
    return;
  }
  BookmarkMergedSurfaceService* service = GetBookmarkMergedSurfaceService();
  const bookmarks::BookmarkNode* node = service->GetNodeAtIndex(parent, index);
  actions::ActionItem* added_action =
      AddBookmarkNodeAction(target_parent_action, node, service);
  host_->UpdateMenuItem(added_action, target_parent_action,
                        GetInsertAfterAction(parent, index));
}

void BookmarksDynamicMenu::BookmarkNodesRemoved(
    const BookmarkParentFolder& parent,
    const base::flat_set<const bookmarks::BookmarkNode*>& nodes) {
  for (const bookmarks::BookmarkNode* node : nodes) {
    if (actions::ActionItem* action = FindActionForNode(node)) {
      host_->UpdateMenuItem(action, /*target_parent_action=*/nullptr);
    }
  }
}

void BookmarksDynamicMenu::BookmarkNodeMoved(
    const BookmarkParentFolder& old_parent,
    size_t old_index,
    const BookmarkParentFolder& new_parent,
    size_t new_index) {
  BookmarkMergedSurfaceService* service = GetBookmarkMergedSurfaceService();
  const bookmarks::BookmarkNode* moved_node =
      service->GetNodeAtIndex(new_parent, new_index);
  actions::ActionItem* action = FindActionForNode(moved_node);
  if (!action) {
    BookmarkNodeAdded(new_parent, new_index);
    return;
  }
  actions::BaseAction* target_parent_action =
      GetParentActionForFolder(new_parent);
  if (!target_parent_action) {
    host_->UpdateMenuItem(action, /*target_parent_action=*/nullptr);
    return;
  }
  host_->UpdateMenuItem(action, target_parent_action,
                        GetInsertAfterAction(new_parent, new_index));
}

void BookmarksDynamicMenu::BookmarkNodeChanged(
    const bookmarks::BookmarkNode* node) {
  if (actions::ActionItem* action = FindActionForNode(node)) {
    std::u16string title = node->GetTitle().empty()
                               ? base::UTF8ToUTF16(node->url().spec())
                               : node->GetTitle();
    action->SetText(title);
  }
}

void BookmarksDynamicMenu::BookmarkNodeFaviconChanged(
    const bookmarks::BookmarkNode* node) {
  if (actions::ActionItem* action = FindActionForNode(node)) {
    const gfx::Image& image =
        GetBookmarkMergedSurfaceService()->bookmark_model()->GetFavicon(node);
    action->SetImage(!image.IsEmpty() ? ui::ImageModel::FromImage(image)
                                      : favicon::GetDefaultFaviconModel());
  }
}

void BookmarksDynamicMenu::BookmarkParentFolderChildrenReordered(
    const BookmarkParentFolder& folder) {
  actions::BaseAction* target_parent_action = GetParentActionForFolder(folder);
  if (!target_parent_action) {
    return;
  }
  BookmarkParentFolderChildren children =
      GetBookmarkMergedSurfaceService()->GetChildren(folder);
  for (size_t i = 0; i < children.size(); ++i) {
    host_->UpdateMenuItem(FindActionForNode(children[i]), target_parent_action,
                          GetInsertAfterAction(folder, i));
  }
}

void BookmarksDynamicMenu::BookmarkAllUserNodesRemoved() {
  if (!dynamic_section_) {
    return;
  }
  std::vector<actions::ActionItem*> actions_to_remove;
  for (const auto& child : dynamic_section_->GetChildren().children()) {
    const BookmarkFolderOrURL* prop =
        child->GetProperty(kBookmarkFolderOrURLKey);
    if (!prop) {
      continue;
    }
    if (prop->GetIfNonPermanentNode()) {
      actions_to_remove.push_back(child->GetActionItem());
    } else if (*prop->GetIfBookmarkFolder() !=
               BookmarkParentFolder::ManagedFolder()) {
      for (const auto& folder_child : child->GetChildren().children()) {
        actions_to_remove.push_back(folder_child->GetActionItem());
      }
    }
  }
  for (actions::ActionItem* action : actions_to_remove) {
    host_->UpdateMenuItem(action, /*target_parent_action=*/nullptr);
  }
}

BookmarkMergedSurfaceService*
BookmarksDynamicMenu::GetBookmarkMergedSurfaceService() const {
  return BookmarkMergedSurfaceServiceFactory::GetForProfile(
      browser_window_interface_->GetProfile());
}

actions::ActionItem* BookmarksDynamicMenu::FindActionForTarget(
    const BookmarkFolderOrURL& target) const {
  if (!dynamic_section_) {
    return nullptr;
  }
  auto find_in = [&](this auto& self,
                     actions::BaseAction* parent) -> actions::ActionItem* {
    for (const auto& child : parent->GetChildren().children()) {
      if (const BookmarkFolderOrURL* prop =
              child->GetProperty(kBookmarkFolderOrURLKey);
          prop && *prop == target) {
        return child->GetActionItem();
      }
      if (actions::ActionItem* found = self(child.get())) {
        return found;
      }
    }
    return nullptr;
  };
  return find_in(dynamic_section_.get());
}

actions::ActionItem* BookmarksDynamicMenu::FindActionForNode(
    const bookmarks::BookmarkNode* node) const {
  return FindActionForTarget(BookmarkFolderOrURL(node));
}

const BookmarksDynamicMenu::BookmarkFolderOrURL*
BookmarksDynamicMenu::FindNodeForAction(actions::BaseAction* action) const {
  return action ? action->GetProperty(kBookmarkFolderOrURLKey) : nullptr;
}

actions::BaseAction* BookmarksDynamicMenu::GetParentActionForFolder(
    const BookmarkParentFolder& folder) const {
  if (folder == BookmarkParentFolder::BookmarkBarFolder()) {
    return dynamic_section_.get();
  }
  return FindActionForTarget(BookmarkFolderOrURL(folder));
}

actions::BaseAction* BookmarksDynamicMenu::GetInsertAfterAction(
    const BookmarkParentFolder& parent_folder,
    size_t index) const {
  if (index > 0) {
    const bookmarks::BookmarkNode* prev_node =
        GetBookmarkMergedSurfaceService()->GetNodeAtIndex(parent_folder,
                                                          index - 1);
    return FindActionForNode(prev_node);
  }

  if (parent_folder == BookmarkParentFolder::BookmarkBarFolder()) {
    for (const auto& child : dynamic_section_->GetChildren().children()) {
      if (child->GetActionItem()->GetProperty(
              AppMenuActionItem::kDisplayTypeKey) ==
          AppMenuActionItem::DisplayType::kHeader) {
        return child.get();
      }
    }
  }
  return nullptr;
}

actions::ActionItem* BookmarksDynamicMenu::AddBookmarkNodeAction(
    actions::BaseAction* parent_item,
    const bookmarks::BookmarkNode* node,
    BookmarkMergedSurfaceService* service) {
  if (node->is_folder()) {
    return AddBookmarkFolderAction(
        parent_item, BookmarkParentFolder::FromFolderNode(node), service);
  }
  CHECK(node->is_url());

  auto builder = actions::ActionItem::Builder();
  std::u16string title = node->GetTitle().empty()
                             ? base::UTF8ToUTF16(node->url().spec())
                             : node->GetTitle();
  builder.SetText(title);

  const gfx::Image& image = service->bookmark_model()->GetFavicon(node);
  builder.SetImage(!image.IsEmpty() ? ui::ImageModel::FromImage(image)
                                    : favicon::GetDefaultFaviconModel());

  builder.SetProperty(AppMenuActionItem::kContainerColorKey,
                      ui::kColorMenuBackground);

  int64_t node_id = node->id();
  builder.SetInvokeActionCallback(base::BindRepeating(
      [](BrowserWindowInterface* browser, int64_t node_id,
         actions::ActionItem* item, actions::ActionInvocationContext context) {
        RecordBookmarkLaunch(
            BookmarkLaunchLocation::kAppMenu,
            profile_metrics::GetBrowserProfileType(browser->GetProfile()));
        WindowOpenDisposition disposition =
            context.GetProperty(chrome::kDispositionKey);
        if (disposition == WindowOpenDisposition::UNKNOWN) {
          disposition = WindowOpenDisposition::CURRENT_TAB;
        }
        bookmarks::BookmarkModel* model =
            BookmarkModelFactory::GetForBrowserContext(browser->GetProfile());
        if (const bookmarks::BookmarkNode* bookmark_node =
                bookmarks::GetBookmarkNodeByID(model, node_id)) {
          bookmarks::OpenAllIfAllowed(browser, {bookmark_node}, disposition);
        }
      },
      browser_window_interface_, node_id));

  actions::ActionItem* added =
      parent_item->AddChild(std::move(builder).Build());
  added->SetProperty(kBookmarkFolderOrURLKey,
                     std::make_unique<BookmarkFolderOrURL>(node));
  return added;
}

actions::ActionItem* BookmarksDynamicMenu::AddBookmarkFolderAction(
    actions::BaseAction* parent_item,
    const BookmarkParentFolder& folder,
    BookmarkMergedSurfaceService* service) {
  BookmarkParentFolderChildren children = service->GetChildren(folder);
  CHECK(children.size() > 0 || folder.HoldsNonPermanentFolder());

  std::vector<const bookmarks::BookmarkNode*> underlying_nodes =
      service->GetUnderlyingNodes(folder);
  CHECK(!underlying_nodes.empty());

  const chrome::BookmarkFolderIconType folder_icon_type =
      (folder == BookmarkParentFolder::ManagedFolder())
          ? chrome::BookmarkFolderIconType::kManaged
          : chrome::BookmarkFolderIconType::kNormal;

  auto builder = actions::ActionItem::Builder();
  builder.SetText(underlying_nodes[0]->GetTitle())
      .SetImage(
          chrome::GetBookmarkFolderIcon(folder_icon_type, ui::kColorMenuIcon))
      .SetProperty(AppMenuActionItem::kContainerColorKey,
                   ui::kColorMenuBackground)
      .SetProperty(AppMenuActionItem::kIsSubmenuKey, true);
  auto folder_action = std::move(builder).Build();

  for (const auto* child : children) {
    AddBookmarkNodeAction(folder_action.get(), child, service);
  }

  actions::ActionItem* added = parent_item->AddChild(std::move(folder_action));
  added->SetProperty(kBookmarkFolderOrURLKey,
                     std::make_unique<BookmarkFolderOrURL>(folder));
  return added;
}

bool BookmarksDynamicMenu::IsDropValid(
    const BookmarkFolderOrURL* target,
    const views::MenuDelegate::DropPosition* position) const {
  CHECK(target);
  const BookmarkParentFolder* target_folder = target->GetIfBookmarkFolder();
  const bool drop_on_url_node = !target_folder;
  switch (*position) {
    case views::MenuDelegate::DropPosition::kUnknow:
    case views::MenuDelegate::DropPosition::kNone:
      return false;

    case views::MenuDelegate::DropPosition::kBefore:
      return drop_on_url_node || target_folder->HoldsNonPermanentFolder() ||
             target_folder->as_permanent_folder() ==
                 PermanentFolderType::kOtherNode;

    case views::MenuDelegate::DropPosition::kAfter:
      return drop_on_url_node || target_folder->HoldsNonPermanentFolder() ||
             target_folder->as_permanent_folder() ==
                 PermanentFolderType::kManagedNode;

    case views::MenuDelegate::DropPosition::kOn:
      return !drop_on_url_node;
  }
  NOTREACHED();
}

std::optional<BookmarksDynamicMenu::DropParams>
BookmarksDynamicMenu::GetDropParams(
    actions::BaseAction* action,
    views::MenuDelegate::DropPosition* position) const {
  const BookmarkFolderOrURL* drop_node = FindNodeForAction(action);
  if (!drop_node || !IsDropValid(drop_node, position)) {
    return std::nullopt;
  }

  const BookmarkParentFolder* drop_folder = drop_node->GetIfBookmarkFolder();
  // Initial params drop on bookmark bar.
  DropParams drop_params{BookmarkParentFolder::BookmarkBarFolder(), 0};
  const BookmarkMergedSurfaceService* service =
      GetBookmarkMergedSurfaceService();

  switch (*position) {
    case views::MenuDelegate::DropPosition::kAfter:
      if (drop_folder && drop_folder->as_permanent_folder() ==
                             PermanentFolderType::kManagedNode) {
        // Managed folder is shown at the top of the bookmarks menu.
        // Use initial params for `drop_params` with the parent as the bookmark
        // bar and the index is 0.
        CHECK_EQ(*drop_params.drop_parent.as_permanent_folder(),
                 PermanentFolderType::kBookmarkBarNode);
      } else {
        // Drop after a URL or non permanent node.
        const bookmarks::BookmarkNode* node =
            drop_node->GetIfNonPermanentNode();
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
        CHECK_EQ(*drop_params.drop_parent.as_permanent_folder(),
                 PermanentFolderType::kBookmarkBarNode);
        drop_params.index_to_drop_at =
            service->GetChildrenCount(drop_params.drop_parent);
      } else {
        // Drop before a URL or non permanent node.
        const bookmarks::BookmarkNode* node =
            drop_node->GetIfNonPermanentNode();
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
