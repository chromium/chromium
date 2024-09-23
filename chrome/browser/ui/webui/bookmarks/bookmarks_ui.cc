// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/bookmarks/bookmarks_ui.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/bookmarks/bookmarks_message_handler.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/managed_ui_handler.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/page_not_available_for_guest/page_not_available_for_guest_ui.h"
#include "chrome/browser/ui/webui/plural_string_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/bookmarks_resources.h"
#include "chrome/grit/bookmarks_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"

namespace {

void AddLocalizedString(content::WebUIDataSource* source,
                        const std::string& message,
                        int id) {
  std::u16string str = l10n_util::GetStringUTF16(id);
  std::erase(str, '&');
  source->AddString(message, str);
}

content::WebUIDataSource* CreateAndAddBookmarksUIHTMLSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIBookmarksHost);
  webui::SetupWebUIDataSource(
      source, base::make_span(kBookmarksResources, kBookmarksResourcesSize),
      IDR_BOOKMARKS_BOOKMARKS_HTML);

  // Build an Accelerator to describe undo shortcut
  // NOTE: the undo shortcut is also defined in bookmarks/command_manager.js
  // TODO(crbug.com/40597071): de-duplicate shortcut by moving all shortcut
  // definitions from JS to C++.
  ui::Accelerator undo_accelerator(ui::VKEY_Z, ui::EF_PLATFORM_ACCELERATOR);
  source->AddString("undoDescription", l10n_util::GetStringFUTF16(
                                           IDS_UNDO_DESCRIPTION,
                                           undo_accelerator.GetShortcutText()));

  // Localized strings (alphabetical order).
  static constexpr webui::LocalizedString kStrings[] = {
      {"addBookmarkTitle", IDS_BOOKMARK_MANAGER_ADD_BOOKMARK_TITLE},
      {"addFolderTitle", IDS_BOOKMARK_MANAGER_ADD_FOLDER_TITLE},
      {"cancel", IDS_CANCEL},
      {"clearSearch", IDS_BOOKMARK_MANAGER_CLEAR_SEARCH},
      {"delete", IDS_DELETE},
      {"editBookmarkTitle", IDS_BOOKMARK_EDITOR_TITLE},
      {"editDialogInvalidUrl", IDS_BOOKMARK_MANAGER_INVALID_URL},
      {"editDialogNameInput", IDS_BOOKMARK_MANAGER_NAME_INPUT_PLACE_HOLDER},
      {"editDialogUrlInput", IDS_BOOKMARK_MANAGER_URL_INPUT_PLACE_HOLDER},
      {"emptyList", IDS_BOOKMARK_MANAGER_EMPTY_LIST},
      {"emptyUnmodifiableList", IDS_BOOKMARK_MANAGER_EMPTY_UNMODIFIABLE_LIST},
      {"folderLabel", IDS_BOOKMARK_MANAGER_FOLDER_LABEL},
      {"importBegan", IDS_BOOKMARK_MANAGER_MENU_IMPORT_BEGAN},
      {"importEnded", IDS_BOOKMARK_MANAGER_MENU_IMPORT_ENDED},
      {"itemsSelected", IDS_BOOKMARK_MANAGER_ITEMS_SELECTED},
      {"itemsUnselected", IDS_BOOKMARK_MANAGER_ITEMS_UNSELECTED},
      {"listAxLabel", IDS_BOOKMARK_MANAGER_LIST_AX_LABEL},
      {"menu", IDS_MENU},
      {"menuAddBookmark", IDS_BOOKMARK_MANAGER_MENU_ADD_BOOKMARK},
      {"menuAddFolder", IDS_BOOKMARK_MANAGER_MENU_ADD_FOLDER},
      {"menuCut", IDS_BOOKMARK_MANAGER_MENU_CUT},
      {"menuCopy", IDS_BOOKMARK_MANAGER_MENU_COPY},
      {"menuPaste", IDS_BOOKMARK_MANAGER_MENU_PASTE},
      {"menuDelete", IDS_DELETE},
      {"menuEdit", IDS_EDIT},
      {"menuExport", IDS_BOOKMARK_MANAGER_MENU_EXPORT},
      {"menuHelpCenter", IDS_BOOKMARK_MANAGER_MENU_HELP_CENTER},
      {"menuImport", IDS_BOOKMARK_MANAGER_MENU_IMPORT},
      {"menuOpenAllNewTab", IDS_BOOKMARK_MANAGER_MENU_OPEN_ALL},
      {"menuOpenAllNewTabWithCount",
       IDS_BOOKMARK_MANAGER_MENU_OPEN_ALL_WITH_COUNT},
      {"menuOpenAllNewWindow", IDS_BOOKMARK_MANAGER_MENU_OPEN_ALL_NEW_WINDOW},
      {"menuOpenAllNewWindowWithCount",
       IDS_BOOKMARK_MANAGER_MENU_OPEN_ALL_NEW_WINDOW_WITH_COUNT},
      {"menuOpenAllIncognito", IDS_BOOKMARK_MANAGER_MENU_OPEN_ALL_INCOGNITO},
      {"menuOpenAllIncognitoWithCount",
       IDS_BOOKMARK_MANAGER_MENU_OPEN_ALL_INCOGNITO_WITH_COUNT},
      {"menuOpenNewTab", IDS_BOOKMARK_MANAGER_MENU_OPEN_IN_NEW_TAB},
      {"menuOpenNewWindow", IDS_BOOKMARK_MANAGER_MENU_OPEN_IN_NEW_WINDOW},
      {"menuOpenIncognito", IDS_BOOKMARK_MANAGER_MENU_OPEN_INCOGNITO},
      {"menuRename", IDS_BOOKMARK_MANAGER_MENU_RENAME},
      {"menuShowInFolder", IDS_BOOKMARK_MANAGER_MENU_SHOW_IN_FOLDER},
      {"menuSort", IDS_BOOKMARK_MANAGER_MENU_SORT},
      {"moreActionsButtonTitle", IDS_BOOKMARK_MANAGER_MORE_ACTIONS},
      {"moreActionsButtonAxLabel", IDS_BOOKMARK_MANAGER_MORE_ACTIONS_AX_LABEL},
      {"moreActionsMultiButtonAxLabel",
       IDS_BOOKMARK_MANAGER_MORE_ACTIONS_MULTI_AX_LABEL},
      {"noSearchResults", IDS_SEARCH_NO_RESULTS},
      {"openDialogBody", IDS_BOOKMARK_BAR_SHOULD_OPEN_ALL},
      {"openDialogConfirm", IDS_BOOKMARK_MANAGER_OPEN_DIALOG_CONFIRM},
      {"openDialogTitle", IDS_BOOKMARK_MANAGER_OPEN_DIALOG_TITLE},
      {"organizeButtonTitle", IDS_BOOKMARK_MANAGER_ORGANIZE_MENU},
      {"renameFolderTitle", IDS_BOOKMARK_MANAGER_FOLDER_RENAME_TITLE},
      {"searchPrompt", IDS_BOOKMARK_MANAGER_SEARCH_BUTTON},
      {"sidebarAxLabel", IDS_BOOKMARK_MANAGER_SIDEBAR_AX_LABEL},
      {"searchCleared", IDS_SEARCH_CLEARED},
      {"searchResults", IDS_SEARCH_RESULTS},
      {"saveEdit", IDS_SAVE},
      {"title", IDS_BOOKMARK_MANAGER_TITLE},
      {"toastFolderSorted", IDS_BOOKMARK_MANAGER_TOAST_FOLDER_SORTED},
      {"toastItemCopied", IDS_BOOKMARK_MANAGER_TOAST_ITEM_COPIED},
      {"toastItemDeleted", IDS_BOOKMARK_MANAGER_TOAST_ITEM_DELETED},
      {"undo", IDS_BOOKMARK_BAR_UNDO},
  };
  for (const auto& str : kStrings)
    AddLocalizedString(source, str.name, str.id);

  return source;
}

}  // namespace

BookmarksUIConfig::BookmarksUIConfig()
    : WebUIConfig(content::kChromeUIScheme, chrome::kChromeUIBookmarksHost) {}

BookmarksUIConfig::~BookmarksUIConfig() = default;

std::unique_ptr<content::WebUIController>
BookmarksUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                         const GURL& url) {
  Profile* profile = Profile::FromWebUI(web_ui);
  if (profile->IsGuestSession()) {
    return std::make_unique<PageNotAvailableForGuestUI>(
        web_ui, chrome::kChromeUIBookmarksHost);
  }
  return std::make_unique<BookmarksUI>(web_ui);
}

BookmarksUI::BookmarksUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  // Set up the chrome://bookmarks/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  auto* source = CreateAndAddBookmarksUIHTMLSource(profile);
  ManagedUIHandler::Initialize(web_ui, source);

  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));

  auto plural_string_handler = std::make_unique<PluralStringHandler>();
  plural_string_handler->AddLocalizedString(
      "listChanged", IDS_BOOKMARK_MANAGER_FOLDER_LIST_CHANGED);
  plural_string_handler->AddLocalizedString(
      "toastItemsDeleted", IDS_BOOKMARK_MANAGER_TOAST_ITEMS_DELETED);
  plural_string_handler->AddLocalizedString(
      "toastItemsCopied", IDS_BOOKMARK_MANAGER_TOAST_ITEMS_COPIED);
  web_ui->AddMessageHandler(std::move(plural_string_handler));

  web_ui->AddMessageHandler(std::make_unique<BookmarksMessageHandler>());
  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());
}

// static
base::RefCountedMemory* BookmarksUI::GetFaviconResourceBytes(
    ui::ResourceScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      IDR_BOOKMARKS_FAVICON, scale_factor);
}
