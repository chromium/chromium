// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/bookmarks/bookmarks_page_handler.h"

#include <algorithm>
#include <iterator>
#include <optional>

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_parent_folder.h"
#include "chrome/browser/bookmarks/bookmark_parent_folder_children.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/extensions/api/bookmark_manager_private/bookmark_manager_private_api.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_context_menu_controller.h"
#include "chrome/browser/ui/bookmarks/bookmark_drag_drop.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/bookmarks/bookmark_ui_operations_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/webui/bookmarks/bookmark_prefs.h"
#include "chrome/browser/ui/webui/commerce/shopping_list_context_menu_controller.h"
#include "chrome/browser/ui/webui/side_panel/bookmarks/bookmarks.mojom.h"
#include "chrome/browser/ui/webui/side_panel/bookmarks/bookmarks_side_panel_ui.h"
#include "chrome/browser/ui/webui/side_panel/reading_list/reading_list_ui.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/undo/bookmark_undo_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/browser/scoped_group_bookmark_actions.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/prefs/pref_service.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/strings/grit/components_strings.h"
#include "components/undo/bookmark_undo_service.h"
#include "mojo/public/cpp/bindings/message.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

class BookmarkContextMenu : public ui::SimpleMenuModel,
                            public ui::SimpleMenuModel::Delegate,
                            public BookmarkContextMenuControllerDelegate {
 public:
  explicit BookmarkContextMenu(
      BrowserWindowInterface* browser_window,
      base::WeakPtr<TopChromeWebUIController::Embedder> embedder,
      std::vector<raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>
          bookmarks,
      const side_panel::mojom::ActionSource& source,
      commerce::ShoppingListContextMenuController* shopping_list_controller)
      : ui::SimpleMenuModel(this),
        embedder_(embedder),
        controller_(base::WrapUnique(new BookmarkContextMenuController(
            BrowserElementsViews::From(browser_window)
                ->GetPrimaryWindowWidget()
                ->GetNativeWindow(),
            this,
            browser_window->GetBrowserForMigrationOnly(),
            browser_window->GetProfile(),
            BookmarkLaunchLocation::kSidePanelContextMenu,
            bookmarks))),
        shopping_list_controller_(shopping_list_controller),
        bookmarks_(bookmarks) {
    if (bookmarks.size() == 0) {
      mojo::ReportBadMessage("BookmarkContextMenu has empty bookmarks");
      return;
    }
    if (source == side_panel::mojom::ActionSource::kPriceTracking) {
      DCHECK(shopping_list_controller_);
      AddItem(IDC_BOOKMARK_BAR_OPEN_ALL);
      AddItem(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW);
      AddItem(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO);
      if (bookmarks.size() == 1 && bookmarks.front()->is_url() &&
          base::FeatureList::IsEnabled(features::kSideBySide)) {
        AddItem(IDC_BOOKMARK_BAR_OPEN_SPLIT_VIEW);
      }
      AddSeparator(ui::NORMAL_SEPARATOR);
      shopping_list_controller_->AddPriceTrackingItemForBookmark(
          this, bookmarks.front());
      AddSeparator(ui::NORMAL_SEPARATOR);
      AddItem(IDC_BOOKMARK_MANAGER);
      return;
    }

    AddItem(IDC_BOOKMARK_BAR_OPEN_ALL);
    AddItem(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW);
    AddItem(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO);
    if (bookmarks.size() == 1 && bookmarks.front()->is_url() &&
        base::FeatureList::IsEnabled(features::kSideBySide)) {
      AddItem(IDC_BOOKMARK_BAR_OPEN_SPLIT_VIEW);
    }
    AddSeparator(ui::NORMAL_SEPARATOR);

    AddItem((bookmarks.size() == 1 && bookmarks.front()->is_folder()) ||
                    IsSelectionPermanentBookmarkFolder(bookmarks)
                ? IDC_BOOKMARK_BAR_RENAME_FOLDER
                : IDC_BOOKMARK_BAR_EDIT);
    AddSeparator(ui::NORMAL_SEPARATOR);

    AddItem(IDC_CUT);
    AddItem(IDC_COPY);
    AddItem(IDC_PASTE);
    AddSeparator(ui::NORMAL_SEPARATOR);

    AddItem(IDC_BOOKMARK_BAR_REMOVE);
    AddSeparator(ui::NORMAL_SEPARATOR);

    AddItem(IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK);
    AddItem(IDC_BOOKMARK_BAR_NEW_FOLDER);
    AddSeparator(ui::NORMAL_SEPARATOR);

    AddItem(IDC_BOOKMARK_MANAGER);
  }
  ~BookmarkContextMenu() override = default;

  void ExecuteCommand(int command_id, int event_flags) override {
    if (shopping_list_controller_ && shopping_list_controller_->ExecuteCommand(
                                         command_id, bookmarks_.front())) {
      return;
    }
    controller_->ExecuteCommand(command_id, event_flags);
  }

  bool IsCommandIdEnabled(int command_id) const override {
    return controller_->IsCommandIdEnabled(command_id);
  }

  bool IsCommandIdVisible(int command_id) const override {
    return controller_->IsCommandIdVisible(command_id);
  }

  // BookmarkContextMenuControllerDelegate:
  void CloseMenu() override {
    if (embedder_) {
      embedder_->HideContextMenu();
    }
  }

 private:
  void AddItem(int command_id) {
    ui::SimpleMenuModel::AddItem(command_id,
                                 controller_->menu_model()->GetLabelAt(
                                     controller_->menu_model()
                                         ->GetIndexOfCommandId(command_id)
                                         .value()));
  }
  base::WeakPtr<TopChromeWebUIController::Embedder> embedder_;
  std::unique_ptr<BookmarkContextMenuController> controller_;
  raw_ptr<commerce::ShoppingListContextMenuController>
      shopping_list_controller_;
  std::vector<raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>
      bookmarks_;
};

std::unique_ptr<BookmarkContextMenu> ContextMenuFromNodes(
    const std::vector<int64_t> node_ids,
    base::WeakPtr<TopChromeWebUIController::Embedder> embedder,
    side_panel::mojom::ActionSource source,
    commerce::ShoppingListContextMenuController* shopping_list_controller,
    BrowserWindowInterface* browser_window) {
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser_window->GetProfile());
  std::vector<raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>
      bookmarks = {};
  for (const int64_t id : node_ids) {
    const bookmarks::BookmarkNode* bookmark =
        bookmarks::GetBookmarkNodeByID(bookmark_model, id);
    if (bookmark) {
      bookmarks.push_back(bookmark);
    }
  }

  return bookmarks.empty() ? nullptr
                           : std::make_unique<BookmarkContextMenu>(
                                 browser_window, embedder, bookmarks, source,
                                 shopping_list_controller);
}

// Returns the Side Panel merged ID for permanent folders.
std::string GetPermanentFolderSidePanelID(
    BookmarkParentFolder::PermanentFolderType folder_type) {
  switch (folder_type) {
    case BookmarkParentFolder::PermanentFolderType::kBookmarkBarNode:
      return kSidePanelBookmarkBarID;
    case BookmarkParentFolder::PermanentFolderType::kOtherNode:
      return kSidePanelOtherBookmarksID;
    case BookmarkParentFolder::PermanentFolderType::kMobileNode:
      return kSidePanelMobileBookmarksID;
    case BookmarkParentFolder::PermanentFolderType::kManagedNode:
      return kSidePanelManagedBookmarksID;
  }
}

// Returns the correct ID of the folder used in Ui. Can either return itself if
// it is a regular folder, or return the merged Id if it is a permanent folder
// (that are merged in the Ui).
std::string GetFolderSidePanelID(const BookmarkParentFolder& folder) {
  std::optional<BookmarkParentFolder::PermanentFolderType> folder_type =
      folder.as_permanent_folder();
  if (folder_type.has_value()) {
    return GetPermanentFolderSidePanelID(folder_type.value());
  }

  return base::ToString(folder.as_non_permanent_folder()->id());
}

// Returns `std::nullopt` if `side_panel_id` does not correspond to a permanent
// node special ID.
std::optional<BookmarkParentFolder> GetBookmarkSidePanelPermanentParentFolder(
    const std::string& side_panel_id) {
  if (side_panel_id == kSidePanelBookmarkBarID) {
    return BookmarkParentFolder::BookmarkBarFolder();
  }
  if (side_panel_id == kSidePanelOtherBookmarksID) {
    return BookmarkParentFolder::OtherFolder();
  }
  if (side_panel_id == kSidePanelMobileBookmarksID) {
    return BookmarkParentFolder::MobileFolder();
  }
  if (side_panel_id == kSidePanelManagedBookmarksID) {
    return BookmarkParentFolder::ManagedFolder();
  }

  // `side_panel_id` is not a side panel special permanent ID.
  return std::nullopt;
}

// Given a list of side panel string IDs, returns the equivalent bookmark
// int64_t id. If one of those string IDs map to a permanent special side panel
// ID, then the underlying ID(s) are returned, that maps to one or two permanent
// nodes.
// If any of the IDs do not match a permanent Side Panel IDs or can be cast to
// an int64_t value, then the input is considered faulty and an empty vector is
// returned.
// The `side_panel_ids` should not be empty.
std::vector<int64_t> GetBookmarkIDsFromSidePanelIDs(
    const BookmarkMergedSurfaceService& bookmark_merged_surface,
    const std::vector<std::string>& side_panel_ids) {
  CHECK(!side_panel_ids.empty());
  std::vector<int64_t> node_ids;
  for (const std::string& side_panel_id : side_panel_ids) {
    // Side Panel Permanent ID treatment:
    std::optional<BookmarkParentFolder> side_panel_permanent_parent_folder =
        GetBookmarkSidePanelPermanentParentFolder(side_panel_id);
    // If one of the IDs is a permanent ID, we need to extract the underlying
    // permanent nodes.
    if (side_panel_permanent_parent_folder.has_value()) {
      for (const bookmarks::BookmarkNode* permanent_node :
           bookmark_merged_surface.GetUnderlyingNodes(
               side_panel_permanent_parent_folder.value())) {
        CHECK(permanent_node->is_permanent_node());
        node_ids.push_back(permanent_node->id());
      }
      continue;
    }

    // Regular ID treatment:
    int64_t converted_id = 0;
    // Conversion check validity.
    if (!base::StringToInt64(side_panel_id, &converted_id)) {
      if (mojo::IsInMessageDispatch()) {
        mojo::ReportBadMessage(
            "Unsupported conversion: side_panel_id should either be a "
            "permanent merged node ID or represent an int64 id value");
      }
      // Early return in case one of the nodes is not valid.
      return {};
    }
    node_ids.push_back(converted_id);
  }

  return node_ids;
}

// Will return `std::nullopt` if `side_panel_id` does not correspond to a
// permanent node special ID or is not an actual int64 id. Invalid inputs should
// result in no-ops.
std::optional<BookmarkParentFolder> GetBookmarkParentFolderFromSidePanel(
    const BookmarkMergedSurfaceService& bookmark_merged_surface,
    const std::string& side_panel_id) {
  // Permanent folders have a special string ID.
  std::optional<BookmarkParentFolder> side_panel_permanent_parent_folder =
      GetBookmarkSidePanelPermanentParentFolder(side_panel_id);
  if (side_panel_permanent_parent_folder.has_value()) {
    return side_panel_permanent_parent_folder.value();
  }

  // A regular folder should have a valid int64 node ID.
  int64_t folder_id = 0;
  // Conversion check validity.
  if (!base::StringToInt64(side_panel_id, &folder_id)) {
    if (mojo::IsInMessageDispatch()) {
      mojo::ReportBadMessage(
          "Unsupported conversion: side_panel_id should either be a permanent "
          "merged node ID or represent an int64 id value");
    }
    return std::nullopt;
  }

  return BookmarkParentFolder::FromFolderNode(bookmarks::GetBookmarkNodeByID(
      bookmark_merged_surface.bookmark_model(), folder_id));
}

std::vector<side_panel::mojom::BookmarksTreeNodePtr> ConstructMojoChildNodes(
    const BookmarkMergedSurfaceService& bookmark_merged_surface,
    const BookmarkParentFolder& parent,
    const BookmarkParentFolderChildren& children);

side_panel::mojom::BookmarksTreeNodePtr ConstructMojoNode(
    const BookmarkMergedSurfaceService& bookmark_merged_surface,
    const BookmarkParentFolder& parent,
    const bookmarks::BookmarkNode* node,
    bool with_children) {
  side_panel::mojom::BookmarksTreeNodePtr mojo_node =
      side_panel::mojom::BookmarksTreeNode::New();
  mojo_node->title = base::UTF16ToUTF8(node->GetTitle());
  mojo_node->id = base::ToString(node->id());
  mojo_node->parent_id = GetFolderSidePanelID(parent);
  mojo_node->index = bookmark_merged_surface.GetIndexOf(node);
  mojo_node->date_added = node->date_added().InSecondsFSinceUnixEpoch();
  mojo_node->date_last_used = node->date_last_used().InSecondsFSinceUnixEpoch();
  mojo_node->unmodifiable = bookmarks::IsDescendantOf(
      node, bookmark_merged_surface.managed_bookmark_service()->managed_node());
  if (node->is_folder()) {
    if (with_children) {
      const BookmarkParentFolder& sub_parent =
          BookmarkParentFolder::FromFolderNode(node);
      mojo_node->children = ConstructMojoChildNodes(
          bookmark_merged_surface, sub_parent,
          bookmark_merged_surface.GetChildren(sub_parent));
    }
  } else {
    mojo_node->url = node->url().spec();
  }

  return mojo_node;
}

std::vector<side_panel::mojom::BookmarksTreeNodePtr> ConstructMojoChildNodes(
    const BookmarkMergedSurfaceService& bookmark_merged_surface,
    const BookmarkParentFolder& parent,
    const BookmarkParentFolderChildren& children) {
  std::vector<side_panel::mojom::BookmarksTreeNodePtr> mojo_nodes;
  for (const bookmarks::BookmarkNode* node : children) {
    mojo_nodes.push_back(ConstructMojoNode(bookmark_merged_surface, parent,
                                           node, /*with_children=*/true));
  }
  return mojo_nodes;
}

}  // namespace

BookmarksPageHandler::BookmarksPageHandler(
    mojo::PendingReceiver<side_panel::mojom::BookmarksPageHandler> receiver,
    mojo::PendingRemote<side_panel::mojom::BookmarksPage> page,
    BookmarksSidePanelUI* bookmarks_ui,
    content::WebUI* web_ui)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      web_ui_(web_ui),
      bookmarks_ui_(bookmarks_ui),
      bookmark_merged_surface_(
          BookmarkMergedSurfaceServiceFactory::GetForProfile(
              Profile::FromWebUI(web_ui_))),
      browser_window_interface_(
          webui::GetBrowserWindowInterface(web_ui_->GetWebContents())) {
  CHECK(bookmark_merged_surface_);
  scoped_bookmark_merged_service_observation_.Observe(bookmark_merged_surface_);
}

BookmarksPageHandler::~BookmarksPageHandler() = default;

void BookmarksPageHandler::BookmarkCurrentTabInFolder(
    const std::string& folder_id) {
  std::optional<BookmarkParentFolder> parent =
      GetBookmarkParentFolderFromSidePanel(*bookmark_merged_surface_,
                                           folder_id);
  if (!parent) {
    return;
  }
  chrome::BookmarkCurrentTabInFolder(
      browser_window_interface_->GetBrowserForMigrationOnly(),
      bookmark_merged_surface_->bookmark_model(),
      bookmark_merged_surface_->GetDefaultParentForNewNodes(*parent)->id());
}

void BookmarksPageHandler::CreateFolder(const std::string& folder_id,
                                        const std::string& title,
                                        CreateFolderCallback callback) {
  std::optional<BookmarkParentFolder> parent =
      GetBookmarkParentFolderFromSidePanel(*bookmark_merged_surface_,
                                           folder_id);
  if (!parent) {
    std::move(callback).Run("");
    return;
  }
  const bookmarks::BookmarkNode* parent_node =
      bookmark_merged_surface_->GetDefaultParentForNewNodes(*parent);

  bookmarks::BookmarkModel* model = bookmark_merged_surface_->bookmark_model();
  const bookmarks::BookmarkNode* new_folder =
      model->AddFolder(parent_node, /*index=*/0, base::UTF8ToUTF16(title));
  model->SetDateFolderModified(parent_node, base::Time::Now());

  std::move(callback).Run(base::ToString(new_folder->id()));
}

void BookmarksPageHandler::DropBookmarks(const std::string& folder_id,
                                         DropBookmarksCallback callback) {
  base::ScopedClosureRunner closure_runner(std::move(callback));

  if (!bookmarks_ui_) {
    return;
  }

  // Do not continue if editing bookmarks is not allowed.
  if (!browser_window_interface_->GetProfile()->GetPrefs()->GetBoolean(
          bookmarks::prefs::kEditBookmarksEnabled)) {
    return;
  }

  std::optional<BookmarkParentFolder> parent =
      GetBookmarkParentFolderFromSidePanel(*bookmark_merged_surface_,
                                           folder_id);
  if (!parent) {
    return;
  }

  // Do not allow a drop in a managed folder.
  if (bookmark_merged_surface_->IsParentFolderManaged(parent.value())) {
    return;
  }

  const bookmarks::BookmarkNode* parent_node =
      bookmark_merged_surface_->GetDefaultParentForNewNodes(parent.value());

  const base::FilePath destination_profile_path =
      browser_window_interface_->GetProfile()->GetPath();
  content::WebContents* side_panel_web_contents = web_ui_->GetWebContents();
  CHECK(side_panel_web_contents);

  const bookmarks::BookmarkNodeData* drag_data =
      extensions::BookmarkManagerPrivateDragEventRouter::FromWebContents(
          side_panel_web_contents)
          ->GetBookmarkNodeData();
  CHECK(drag_data);
  CHECK(drag_data->is_valid());

  // The following checks are only necessary when moving data within the same
  // profile.
  if (drag_data->IsFromProfilePath(destination_profile_path)) {
    for (const auto& node :
         drag_data->GetNodes(bookmark_merged_surface_->bookmark_model(),
                             destination_profile_path)) {
      // Abort if we are trying to move a node into one of its descendants.
      // This can also happen from other surfaces, e.g. when dragging a folder
      // from the bookmark manager into one of its children in the side panel.
      // Also do not continue if any of the dropped nodes are managed.
      // TODO(crbug.com/409283807): Dropping into descendants should be blocked
      // by the UI instead.
      // TODO(crbug.com/409284055): Instead of doing a no-op, perform a copy
      // when dropping managed nodes.
      if (parent_node->HasAncestor(node) ||
          bookmark_merged_surface_->IsNodeManaged(node)) {
        return;
      }
    }
  }

  BookmarkUIOperationsHelperMergedSurfaces(bookmark_merged_surface_,
                                           &parent.value())
      .DropBookmarks(browser_window_interface_->GetProfile(), *drag_data,
                     /*index=*/parent_node->children().size(),
                     /*copy=*/false,
                     chrome::BookmarkReorderDropTarget::kBookmarkSidePanel,
                     browser_window_interface_->GetBrowserForMigrationOnly());
}

void BookmarksPageHandler::ExecuteOpenInNewTabCommand(
    const std::vector<std::string>& side_panel_ids,
    side_panel::mojom::ActionSource source) {
  const std::vector<int64_t> node_ids =
      GetBookmarkIDsFromSidePanelIDs(*bookmark_merged_surface_, side_panel_ids);
  if (node_ids.empty()) {
    return;
  }
  ExecuteContextMenuCommand(node_ids, source, IDC_BOOKMARK_BAR_OPEN_ALL);
}

void BookmarksPageHandler::ExecuteOpenInNewWindowCommand(
    const std::vector<std::string>& side_panel_ids,
    side_panel::mojom::ActionSource source) {
  const std::vector<int64_t> node_ids =
      GetBookmarkIDsFromSidePanelIDs(*bookmark_merged_surface_, side_panel_ids);
  if (node_ids.empty()) {
    return;
  }
  ExecuteContextMenuCommand(node_ids, source,
                            IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW);
}

void BookmarksPageHandler::ExecuteOpenInIncognitoWindowCommand(
    const std::vector<std::string>& side_panel_ids,
    side_panel::mojom::ActionSource source) {
  const std::vector<int64_t> node_ids =
      GetBookmarkIDsFromSidePanelIDs(*bookmark_merged_surface_, side_panel_ids);
  if (node_ids.empty()) {
    return;
  }
  ExecuteContextMenuCommand(node_ids, source,
                            IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO);
}

void BookmarksPageHandler::ExecuteOpenInNewTabGroupCommand(
    const std::vector<std::string>& side_panel_ids,
    side_panel::mojom::ActionSource source) {
  const std::vector<int64_t> node_ids =
      GetBookmarkIDsFromSidePanelIDs(*bookmark_merged_surface_, side_panel_ids);
  if (node_ids.empty()) {
    return;
  }
  ExecuteContextMenuCommand(node_ids, source,
                            IDC_BOOKMARK_BAR_OPEN_ALL_NEW_TAB_GROUP);
}

void BookmarksPageHandler::ExecuteOpenInSplitViewCommand(
    const std::vector<int64_t>& node_ids,
    side_panel::mojom::ActionSource source) {
  CHECK(base::FeatureList::IsEnabled(features::kSideBySide));
  ExecuteContextMenuCommand(node_ids, source, IDC_BOOKMARK_BAR_OPEN_SPLIT_VIEW);
}

void BookmarksPageHandler::ExecuteEditCommand(
    const std::vector<int64_t>& node_ids,
    side_panel::mojom::ActionSource source) {
  ExecuteContextMenuCommand(node_ids, source, IDC_BOOKMARK_BAR_EDIT);
}

void BookmarksPageHandler::ExecuteMoveCommand(
    const std::vector<int64_t>& node_ids,
    side_panel::mojom::ActionSource source) {
  ExecuteContextMenuCommand(node_ids, source, IDC_BOOKMARK_BAR_MOVE);
}

void BookmarksPageHandler::ExecuteAddToBookmarksBarCommand(
    const int64_t node_id,
    side_panel::mojom::ActionSource source) {
  ExecuteContextMenuCommand({node_id}, source,
                            IDC_BOOKMARK_BAR_ADD_TO_BOOKMARKS_BAR);
}

void BookmarksPageHandler::ExecuteRemoveFromBookmarksBarCommand(
    int64_t node_id,
    side_panel::mojom::ActionSource source) {
  ExecuteContextMenuCommand({node_id}, source,
                            IDC_BOOKMARK_BAR_REMOVE_FROM_BOOKMARKS_BAR);
}

void BookmarksPageHandler::ExecuteDeleteCommand(
    const std::vector<int64_t>& node_ids,
    side_panel::mojom::ActionSource source) {
  ExecuteContextMenuCommand(node_ids, source, IDC_BOOKMARK_BAR_REMOVE);
}

void BookmarksPageHandler::ExecuteContextMenuCommand(
    const std::vector<int64_t>& node_ids,
    side_panel::mojom::ActionSource source,
    int command_id) {
  std::unique_ptr<BookmarkContextMenu> context_menu = ContextMenuFromNodes(
      node_ids, bookmarks_ui_->embedder(), source,
      bookmarks_ui_->GetShoppingListContextMenuController(),
      browser_window_interface_);
  if (context_menu->IsCommandIdEnabled(command_id)) {
    context_menu->ExecuteCommand(command_id, 0);
  }
}

void BookmarksPageHandler::OpenBookmark(
    int64_t node_id,
    int32_t parent_folder_depth,
    ui::mojom::ClickModifiersPtr click_modifiers,
    side_panel::mojom::ActionSource source) {
  const bookmarks::BookmarkNode* bookmark_node = bookmarks::GetBookmarkNodeByID(
      bookmark_merged_surface_->bookmark_model(), node_id);
  if (!bookmark_node) {
    return;
  }

  WindowOpenDisposition open_location = ui::DispositionFromClick(
      click_modifiers->middle_button, click_modifiers->alt_key,
      click_modifiers->ctrl_key, click_modifiers->meta_key,
      click_modifiers->shift_key);
  bookmarks::OpenAllIfAllowed(
      browser_window_interface_->GetBrowserForMigrationOnly(), {bookmark_node},
      open_location);
  if (source == side_panel::mojom::ActionSource::kPriceTracking) {
    return;
  }
  base::RecordAction(base::UserMetricsAction("SidePanel.Bookmarks.Navigation"));
  RecordBookmarkLaunch(parent_folder_depth > 0
                           ? BookmarkLaunchLocation::kSidePanelSubfolder
                           : BookmarkLaunchLocation::kSidePanelFolder,
                       profile_metrics::GetBrowserProfileType(
                           browser_window_interface_->GetProfile()));
}

void BookmarksPageHandler::Undo() {
  BookmarkUndoServiceFactory::GetForProfile(
      browser_window_interface_->GetProfile())
      ->undo_manager()
      ->Undo();
}

void BookmarksPageHandler::RenameBookmark(int64_t node_id,
                                          const std::string& new_title) {
  bookmarks::BookmarkModel* model = bookmark_merged_surface_->bookmark_model();
  const bookmarks::BookmarkNode* node_to_rename =
      bookmarks::GetBookmarkNodeByID(model, node_id);
  if (!node_to_rename) {
    return;
  }

  // Using extensions metrics recording as this action was previously done
  // through the extensions API.
  model->SetTitle(node_to_rename, base::UTF8ToUTF16(new_title),
                  bookmarks::metrics::BookmarkEditSource::kExtension);
}

void BookmarksPageHandler::MoveBookmark(int64_t node_id,
                                        const std::string& folder_id) {
  std::optional<BookmarkParentFolder> parent =
      GetBookmarkParentFolderFromSidePanel(*bookmark_merged_surface_,
                                           folder_id);
  if (!parent) {
    return;
  }

  const bookmarks::BookmarkNode* node_to_move = bookmarks::GetBookmarkNodeByID(
      bookmark_merged_surface_->bookmark_model(), node_id);
  bookmark_merged_surface_->Move(
      node_to_move, *parent,
      bookmark_merged_surface_->GetChildrenCount(*parent),
      browser_window_interface_->GetBrowserForMigrationOnly());
}

void BookmarksPageHandler::RemoveBookmarks(const std::vector<int64_t>& node_ids,
                                           RemoveBookmarksCallback callback) {
  bookmarks::BookmarkModel* model = bookmark_merged_surface_->bookmark_model();
  bookmarks::ScopedGroupBookmarkActions group_deletes(model);
  for (int64_t node_id : node_ids) {
    const bookmarks::BookmarkNode* node_to_remove =
        bookmarks::GetBookmarkNodeByID(model, node_id);
    // TODO(crbug.com/407986687): Investigate if this is the correct behavior.
    // This fixes the issue when `node_ids` contain both parent and children at
    // the same time. This is possible when using the compact view with tree
    // view. Undo may not be working as expected.
    if (!node_to_remove) {
      continue;
    }
    // Using extensions metrics recording as this action was previously done
    // through the extensions API.
    model->Remove(node_to_remove,
                  bookmarks::metrics::BookmarkEditSource::kExtension,
                  FROM_HERE);
  }

  std::move(callback).Run();
}

void BookmarksPageHandler::SetSortOrder(
    side_panel::mojom::SortOrder sort_order) {
  PrefService* pref_service =
      browser_window_interface_->GetProfile()->GetPrefs();
  if (pref_service) {
    pref_service->SetInteger(bookmarks_webui::prefs::kBookmarksSortOrder,
                             static_cast<int>(sort_order));
  }
}

void BookmarksPageHandler::SetViewType(side_panel::mojom::ViewType view_type) {
  PrefService* pref_service =
      browser_window_interface_->GetProfile()->GetPrefs();
  if (pref_service) {
    pref_service->SetInteger(bookmarks_webui::prefs::kBookmarksViewType,
                             static_cast<int>(view_type));
  }
}

void BookmarksPageHandler::ShowContextMenu(
    const std::string& id_string,
    const gfx::Point& point,
    side_panel::mojom::ActionSource source) {
  int64_t id = 0;
  if (!base::StringToInt64(id_string, &id)) {
    return;
  }

  auto embedder = bookmarks_ui_->embedder();
  if (embedder) {
    std::unique_ptr<BookmarkContextMenu> context_menu = ContextMenuFromNodes(
        {id}, embedder, source,
        bookmarks_ui_->GetShoppingListContextMenuController(),
        browser_window_interface_);
    embedder->ShowContextMenu(point, std::move(context_menu));
  }
}

void BookmarksPageHandler::ShowUI() {
  auto embedder = bookmarks_ui_->embedder();
  if (embedder) {
    embedder->ShowUI();
  }
}

void BookmarksPageHandler::BookmarkMergedSurfaceServiceLoaded() {
  if (get_all_bookmarks_callback_) {
    SendAllBookmarks(std::move(get_all_bookmarks_callback_));
  }
}

void BookmarksPageHandler::GetAllBookmarks(GetAllBookmarksCallback callback) {
  if (!bookmark_merged_surface_->loaded()) {
    // Temporarily store the resulting callback to be executed upon bookmarks
    // load.
    get_all_bookmarks_callback_ = std::move(callback);
    return;
  }

  SendAllBookmarks(std::move(callback));
}

void BookmarksPageHandler::SendAllBookmarks(GetAllBookmarksCallback callback) {
  CHECK(bookmark_merged_surface_->loaded());

  const std::vector<BookmarkParentFolder> permanent_folders{
      BookmarkParentFolder::BookmarkBarFolder(),
      BookmarkParentFolder::OtherFolder(),
      BookmarkParentFolder::MobileFolder(),
      BookmarkParentFolder::ManagedFolder(),
  };

  std::vector<side_panel::mojom::BookmarksTreeNodePtr> mojo_nodes;
  int permanent_folder_side_panel_index = 0;
  // Prepare all non empty permanent nodes using a merged view.
  for (const BookmarkParentFolder& folder : permanent_folders) {
    const auto& underlying_nodes =
        bookmark_merged_surface_->GetUnderlyingNodes(folder);
    CHECK(!underlying_nodes.empty());

    // TODO(crbug.com/401176689): This causes the Mobile/ManagedBookmark folders
    // not to be pushed if empty. Upon sign in/sync event, if the Mobile
    // Bookmarks gets populated while the SidePanel is showing, the added node
    // would not find its parent in the displayed nodes.
    // This would apply to all folders that are not showing visible if empty.
    bool any_visible = std::ranges::any_of(
        underlying_nodes,
        [](const bookmarks::BookmarkNode* node) { return node->IsVisible(); });
    if (!any_visible) {
      continue;
    }

    side_panel::mojom::BookmarksTreeNodePtr mojo_node =
        side_panel::mojom::BookmarksTreeNode::New();
    mojo_node->title = base::UTF16ToUTF8(underlying_nodes.back()->GetTitle());
    std::optional<BookmarkParentFolder::PermanentFolderType> folder_type =
        folder.as_permanent_folder();
    CHECK(folder_type.has_value());
    mojo_node->id = GetPermanentFolderSidePanelID(folder_type.value());
    mojo_node->parent_id = kSidePanelRootBookmarkID;
    mojo_node->index = permanent_folder_side_panel_index++;
    mojo_node->children =
        ConstructMojoChildNodes(*bookmark_merged_surface_, folder,
                                bookmark_merged_surface_->GetChildren(folder));

    // Given that this node represents a merge of two folders, we compute the
    // max of the nodes for each attribute.
    mojo_node->date_added =
        (*std::max_element(underlying_nodes.begin(), underlying_nodes.end(),
                           [](const bookmarks::BookmarkNode* first,
                              const bookmarks::BookmarkNode* second) {
                             return first->date_added() > second->date_added();
                           }))
            ->date_added()
            .InSecondsFSinceUnixEpoch();
    mojo_node->date_last_used =
        (*std::max_element(underlying_nodes.begin(), underlying_nodes.end(),
                           [](const bookmarks::BookmarkNode* first,
                              const bookmarks::BookmarkNode* second) {
                             return first->date_last_used() >
                                    second->date_last_used();
                           }))
            ->date_last_used()
            .InSecondsFSinceUnixEpoch();
    mojo_node->unmodifiable = folder == BookmarkParentFolder::ManagedFolder();

    mojo_nodes.push_back(std::move(mojo_node));
  }

  std::move(callback).Run(std::move(mojo_nodes));
}

void BookmarksPageHandler::BookmarkNodeAdded(const BookmarkParentFolder& parent,
                                             size_t index) {
  const bookmarks::BookmarkNode* added_node =
      bookmark_merged_surface_->GetNodeAtIndex(parent, index);
  // `with_children` false here because `BookmarkNodeAdded` will be called for
  // every child node as well.
  page_->OnBookmarkNodeAdded(ConstructMojoNode(
      *bookmark_merged_surface_, parent, added_node, /*with_children=*/false));
}

void BookmarksPageHandler::BookmarkNodesRemoved(
    const BookmarkParentFolder& parent,
    const base::flat_set<const bookmarks::BookmarkNode*>& nodes) {
  std::vector<std::string> mojo_node_ids;
  std::ranges::transform(nodes, std::back_inserter(mojo_node_ids),
                         [](const bookmarks::BookmarkNode* node) {
                           return base::ToString(node->id());
                         });
  page_->OnBookmarkNodesRemoved(std::move(mojo_node_ids));
}

void BookmarksPageHandler::BookmarkParentFolderChildrenReordered(
    const BookmarkParentFolder& folder) {
  std::vector<std::string> mojo_children_ordered_ids;
  for (const bookmarks::BookmarkNode* child :
       bookmark_merged_surface_->GetChildren(folder)) {
    mojo_children_ordered_ids.push_back(base::ToString(child->id()));
  }
  page_->OnBookmarkParentFolderChildrenReordered(GetFolderSidePanelID(folder),
                                                 mojo_children_ordered_ids);
}

void BookmarksPageHandler::BookmarkNodeMoved(
    const BookmarkParentFolder& old_parent,
    size_t old_index,
    const BookmarkParentFolder& new_parent,
    size_t new_index) {
  page_->OnBookmarkNodeMoved(GetFolderSidePanelID(old_parent), old_index,
                             GetFolderSidePanelID(new_parent), new_index);
}

void BookmarksPageHandler::BookmarkNodeChanged(
    const bookmarks::BookmarkNode* node) {
  page_->OnBookmarkNodeChanged(base::ToString(node->id()),
                               base::UTF16ToUTF8(node->GetTitle()),
                               node->is_url() ? node->url().spec() : "");
}

std::string GetFolderSidePanelIDForTesting(
    const BookmarkParentFolder& folder) {
  return GetFolderSidePanelID(folder);
}
