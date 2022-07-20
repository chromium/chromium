// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/bookmarks/bookmarks_page_handler.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_context_menu_controller.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/side_panel/bookmarks/bookmarks_side_panel_ui.h"
#include "chrome/browser/ui/webui/side_panel/reading_list/reading_list_ui.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"
#include "ui/base/window_open_disposition.h"

namespace {

class BookmarkContextMenu : public ui::SimpleMenuModel,
                            public ui::SimpleMenuModel::Delegate,
                            public BookmarkContextMenuControllerDelegate {
 public:
  explicit BookmarkContextMenu(
      Browser* browser,
      base::WeakPtr<ui::MojoBubbleWebUIController::Embedder> embedder,
      const bookmarks::BookmarkNode* bookmark)
      : ui::SimpleMenuModel(this),
        embedder_(embedder),
        controller_(base::WrapUnique(new BookmarkContextMenuController(
            browser->window()->GetNativeWindow(),
            this,
            browser,
            browser->profile(),
            base::BindRepeating(
                [](content::PageNavigator* navigator) { return navigator; },
                browser),
            BOOKMARK_LAUNCH_LOCATION_SIDE_PANEL_CONTEXT_MENU,
            bookmark->parent(),
            {bookmark}))) {
    AddItem(IDC_BOOKMARK_BAR_OPEN_ALL);
    AddItem(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW);
    AddItem(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO);
    AddSeparator(ui::NORMAL_SEPARATOR);

    AddItem(bookmark->is_folder() ? IDC_BOOKMARK_BAR_RENAME_FOLDER
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
    controller_->ExecuteCommand(command_id, event_flags);
  }

  bool IsCommandIdEnabled(int command_id) const override {
    return controller_->IsCommandIdEnabled(command_id);
  }

  bool IsCommandIdVisible(int command_id) const override {
    return controller_->IsCommandIdVisible(command_id);
  }

  // BookmarkContextMenuControllerDelegate:
  void CloseMenu() override { embedder_->HideContextMenu(); }

 private:
  void AddItem(int command_id) {
    ui::SimpleMenuModel::AddItem(
        command_id,
        controller_->menu_model()->GetLabelAt(
            controller_->menu_model()->GetIndexOfCommandId(command_id)));
  }
  base::WeakPtr<ui::MojoBubbleWebUIController::Embedder> embedder_;
  std::unique_ptr<BookmarkContextMenuController> controller_;
};

}  // namespace

BookmarksPageHandler::BookmarksPageHandler(
    mojo::PendingReceiver<side_panel::mojom::BookmarksPageHandler> receiver,
    BookmarksSidePanelUI* bookmarks_ui)
    : receiver_(this, std::move(receiver)), bookmarks_ui_(bookmarks_ui) {}

BookmarksPageHandler::BookmarksPageHandler(
    mojo::PendingReceiver<side_panel::mojom::BookmarksPageHandler> receiver,
    ReadingListUI* reading_list_ui)
    : receiver_(this, std::move(receiver)), reading_list_ui_(reading_list_ui) {}

BookmarksPageHandler::~BookmarksPageHandler() = default;

void BookmarksPageHandler::OpenBookmark(
    int64_t node_id,
    int32_t parent_folder_depth,
    ui::mojom::ClickModifiersPtr click_modifiers) {
  Browser* browser = chrome::FindLastActive();
  if (!browser)
    return;

  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser->profile());
  const bookmarks::BookmarkNode* bookmark_node =
      bookmarks::GetBookmarkNodeByID(bookmark_model, node_id);
  if (!bookmark_node)
    return;

  WindowOpenDisposition open_location = ui::DispositionFromClick(
      click_modifiers->middle_button, click_modifiers->alt_key,
      click_modifiers->ctrl_key, click_modifiers->meta_key,
      click_modifiers->shift_key);
  content::OpenURLParams params(bookmark_node->url(), content::Referrer(),
                                open_location,
                                ui::PAGE_TRANSITION_AUTO_BOOKMARK, false);
  browser->OpenURL(params);
  base::RecordAction(base::UserMetricsAction("SidePanel.Bookmarks.Navigation"));
  RecordBookmarkLaunch(
      parent_folder_depth > 0 ? BOOKMARK_LAUNCH_LOCATION_SIDE_PANEL_SUBFOLDER
                              : BOOKMARK_LAUNCH_LOCATION_SIDE_PANEL_FOLDER,
      profile_metrics::GetBrowserProfileType(browser->profile()));
}

void BookmarksPageHandler::ShowContextMenu(const std::string& id_string,
                                           const gfx::Point& point) {
  int64_t id;
  if (!base::StringToInt64(id_string, &id))
    return;

  Browser* browser = chrome::FindLastActive();
  if (!browser)
    return;

  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser->profile());
  const bookmarks::BookmarkNode* bookmark =
      bookmarks::GetBookmarkNodeByID(bookmark_model, id);
  if (!bookmark)
    return;

  auto embedder =
      bookmarks_ui_ ? bookmarks_ui_->embedder() : reading_list_ui_->embedder();
  if (embedder) {
    embedder->ShowContextMenu(point, std::make_unique<BookmarkContextMenu>(
                                         browser, embedder, bookmark));
  }
}

void BookmarksPageHandler::ShowUI() {
  auto embedder = bookmarks_ui_->embedder();
  if (embedder) {
    embedder->ShowUI();
  }
}
