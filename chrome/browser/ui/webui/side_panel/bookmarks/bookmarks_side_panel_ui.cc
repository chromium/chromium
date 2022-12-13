// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/bookmarks/bookmarks_side_panel_ui.h"

#include <string>
#include <utility>

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/plural_string_handler.h"
#include "chrome/browser/ui/webui/sanitized_image_source.h"
#include "chrome/browser/ui/webui/side_panel/bookmarks/bookmarks_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
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
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/views/style/platform_style.h"

BookmarksSidePanelUI::BookmarksSidePanelUI(content::WebUI* web_ui)
    : ui::MojoBubbleWebUIController(web_ui, true) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIBookmarksSidePanelHost);
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"bookmarksTabTitle", IDS_BOOKMARK_MANAGER_TITLE},
      {"bookmarkCopied", IDS_BOOKMARK_MANAGER_TOAST_ITEM_COPIED},
      {"bookmarkDeleted", IDS_BOOKMARK_MANAGER_TOAST_ITEM_DELETED},
      {"bookmarkCreated", IDS_BOOKMARK_SCREEN_READER_CREATED},
      {"bookmarkReordered", IDS_BOOKMARK_SCREEN_READER_REORDERED},
      {"bookmarkMoved", IDS_BOOKMARK_SCREEN_READER_MOVED},
      {"sidePanelTitle", IDS_SIDE_PANEL_TITLE},
      {"tooltipClose", IDS_CLOSE},
      {"tooltipDelete", IDS_DELETE},
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
      {"addCurrentTab", IDS_READ_LATER_ADD_CURRENT_TAB},
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

  source->AddBoolean("unifiedSidePanel",
                     base::FeatureList::IsEnabled(features::kUnifiedSidePanel));

  source->AddBoolean("guestMode", profile->IsGuestSession());

  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  source->AddString(
      "otherBookmarksId",
      base::NumberToString(bookmark_model ? bookmark_model->other_node()->id()
                                          : -1));

  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));
  const int resource =
      base::FeatureList::IsEnabled(features::kPowerBookmarksSidePanel)
          ? IDR_SIDE_PANEL_BOOKMARKS_POWER_BOOKMARKS_HTML
          : IDR_SIDE_PANEL_BOOKMARKS_BOOKMARKS_HTML;
  webui::SetupWebUIDataSource(
      source, base::make_span(kSidePanelResources, kSidePanelResourcesSize),
      resource);

  // Add a handler to provide pluralized strings.
  auto plural_string_handler = std::make_unique<PluralStringHandler>();
  plural_string_handler->AddLocalizedString("bookmarkFolderChildCount",
                                            IDS_BOOKMARK_FOLDER_CHILD_COUNT);
  web_ui->AddMessageHandler(std::move(plural_string_handler));

  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                source);
  content::URLDataSource::Add(profile,
                              std::make_unique<SanitizedImageSource>(profile));
}

BookmarksSidePanelUI::~BookmarksSidePanelUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(BookmarksSidePanelUI)

void BookmarksSidePanelUI::BindInterface(
    mojo::PendingReceiver<side_panel::mojom::BookmarksPageHandlerFactory>
        receiver) {
  bookmarks_page_factory_receiver_.reset();
  bookmarks_page_factory_receiver_.Bind(std::move(receiver));
}

void BookmarksSidePanelUI::BindInterface(
    mojo::PendingReceiver<shopping_list::mojom::ShoppingListHandlerFactory>
        receiver) {
  shopping_list_factory_receiver_.reset();
  shopping_list_factory_receiver_.Bind(std::move(receiver));
}

void BookmarksSidePanelUI::CreateBookmarksPageHandler(
    mojo::PendingReceiver<side_panel::mojom::BookmarksPageHandler> receiver) {
  bookmarks_page_handler_ =
      std::make_unique<BookmarksPageHandler>(std::move(receiver), this);
}

void BookmarksSidePanelUI::CreateShoppingListHandler(
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
