// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/side_panel/bookmarks/bookmarks_side_panel_ui.h"

#include <string>
#include <utility>

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/page_image_service/image_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/bookmarks/bookmark_prefs.h"
#include "chrome/browser/ui/webui/commerce/shopping_list_context_menu_controller.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/plural_string_handler.h"
#include "chrome/browser/ui/webui/sanitized_image_source.h"
#include "chrome/browser/ui/webui/side_panel/bookmarks/bookmarks_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/side_panel_bookmarks_resources.h"
#include "chrome/grit/side_panel_bookmarks_resources_map.h"
#include "chrome/grit/side_panel_shared_resources.h"
#include "chrome/grit/side_panel_shared_resources_map.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/webui/shopping_service_handler.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/page_image_service/features.h"
#include "components/page_image_service/image_service.h"
#include "components/page_image_service/image_service_handler.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/views/style/platform_style.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

BookmarksSidePanelUIConfig::BookmarksSidePanelUIConfig()
    : DefaultTopChromeWebUIConfig(content::kChromeUIScheme,
                                  chrome::kChromeUIBookmarksSidePanelHost) {}

bool BookmarksSidePanelUIConfig::IsPreloadable() {
  return true;
}

std::optional<int> BookmarksSidePanelUIConfig::GetCommandIdForTesting() {
  return IDC_SHOW_BOOKMARK_SIDE_PANEL;
}

BookmarksSidePanelUI::BookmarksSidePanelUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui, true) {
  Profile* const profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIBookmarksSidePanelHost);
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"bookmarksTabTitle", IDS_BOOKMARK_MANAGER_TITLE},
      {"bookmarkCopied", IDS_BOOKMARK_MANAGER_TOAST_ITEM_COPIED},
      {"bookmarkDeleted", IDS_BOOKMARK_MANAGER_TOAST_ITEM_DELETED},
      {"bookmarkCreated", IDS_BOOKMARK_SCREEN_READER_CREATED},
      {"bookmarkFolderCreated", IDS_BOOKMARK_SCREEN_READER_FOLDER_CREATED},
      {"bookmarkReordered", IDS_BOOKMARK_SCREEN_READER_REORDERED},
      {"bookmarkMoved", IDS_BOOKMARK_SCREEN_READER_MOVED},
      {"tooltipClose", IDS_CLOSE},
      {"tooltipDelete", IDS_DELETE},
      {"tooltipMore", IDS_BOOKMARKS_EDIT_MORE},
      {"tooltipMove", IDS_BOOKMARKS_EDIT_MOVE_TO_ANOTHER_FOLDER},
      {"tooltipOrganize", IDS_BOOKMARK_MANAGER_ORGANIZE_MENU},
      {"tooltipNewFolder", IDS_BOOKMARKS_NEW_FOLDER_TOOLTIP},
      {"tooltipEdit", IDS_EDIT},
      {"tooltipBack", IDS_BOOKMARKS_BACK_BUTTON_TOOLTIP},
      {"shoppingListFolderTitle", IDS_SIDE_PANEL_TRACKED_PRODUCTS},
      {"shoppingListTrackPriceButtonDescription",
       IDS_PRICE_TRACKING_TRACK_PRODUCT_ACCESSIBILITY},
      {"shoppingListUntrackPriceButtonDescription",
       IDS_PRICE_TRACKING_UNTRACK_PRODUCT_ACCESSIBILITY},
      {"shoppingListErrorMessage", IDS_PRICE_TRACKING_SIDE_PANEL_ERROR_MESSAGE},
      {"shoppingListErrorButton", IDS_PRICE_TRACKING_SIDE_PANEL_ERROR_BUTTON},
      {"sortByType", IDS_BOOKMARKS_SORT_BY_TYPE},
      {"allBookmarks", IDS_BOOKMARKS_ALL_BOOKMARKS},
      {"priceTrackingLabel", IDS_BOOKMARKS_LABEL_TRACKED_PRODUCTS},
      {"sortNewest", IDS_BOOKMARKS_SORT_NEWEST},
      {"sortNewestLower", IDS_BOOKMARKS_SORT_NEWEST_LOWER},
      {"sortOldest", IDS_BOOKMARKS_SORT_OLDEST},
      {"sortOldestLower", IDS_BOOKMARKS_SORT_OLDEST_LOWER},
      {"sortAlphabetically", IDS_BOOKMARKS_SORT_ALPHABETICALLY},
      {"sortReverseAlphabetically", IDS_BOOKMARKS_SORT_REVERSE_ALPHABETICALLY},
      {"sortLastOpened", IDS_BOOKMARKS_SORT_LAST_OPENED},
      {"sortLastOpenedLower", IDS_BOOKMARKS_SORT_LAST_OPENED_LOWER},
      {"visualView", IDS_BOOKMARKS_VISUAL_VIEW},
      {"compactView", IDS_BOOKMARKS_COMPACT_VIEW},
      {"sortMenuA11yLabel", IDS_BOOKMARKS_SORT_MENU_A11Y_LABEL},
      {"createNewFolderA11yLabel", IDS_BOOKMARKS_CREATE_NEW_FOLDER_A11Y_LABEL},
      {"editBookmarkListA11yLabel",
       IDS_BOOKMARKS_EDIT_BOOKMARK_LIST_A11Y_LABEL},
      {"cancelA11yLabel", IDS_CANCEL},
      {"bookmarkNameA11yLabel", IDS_BOOKMARK_AX_EDITOR_NAME_LABEL},
      {"addCurrentTab", IDS_READ_LATER_ADD_CURRENT_TAB},
      {"emptyTitle", IDS_BOOKMARKS_EMPTY_STATE_TITLE},
      {"emptyBody", IDS_BOOKMARKS_EMPTY_STATE_BODY},
      {"emptyTitleFolder", IDS_BOOKMARKS_EMPTY_STATE_TITLE_FOLDER},
      {"emptyBodyFolder", IDS_BOOKMARKS_EMPTY_STATE_BODY_FOLDER},
      {"emptyTitleGuest", IDS_BOOKMARKS_EMPTY_STATE_TITLE_GUEST},
      {"emptyBodyGuest", IDS_BOOKMARKS_EMPTY_STATE_BODY_GUEST},
      {"emptyTitleSearch", IDS_BOOKMARKS_EMPTY_STATE_TITLE_SEARCH},
      {"emptyBodySearch", IDS_BOOKMARKS_EMPTY_STATE_BODY_SEARCH},
      {"searchBookmarks", IDS_BOOKMARK_MANAGER_SEARCH_BUTTON},
      {"clearSearch", IDS_BOOKMARK_MANAGER_CLEAR_SEARCH},
      {"selectedBookmarkCount", IDS_BOOKMARK_MANAGER_ITEMS_SELECTED},
      {"menuOpenNewTab", IDS_BOOKMARK_MANAGER_MENU_OPEN_IN_NEW_TAB},
      {"menuOpenNewTabWithCount",
       IDS_BOOKMARK_MANAGER_MENU_OPEN_ALL_WITH_COUNT},
      {"menuOpenNewWindow", IDS_BOOKMARK_MANAGER_MENU_OPEN_IN_NEW_WINDOW},
      {"menuOpenNewWindowWithCount",
       IDS_BOOKMARK_MANAGER_MENU_OPEN_ALL_NEW_WINDOW_WITH_COUNT},
      {"menuOpenIncognito", IDS_BOOKMARK_MANAGER_MENU_OPEN_INCOGNITO},
      {"menuOpenIncognitoWithCount",
       IDS_BOOKMARK_MANAGER_MENU_OPEN_ALL_INCOGNITO_WITH_COUNT},
      {"menuOpenNewTabGroup", IDS_BOOKMARK_MANAGER_MENU_OPEN_IN_NEW_TAB_GROUP},
      {"menuOpenNewTabGroupWithCount",
       IDS_BOOKMARK_MANAGER_MENU_OPEN_ALL_NEW_TAB_GROUP_WITH_COUNT},
      {"menuMoveToBookmarksBar", IDS_BOOKMARKS_MOVE_TO_BOOKMARKS_BAR},
      {"menuMoveToAllBookmarks", IDS_BOOKMARKS_MOVE_TO_ALL_BOOKMARKS},
      {"menuTrackPrice", IDS_SIDE_PANEL_TRACK_BUTTON},
      {"menuUntrackPrice", IDS_SIDE_PANEL_UNTRACK_BUTTON},
      {"menuEdit", IDS_BOOKMARKS_EDIT},
      {"menuRename", IDS_BOOKMARKS_RENAME},
      {"newFolderTitle", IDS_BOOKMARK_EDITOR_NEW_FOLDER_NAME},
      {"undoBookmarkDeletion", IDS_UNDO_BOOKMARK_DELETION},
      {"urlFolderDescription", IDS_BOOKMARKS_URL_FOLDER_DESCRIPTION},
      {"editBookmark", IDS_BOOKMARKS_EDIT_BOOKMARK},
      {"editMoveFolderTo", IDS_BOOKMARKS_EDIT_MOVE_TO},
      {"editNewFolder", IDS_BOOKMARKS_EDIT_NEW_FOLDER},
      {"editCancel", IDS_BOOKMARKS_EDIT_CANCEL},
      {"editSave", IDS_BOOKMARKS_EDIT_SAVE},
      {"editName", IDS_BOOKMARKS_EDIT_NAME},
      {"editUrl", IDS_BOOKMARKS_EDIT_URL},
      {"disabledFeature", IDS_BOOKMARKS_DISABLED_FEATURE},
      {"backButtonLabel", IDS_BOOKMARKS_BACK_BUTTON_LABEL},
      {"forwardButtonLabel", IDS_BOOKMARKS_FORWARD_BUTTON_LABEL},
      {"bookmarkMenuLabel", IDS_BOOKMARK_OPTIONS_LABEL},
      {"folderMenuLabel", IDS_FOLDER_OPTIONS_LABEL},
      {"openFolderLabel", IDS_BOOKMARKS_OPEN_FOLDER_LABEL},
      {"openBookmarkLabel", IDS_BOOKMARKS_OPEN_BOOKMARK_LABEL},
      {"selectFolderLabel", IDS_BOOKMARKS_SELECT_FOLDER_LABEL},
      {"selectBookmarkLabel", IDS_BOOKMARKS_SELECT_BOOKMARK_LABEL},
      {"deselectFolderLabel", IDS_BOOKMARKS_DESELECT_FOLDER_LABEL},
      {"deselectBookmarkLabel", IDS_BOOKMARKS_DESELECT_BOOKMARK_LABEL},
      {"a11yDescriptionPriceTracking",
       IDS_BOOKMARK_ACCESSIBLE_DESCRIPTION_PRICE_TRACKING},
      {"a11yDescriptionPriceChange",
       IDS_BOOKMARK_ACCESSIBLE_DESCRIPTION_PRICE_CHANGE},
      {"checkboxA11yLabel", IDS_BOOKMARKS_CHECKBOX_LABEL},
      {"editInvalidUrl", IDS_BOOKMARK_MANAGER_INVALID_URL},
      {"bookmarkFolderChildCount", IDS_BOOKMARK_FOLDER_CHILD_COUNT},
      {"primaryFilterHeading", IDS_BOOKMARKS_PRIMARY_FILTER_HEADING},
      {"secondaryFilterHeading", IDS_BOOKMARKS_SECONDARY_FILTER_HEADING},
  };
  for (const auto& str : kLocalizedStrings)
    webui::AddLocalizedString(source, str.name, str.id);

  source->AddBoolean("useRipples", views::PlatformStyle::kUseRipples);

  PrefService* prefs = profile->GetPrefs();
  source->AddBoolean(
      "editBookmarksEnabled",
      prefs->GetBoolean(bookmarks::prefs::kEditBookmarksEnabled));
  source->AddBoolean(
      "hasManagedBookmarks",
      !prefs->GetList(bookmarks::prefs::kManagedBookmarks).empty());
  source->AddBoolean("shoppingListEnabled",
                     commerce::IsShoppingListAllowedForEnterprise(prefs));
  source->AddBoolean(
      "urlImagesEnabled",
      base::FeatureList::IsEnabled(page_image_service::kImageService));

  source->AddBoolean("guestMode", profile->IsGuestSession());
  source->AddBoolean("incognitoMode", profile->IsIncognitoProfile());
  source->AddBoolean("isIncognitoModeAvailable", IsIncognitoModeAvailable());
  source->AddBoolean(
      "bookmarksTreeViewEnabled",
      base::FeatureList::IsEnabled(features::kBookmarksTreeView));
  source->AddInteger(
      "sortOrder",
      prefs->GetInteger(bookmarks_webui::prefs::kBookmarksSortOrder));
  source->AddInteger(
      "viewType",
      prefs->GetInteger(bookmarks_webui::prefs::kBookmarksViewType));

  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  source->AddString(
      "bookmarksBarId",
      base::NumberToString(bookmark_model && bookmark_model->bookmark_bar_node()
                               ? bookmark_model->bookmark_bar_node()->id()
                               : -1));
  source->AddString(
      "otherBookmarksId",
      base::NumberToString(bookmark_model && bookmark_model->other_node()
                               ? bookmark_model->other_node()->id()
                               : -1));
  source->AddString(
      "mobileBookmarksId",
      base::NumberToString(bookmark_model && bookmark_model->mobile_node()
                               ? bookmark_model->mobile_node()->id()
                               : -1));
  bookmarks::ManagedBookmarkService* managed =
      ManagedBookmarkServiceFactory::GetForProfile(profile);
  source->AddString("managedBookmarksFolderId",
                    managed && managed->managed_node()
                        ? base::NumberToString(managed->managed_node()->id())
                        : "");

  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));
  const int resource = IDR_SIDE_PANEL_BOOKMARKS_POWER_BOOKMARKS_HTML;
  webui::SetupWebUIDataSource(source,
                              base::make_span(kSidePanelBookmarksResources,
                                              kSidePanelBookmarksResourcesSize),
                              resource);
  source->AddResourcePaths(base::make_span(kSidePanelSharedResources,
                                           kSidePanelSharedResourcesSize));

  // Add a handler to provide pluralized strings.
  auto plural_string_handler = std::make_unique<PluralStringHandler>();
  plural_string_handler->AddLocalizedString("bookmarkDeletionCount",
                                            IDS_BOOKMARK_DELETION_COUNT);
  web_ui->AddMessageHandler(std::move(plural_string_handler));

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
    mojo::PendingReceiver<
        shopping_service::mojom::ShoppingServiceHandlerFactory> receiver) {
  shopping_service_factory_receiver_.reset();
  shopping_service_factory_receiver_.Bind(std::move(receiver));
}

void BookmarksSidePanelUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
        pending_receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(pending_receiver));
}

void BookmarksSidePanelUI::BindInterface(
    mojo::PendingReceiver<page_image_service::mojom::PageImageServiceHandler>
        pending_image_handler) {
  base::WeakPtr<page_image_service::ImageService> image_service_weak;
  if (auto* image_service =
          page_image_service::ImageServiceFactory::GetForBrowserContext(
              Profile::FromWebUI(web_ui()))) {
    image_service_weak = image_service->GetWeakPtr();
  }
  image_service_handler_ =
      std::make_unique<page_image_service::ImageServiceHandler>(
          std::move(pending_image_handler), std::move(image_service_weak));
}

commerce::ShoppingListContextMenuController*
BookmarksSidePanelUI::GetShoppingListContextMenuController() {
  return shopping_list_context_menu_controller_.get();
}

void BookmarksSidePanelUI::CreateBookmarksPageHandler(
    mojo::PendingReceiver<side_panel::mojom::BookmarksPageHandler> receiver) {
  bookmarks_page_handler_ =
      std::make_unique<BookmarksPageHandler>(std::move(receiver), this);
}

void BookmarksSidePanelUI::CreateShoppingServiceHandler(
    mojo::PendingRemote<shopping_service::mojom::Page> page,
    mojo::PendingReceiver<shopping_service::mojom::ShoppingServiceHandler>
        receiver) {
  Profile* const profile = Profile::FromWebUI(web_ui());
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  commerce::ShoppingService* shopping_service =
      commerce::ShoppingServiceFactory::GetForBrowserContext(profile);
  feature_engagement::Tracker* const tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(profile);
  shopping_service_handler_ =
      std::make_unique<commerce::ShoppingServiceHandler>(
          std::move(page), std::move(receiver), bookmark_model,
          shopping_service, profile->GetPrefs(), tracker, nullptr, nullptr);
  shopping_list_context_menu_controller_ =
      std::make_unique<commerce::ShoppingListContextMenuController>(
          bookmark_model, shopping_service, shopping_service_handler_.get());
}

bool BookmarksSidePanelUI::IsIncognitoModeAvailable() {
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  return prefs->GetInteger(policy::policy_prefs::kIncognitoModeAvailability) ==
         static_cast<int>(policy::IncognitoModeAvailability::kEnabled);
}
