// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/bookmarks/bookmarks_page_handler.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_parent_folder_children.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_context_menu_controller.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/bookmarks/bookmark_prefs.h"
#include "chrome/browser/ui/webui/commerce/shopping_list_context_menu_controller.h"
#include "chrome/browser/ui/webui/side_panel/bookmarks/bookmarks.mojom.h"
#include "chrome/browser/ui/webui/side_panel/bookmarks/bookmarks_side_panel_ui.h"
#include "chrome/browser/ui/webui/side_panel/reading_list/reading_list_ui.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/prefs/pref_service.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/strings/grit/components_strings.h"
#include "mojo/public/cpp/bindings/message.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/menus/simple_menu_model.h"

namespace {

class BookmarkContextMenu : public ui::SimpleMenuModel,
                            public ui::SimpleMenuModel::Delegate,
                            public BookmarkContextMenuControllerDelegate {
 public:
  explicit BookmarkContextMenu(
      Browser* browser,
      base::WeakPtr<TopChromeWebUIController::Embedder> embedder,
      std::vector<raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>
          bookmarks,
      const side_panel::mojom::ActionSource& source,
      commerce::ShoppingListContextMenuController* shopping_list_controller)
      : ui::SimpleMenuModel(this),
        embedder_(embedder),
        controller_(base::WrapUnique(new BookmarkContextMenuController(
            browser->window()->GetNativeWindow(),
            this,
            browser,
            browser->profile(),
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
    AddSeparator(ui::NORMAL_SEPARATOR);

    AddItem(bookmarks.size() == 1 && bookmarks.front()->is_folder()
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
    commerce::ShoppingListContextMenuController* shopping_list_controller) {
  Browser* browser = chrome::FindLastActive();
  if (!browser) {
    return nullptr;
  }

  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser->profile());
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
                                 browser, embedder, bookmarks, source,
                                 shopping_list_controller);
}

// Temporary helper function for `GetPermanentFolderSidePanelID`.
std::string GetPermanentFolderStringId(
    const bookmarks::BookmarkNode* permanent_node) {
  if (!permanent_node) {
    return "-1";
  }

  CHECK(permanent_node->is_permanent_node());
  return base::ToString(permanent_node->id());
}

// TODO(crbug.com/380806881): Currently returns the local node to align with the
// resources passed in `BookmarksSidePanelUI` and to keep all operations on
// local nodes work as intended. When the migration of all parent node id
// computation support the merged node, then a neutral fixed Id can be used to
// represent the permanent merged folders.
std::string GetPermanentFolderSidePanelID(
    const BookmarkMergedSurfaceService& bookmark_merged_surface,
    BookmarkParentFolder::PermanentFolderType folder_type) {
  const bookmarks::BookmarkModel* bookmark_model =
      bookmark_merged_surface.bookmark_model();
  CHECK(bookmark_model);

  switch (folder_type) {
    case BookmarkParentFolder::PermanentFolderType::kBookmarkBarNode:
      return GetPermanentFolderStringId(bookmark_model->bookmark_bar_node());
    case BookmarkParentFolder::PermanentFolderType::kOtherNode:
      return GetPermanentFolderStringId(bookmark_model->other_node());
    case BookmarkParentFolder::PermanentFolderType::kMobileNode:
      return GetPermanentFolderStringId(bookmark_model->mobile_node());
    case BookmarkParentFolder::PermanentFolderType::kManagedNode:
      const bookmarks::ManagedBookmarkService* managed_bookmark_service =
          bookmark_merged_surface.managed_bookmark_service();
      const bookmarks::BookmarkNode* managed_node =
          managed_bookmark_service ? managed_bookmark_service->managed_node()
                                   : nullptr;
      return GetPermanentFolderStringId(managed_node);
  }
}

// Returns the correct ID of the folder used in Ui. Can either return itself if
// it is a regular folder, or return the merged Id if it is a permanent folder
// (that are merged in the Ui).
std::string GetFolderSidePanelID(
    const BookmarkMergedSurfaceService& bookmark_merged_surface,
    const BookmarkParentFolder& folder) {
  std::optional<BookmarkParentFolder::PermanentFolderType> folder_type =
      folder.as_permanent_folder();
  if (folder_type.has_value()) {
    return GetPermanentFolderSidePanelID(bookmark_merged_surface,
                                         folder_type.value());
  }

  return base::ToString(folder.as_non_permanent_folder()->id());
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
  mojo_node->parent_id = GetFolderSidePanelID(bookmark_merged_surface, parent);
  mojo_node->index = bookmark_merged_surface.GetIndexOf(node);
  mojo_node->date_added = node->date_added().InSecondsFSinceUnixEpoch();
  mojo_node->date_last_used = node->date_last_used().InSecondsFSinceUnixEpoch();
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
    BookmarkMergedSurfaceService* bookmark_merged_surface)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      bookmarks_ui_(bookmarks_ui),
      bookmark_merged_surface_(bookmark_merged_surface) {
  CHECK(bookmark_merged_surface_);
  Browser* browser = chrome::FindLastActive();
  if (!browser) {
    return;
  }

  scoped_bookmark_merged_service_observation_.Observe(bookmark_merged_surface_);
}

BookmarksPageHandler::~BookmarksPageHandler() = default;

void BookmarksPageHandler::BookmarkCurrentTabInFolder(int64_t folder_id) {
  Browser* browser = chrome::FindLastActive();
  if (!browser) {
    return;
  }

  chrome::BookmarkCurrentTabInFolder(browser, folder_id);
}

void BookmarksPageHandler::ExecuteOpenInNewTabCommand(
    const std::vector<int64_t>& node_ids,
    side_panel::mojom::ActionSource source) {
  ExecuteContextMenuCommand(node_ids, source, IDC_BOOKMARK_BAR_OPEN_ALL);
}

void BookmarksPageHandler::ExecuteOpenInNewWindowCommand(
    const std::vector<int64_t>& node_ids,
    side_panel::mojom::ActionSource source) {
  ExecuteContextMenuCommand(node_ids, source,
                            IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW);
}

void BookmarksPageHandler::ExecuteOpenInIncognitoWindowCommand(
    const std::vector<int64_t>& node_ids,
    side_panel::mojom::ActionSource source) {
  ExecuteContextMenuCommand(node_ids, source,
                            IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO);
}

void BookmarksPageHandler::ExecuteOpenInNewTabGroupCommand(
    const std::vector<int64_t>& node_ids,
    side_panel::mojom::ActionSource source) {
  ExecuteContextMenuCommand(node_ids, source,
                            IDC_BOOKMARK_BAR_OPEN_ALL_NEW_TAB_GROUP);
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
      bookmarks_ui_->GetShoppingListContextMenuController());
  if (context_menu && context_menu->IsCommandIdEnabled(command_id)) {
    context_menu->ExecuteCommand(command_id, 0);
  }
}

void BookmarksPageHandler::OpenBookmark(
    int64_t node_id,
    int32_t parent_folder_depth,
    ui::mojom::ClickModifiersPtr click_modifiers,
    side_panel::mojom::ActionSource source) {
  Browser* browser = chrome::FindLastActive();
  if (!browser) {
    return;
  }

  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser->profile());
  const bookmarks::BookmarkNode* bookmark_node =
      bookmarks::GetBookmarkNodeByID(bookmark_model, node_id);
  if (!bookmark_node) {
    return;
  }

  WindowOpenDisposition open_location = ui::DispositionFromClick(
      click_modifiers->middle_button, click_modifiers->alt_key,
      click_modifiers->ctrl_key, click_modifiers->meta_key,
      click_modifiers->shift_key);
  chrome::OpenAllIfAllowed(browser, {bookmark_node}, open_location, false);
  if (source == side_panel::mojom::ActionSource::kPriceTracking) {
    return;
  }
  base::RecordAction(base::UserMetricsAction("SidePanel.Bookmarks.Navigation"));
  RecordBookmarkLaunch(
      parent_folder_depth > 0 ? BookmarkLaunchLocation::kSidePanelSubfolder
                              : BookmarkLaunchLocation::kSidePanelFolder,
      profile_metrics::GetBrowserProfileType(browser->profile()));
}

void BookmarksPageHandler::SetSortOrder(
    side_panel::mojom::SortOrder sort_order) {
  Browser* browser = chrome::FindLastActive();
  if (!browser) {
    return;
  }

  PrefService* pref_service = browser->profile()->GetPrefs();
  if (pref_service) {
    pref_service->SetInteger(bookmarks_webui::prefs::kBookmarksSortOrder,
                             static_cast<int>(sort_order));
  }
}

void BookmarksPageHandler::SetViewType(side_panel::mojom::ViewType view_type) {
  Browser* browser = chrome::FindLastActive();
  if (!browser) {
    return;
  }

  PrefService* pref_service = browser->profile()->GetPrefs();
  if (pref_service) {
    pref_service->SetInteger(bookmarks_webui::prefs::kBookmarksViewType,
                             static_cast<int>(view_type));
  }
}

void BookmarksPageHandler::ShowContextMenu(
    const std::string& id_string,
    const gfx::Point& point,
    side_panel::mojom::ActionSource source) {
  int64_t id;
  if (!base::StringToInt64(id_string, &id)) {
    return;
  }

  auto embedder = bookmarks_ui_->embedder();
  if (embedder) {
    std::unique_ptr<BookmarkContextMenu> context_menu = ContextMenuFromNodes(
        {id}, embedder, source,
        bookmarks_ui_->GetShoppingListContextMenuController());
    if (context_menu) {
      embedder->ShowContextMenu(point, std::move(context_menu));
    }
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

    // TODO(crbug.com/380806881): Temporarily relies on the fact that the last
    // node is the local one, because the Ui does assumptions on the local
    // permanent IDs to perform some special operations on the permanent
    // folders.
    // This allows existing usages of local node manipulation to keep working
    // properly during the transition of all the calls.
    // Once the migration of all the calls to the merged mapping computation is
    // done, we would be able to introduce a new Side Panel ID system for merged
    // permanent folders IDs to stop relying on the assumption that the merged
    // Id is the local Id node.
    const bookmarks::BookmarkNode* local_node = underlying_nodes.back();

    side_panel::mojom::BookmarksTreeNodePtr mojo_node =
        side_panel::mojom::BookmarksTreeNode::New();
    mojo_node->title = base::UTF16ToUTF8(local_node->GetTitle());
    std::optional<BookmarkParentFolder::PermanentFolderType> folder_type =
        folder.as_permanent_folder();
    CHECK(folder_type.has_value());
    mojo_node->id = GetPermanentFolderSidePanelID(*bookmark_merged_surface_,
                                                  folder_type.value());
    mojo_node->parent_id = base::ToString(local_node->parent()->id());
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

std::string GetFolderSidePanelIDForTesting(
    const BookmarkMergedSurfaceService& bookmark_merged_surface,
    const BookmarkParentFolder& folder) {
  return GetFolderSidePanelID(bookmark_merged_surface, folder);
}
