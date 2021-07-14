// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/read_later/side_panel/bookmarks_page_handler.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/read_later/read_later_ui.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/models/simple_menu_model.h"

namespace {

class BookmarkContextMenu : public ui::SimpleMenuModel,
                            public ui::SimpleMenuModel::Delegate {
 public:
  explicit BookmarkContextMenu(Browser* browser,
                               bookmarks::BookmarkModel* bookmark_model,
                               const bookmarks::BookmarkNode* bookmark)
      : ui::SimpleMenuModel(this),
        browser_(browser),
        bookmark_model_(bookmark_model),
        bookmark_(bookmark) {
    AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB,
                        IDS_BOOKMARK_BAR_OPEN_IN_NEW_TAB);
    AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW,
                        IDS_BOOKMARK_BAR_OPEN_IN_NEW_WINDOW);
    AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD,
                        IDS_BOOKMARK_BAR_OPEN_INCOGNITO);
    AddSeparator(ui::NORMAL_SEPARATOR);

    AddItemWithStringId(IDC_BOOKMARK_BAR_EDIT, IDS_BOOKMARK_BAR_EDIT);
    AddItemWithStringId(IDC_BOOKMARK_BAR_REMOVE, IDS_BOOKMARK_BAR_REMOVE);
    AddSeparator(ui::NORMAL_SEPARATOR);

    AddItemWithStringId(IDC_BOOKMARK_MANAGER, IDS_BOOKMARK_MANAGER);
  }
  ~BookmarkContextMenu() override = default;

  void ExecuteCommand(int command_id, int event_flags) override {
    switch (command_id) {
      case IDC_CONTENT_CONTEXT_OPENLINKNEWTAB: {
        content::OpenURLParams params(bookmark_->url(), content::Referrer(),
                                      WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                      ui::PAGE_TRANSITION_AUTO_BOOKMARK, false);
        browser_->OpenURL(params);
        break;
      }

      case IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW: {
        content::OpenURLParams params(bookmark_->url(), content::Referrer(),
                                      WindowOpenDisposition::NEW_WINDOW,
                                      ui::PAGE_TRANSITION_AUTO_BOOKMARK, false);
        browser_->OpenURL(params);
        break;
      }

      case IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD: {
        content::OpenURLParams params(bookmark_->url(), content::Referrer(),
                                      WindowOpenDisposition::OFF_THE_RECORD,
                                      ui::PAGE_TRANSITION_AUTO_BOOKMARK, false);
        browser_->OpenURL(params);
        break;
      }

      case IDC_BOOKMARK_BAR_EDIT:
        BookmarkEditor::Show(browser_->window()->GetNativeWindow(),
                             browser_->profile(),
                             BookmarkEditor::EditDetails::EditNode(bookmark_),
                             BookmarkEditor::SHOW_TREE);
        break;

      case IDC_BOOKMARK_BAR_REMOVE:
        bookmark_model_->Remove(bookmark_);
        break;

      case IDC_BOOKMARK_MANAGER:
        if (bookmark_->parent()) {
          chrome::ShowBookmarkManagerForNode(browser_,
                                             bookmark_->parent()->id());
        } else {
          chrome::ShowBookmarkManager(browser_);
        }
        break;

      default:
        NOTREACHED();
        break;
    }
  }

  bool IsCommandIdEnabled(int command_id) const override {
    PrefService* prefs = browser_->profile()->GetPrefs();
    switch (command_id) {
      case IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD: {
        IncognitoModePrefs::Availability incognito_pref =
            IncognitoModePrefs::GetAvailability(prefs);
        return !browser_->profile()->IsOffTheRecord() &&
               incognito_pref != IncognitoModePrefs::DISABLED &&
               IsURLAllowedInIncognito(bookmark_->url(), browser_->profile());
      }

      case IDC_BOOKMARK_BAR_EDIT:
      case IDC_BOOKMARK_BAR_REMOVE: {
        return prefs->GetBoolean(bookmarks::prefs::kEditBookmarksEnabled) &&
               bookmark_model_->client()->CanBeEditedByUser(bookmark_);
      }
    }

    return true;
  }

 private:
  Browser* const browser_;
  bookmarks::BookmarkModel* bookmark_model_;
  const bookmarks::BookmarkNode* bookmark_;
};

}  // namespace

BookmarksPageHandler::BookmarksPageHandler(
    mojo::PendingReceiver<side_panel::mojom::BookmarksPageHandler> receiver,
    ReadLaterUI* read_later_ui)
    : receiver_(this, std::move(receiver)), read_later_ui_(read_later_ui) {}

BookmarksPageHandler::~BookmarksPageHandler() = default;

void BookmarksPageHandler::OpenBookmark(const GURL& url,
                                        int32_t parent_folder_depth) {
  Browser* browser = chrome::FindLastActive();
  if (!browser)
    return;

  content::OpenURLParams params(url, content::Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
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
  auto embedder = read_later_ui_->embedder();
  if (embedder) {
    embedder->ShowContextMenu(point, std::make_unique<BookmarkContextMenu>(
                                         browser, bookmark_model, bookmark));
  }
}
