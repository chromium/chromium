// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/reading_list/reading_list_ui.h"

#include <string>
#include <utility>

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/read_later/reading_list_model_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/sanitized_image_source.h"
#include "chrome/browser/ui/webui/side_panel/bookmarks/bookmarks_page_handler.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_page_handler.h"
#include "chrome/browser/ui/webui/side_panel/reading_list/reading_list_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/side_panel_resources.h"
#include "chrome/grit/side_panel_resources_map.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/webui/shopping_list_handler.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/prefs/pref_service.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/views/style/platform_style.h"

ReadingListUI::ReadingListUI(content::WebUI* web_ui)
    : ui::MojoBubbleWebUIController(web_ui),
      webui_load_timer_(web_ui->GetWebContents(),
                        "ReadingList.WebUI.LoadDocumentTime",
                        "ReadingList.WebUI.LoadCompletedTime") {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIReadLaterHost);
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"addCurrentTab", IDS_READ_LATER_ADD_CURRENT_TAB},
      {"bookmarksTabTitle", IDS_BOOKMARK_MANAGER_TITLE},
      {"bookmarkCopied", IDS_BOOKMARK_MANAGER_TOAST_ITEM_COPIED},
      {"bookmarkDeleted", IDS_BOOKMARK_MANAGER_TOAST_ITEM_DELETED},
      {"bookmarkCreated", IDS_BOOKMARK_SCREEN_READER_CREATED},
      {"bookmarkReordered", IDS_BOOKMARK_SCREEN_READER_REORDERED},
      {"bookmarkMoved", IDS_BOOKMARK_SCREEN_READER_MOVED},
      {"emptyStateAddFromDialogSubheader",
       IDS_READ_LATER_MENU_EMPTY_STATE_ADD_FROM_DIALOG_SUBHEADER},
      {"emptyStateHeader", IDS_READ_LATER_MENU_EMPTY_STATE_HEADER},
      {"emptyStateSubheader", IDS_READ_LATER_MENU_EMPTY_STATE_SUBHEADER},
      {"markCurrentTabAsRead", IDS_READ_LATER_MARK_CURRENT_TAB_READ},
      {"readAnythingTabTitle", IDS_READ_ANYTHING_TITLE},
      {"readHeader", IDS_READ_LATER_MENU_READ_HEADER},
      {"title", IDS_READ_LATER_TITLE},
      {"sidePanelTitle", IDS_SIDE_PANEL_TITLE},
      {"tooltipClose", IDS_CLOSE},
      {"tooltipDelete", IDS_DELETE},
      {"tooltipMarkAsRead", IDS_READ_LATER_MENU_TOOLTIP_MARK_AS_READ},
      {"tooltipMarkAsUnread", IDS_READ_LATER_MENU_TOOLTIP_MARK_AS_UNREAD},
      {"unreadHeader", IDS_READ_LATER_MENU_UNREAD_HEADER},
      {"shoppingListFolderTitle", IDS_SIDE_PANEL_TRACKED_PRODUCTS},
      {"shoppingListTrackPriceButtonDescription",
       IDS_PRICE_TRACKING_TRACK_PRODUCT_ACCESSIBILITY},
      {"shoppingListUntrackPriceButtonDescription",
       IDS_PRICE_TRACKING_UNTRACK_PRODUCT_ACCESSIBILITY},
      {"sortByType", IDS_BOOKMARKS_SORT_BY_TYPE},
      {"allBookmarks", IDS_BOOKMARKS_ALL_BOOKMARKS},
      {"priceTrackingLabel", IDS_BOOKMARKS_LABEL_TRACKED_PRODUCTS},
      {"sortNewest", IDS_BOOKMARKS_SORT_NEWEST},
      {"sortOldest", IDS_BOOKMARKS_SORT_OLDEST},
      {"sortAlphabetically", IDS_BOOKMARKS_SORT_ALPHABETICALLY},
      {"sortReverseAlphabetically", IDS_BOOKMARKS_SORT_REVERSE_ALPHABETICALLY},
      {"visualView", IDS_BOOKMARKS_VISUAL_VIEW},
      {"compactView", IDS_BOOKMARKS_COMPACT_VIEW},
      {"sortMenuA11yLabel", IDS_BOOKMARKS_SORT_MENU_A11Y_LABEL},
      {"createNewFolderA11yLabel", IDS_BOOKMARKS_CREATE_NEW_FOLDER_A11Y_LABEL},
      {"editBookmarkListA11yLabel",
       IDS_BOOKMARKS_EDIT_BOOKMARK_LIST_A11Y_LABEL},
      {"cancelA11yLabel", IDS_CANCEL},
      {"emptyTitle", IDS_BOOKMARKS_EMPTY_STATE_TITLE},
      {"emptyBody", IDS_BOOKMARKS_EMPTY_STATE_BODY},
      {"emptyTitleGuest", IDS_BOOKMARKS_EMPTY_STATE_TITLE_GUEST},
      {"emptyBodyGuest", IDS_BOOKMARKS_EMPTY_STATE_BODY_GUEST},
      {"searchBookmarks", IDS_BOOKMARK_MANAGER_SEARCH_BUTTON},
      {"clearSearch", IDS_BOOKMARK_MANAGER_CLEAR_SEARCH},
      {"selectedBookmarkCount", IDS_BOOKMARK_MANAGER_ITEMS_SELECTED},
      {"menuOpenNewTab", IDS_BOOKMARK_MANAGER_MENU_OPEN_IN_NEW_TAB},
  };
  for (const auto& str : kLocalizedStrings)
    webui::AddLocalizedString(source, str.name, str.id);

  source->AddBoolean("useRipples", views::PlatformStyle::kUseRipples);

  Profile* const profile = Profile::FromWebUI(web_ui);
  PrefService* prefs = profile->GetPrefs();
  source->AddBoolean(
      "bookmarksDragAndDropEnabled",
      prefs->GetBoolean(bookmarks::prefs::kEditBookmarksEnabled));

  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  source->AddString(
      "otherBookmarksId",
      base::NumberToString(bookmark_model ? bookmark_model->other_node()->id()
                                          : -1));

  ReadingListModel* const reading_list_model =
      ReadingListModelFactory::GetForBrowserContext(profile);
  source->AddBoolean(
      "hasUnseenReadingListEntries",
      reading_list_model->loaded() ? reading_list_model->unseen_size() : false);

  source->AddBoolean("readAnythingEnabled", features::IsReadAnythingEnabled());
  source->AddBoolean("unifiedSidePanel",
                     base::FeatureList::IsEnabled(features::kUnifiedSidePanel));

  source->AddBoolean("guestMode", profile->IsGuestSession());

  source->AddBoolean(
      "showPowerBookmarks",
      base::FeatureList::IsEnabled(features::kPowerBookmarksSidePanel));

  bool shouldShowBookmark =
      prefs->GetBoolean(prefs::kShouldShowSidePanelBookmarkTab);
  source->AddBoolean("shouldShowBookmark", shouldShowBookmark);
  if (shouldShowBookmark) {
    prefs->SetBoolean(prefs::kShouldShowSidePanelBookmarkTab, false);
  }

  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));
  const int resource =
      !base::FeatureList::IsEnabled(features::kUnifiedSidePanel)
          ? IDR_SIDE_PANEL_SIDE_PANEL_HTML
          : IDR_SIDE_PANEL_READING_LIST_READING_LIST_HTML;
  webui::SetupWebUIDataSource(
      source, base::make_span(kSidePanelResources, kSidePanelResourcesSize),
      resource);
  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                source);
  content::URLDataSource::Add(profile,
                              std::make_unique<SanitizedImageSource>(profile));
}

ReadingListUI::~ReadingListUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(ReadingListUI)

void ReadingListUI::BindInterface(
    mojo::PendingReceiver<reading_list::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void ReadingListUI::CreatePageHandler(
    mojo::PendingRemote<reading_list::mojom::Page> page,
    mojo::PendingReceiver<reading_list::mojom::PageHandler> receiver) {
  DCHECK(page);
  page_handler_ = std::make_unique<ReadingListPageHandler>(
      std::move(receiver), std::move(page), this, web_ui());
}

void ReadingListUI::BindInterface(
    mojo::PendingReceiver<side_panel::mojom::BookmarksPageHandlerFactory>
        receiver) {
  bookmarks_page_factory_receiver_.reset();
  bookmarks_page_factory_receiver_.Bind(std::move(receiver));
}

void ReadingListUI::CreateBookmarksPageHandler(
    mojo::PendingReceiver<side_panel::mojom::BookmarksPageHandler> receiver) {
  bookmarks_page_handler_ =
      std::make_unique<BookmarksPageHandler>(std::move(receiver), this);
}

void ReadingListUI::BindInterface(
    mojo::PendingReceiver<read_anything::mojom::PageHandlerFactory> receiver) {
  read_anything_page_factory_receiver_.reset();
  read_anything_page_factory_receiver_.Bind(std::move(receiver));
}

void ReadingListUI::CreatePageHandler(
    mojo::PendingRemote<read_anything::mojom::Page> page,
    mojo::PendingReceiver<read_anything::mojom::PageHandler> receiver) {
  DCHECK(page);
  read_anything_page_handler_ = std::make_unique<ReadAnythingPageHandler>(
      std::move(page), std::move(receiver), web_ui());
}

void ReadingListUI::BindInterface(
    mojo::PendingReceiver<shopping_list::mojom::ShoppingListHandlerFactory>
        receiver) {
  shopping_list_factory_receiver_.reset();
  shopping_list_factory_receiver_.Bind(std::move(receiver));
}

void ReadingListUI::BindInterface(
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandlerFactory>
        pending_receiver) {
  if (help_bubble_handler_factory_receiver_.is_bound())
    help_bubble_handler_factory_receiver_.reset();
  help_bubble_handler_factory_receiver_.Bind(std::move(pending_receiver));
}

void ReadingListUI::CreateHelpBubbleHandler(
    mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> client,
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler> handler) {
  help_bubble_handler_ = std::make_unique<user_education::HelpBubbleHandler>(
      std::move(handler), std::move(client), web_ui()->GetWebContents(),
      std::vector<ui::ElementIdentifier>{
          kAddCurrentTabToReadingListElementId,
          kSidePanelReadingListUnreadElementId,
      });
}

void ReadingListUI::CreateShoppingListHandler(
    mojo::PendingRemote<shopping_list::mojom::Page> page,
    mojo::PendingReceiver<shopping_list::mojom::ShoppingListHandler> receiver) {
  Profile* const profile = Profile::FromWebUI(web_ui());
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  commerce::ShoppingService* shopping_service =
      commerce::ShoppingServiceFactory::GetForBrowserContext(profile);
  feature_engagement::Tracker* const tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(profile);
  shopping_list_handler_ = std::make_unique<commerce::ShoppingListHandler>(
      std::move(page), std::move(receiver), bookmark_model, shopping_service,
      profile->GetPrefs(), tracker, g_browser_process->GetApplicationLocale());
}

void ReadingListUI::SetActiveTabURL(const GURL& url) {
  if (page_handler_)
    page_handler_->SetActiveTabURL(url);
}
