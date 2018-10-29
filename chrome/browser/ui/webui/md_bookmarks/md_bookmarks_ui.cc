// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/md_bookmarks/md_bookmarks_ui.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/md_bookmarks/bookmarks_message_handler.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/plural_string_handler.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_features.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

void AddLocalizedString(content::WebUIDataSource* source,
                        const std::string& message,
                        int id) {
  base::string16 str = l10n_util::GetStringUTF16(id);
  base::Erase(str, '&');
  source->AddString(message, str);
}

content::WebUIDataSource* CreateMdBookmarksUIHTMLSource(Profile* profile) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIBookmarksHost);

  // Build an Accelerator to describe undo shortcut
  // NOTE: the undo shortcut is also defined in md_bookmarks/command_manager.js
  // TODO(b/893033): de-duplicate shortcut by moving all shortcut definitions
  // from JS to C++.
  ui::Accelerator undoAccelerator(ui::VKEY_Z, ui::EF_PLATFORM_ACCELERATOR);
  source->AddString("undoDescription", l10n_util::GetStringFUTF16(
                                           IDS_BOOKMARK_BAR_UNDO_DESCRIPTION,
                                           undoAccelerator.GetShortcutText()));

  // Localized strings (alphabetical order).
  AddLocalizedString(source, "addBookmarkTitle",
                     IDS_MD_BOOKMARK_MANAGER_ADD_BOOKMARK_TITLE);
  AddLocalizedString(source, "addFolderTitle",
                     IDS_MD_BOOKMARK_MANAGER_ADD_FOLDER_TITLE);
  AddLocalizedString(source, "cancel", IDS_CANCEL);
  AddLocalizedString(source, "clearSearch",
                     IDS_MD_BOOKMARK_MANAGER_CLEAR_SEARCH);
  AddLocalizedString(source, "delete", IDS_DELETE);
  AddLocalizedString(source, "editBookmarkTitle", IDS_BOOKMARK_EDITOR_TITLE);
  AddLocalizedString(source, "editDialogInvalidUrl",
                     IDS_BOOKMARK_MANAGER_INVALID_URL);
  AddLocalizedString(source, "editDialogNameInput",
                     IDS_BOOKMARK_MANAGER_NAME_INPUT_PLACE_HOLDER);
  AddLocalizedString(source, "editDialogUrlInput",
                     IDS_BOOKMARK_MANAGER_URL_INPUT_PLACE_HOLDER);
  AddLocalizedString(source, "emptyList", IDS_MD_BOOKMARK_MANAGER_EMPTY_LIST);
  AddLocalizedString(source, "emptyUnmodifiableList",
                     IDS_MD_BOOKMARK_MANAGER_EMPTY_UNMODIFIABLE_LIST);
  AddLocalizedString(source, "folderLabel",
                     IDS_MD_BOOKMARK_MANAGER_FOLDER_LABEL);
  AddLocalizedString(source, "itemsSelected",
                     IDS_MD_BOOKMARK_MANAGER_ITEMS_SELECTED);
  AddLocalizedString(source, "listAxLabel",
                     IDS_MD_BOOKMARK_MANAGER_LIST_AX_LABEL);
  AddLocalizedString(source, "menuAddBookmark",
                     IDS_MD_BOOKMARK_MANAGER_MENU_ADD_BOOKMARK);
  AddLocalizedString(source, "menuAddFolder",
                     IDS_MD_BOOKMARK_MANAGER_MENU_ADD_FOLDER);
  AddLocalizedString(source, "menuCopyURL",
                     IDS_MD_BOOKMARK_MANAGER_MENU_COPY_URL);
  AddLocalizedString(source, "menuDelete", IDS_DELETE);
  AddLocalizedString(source, "menuEdit", IDS_EDIT);
  AddLocalizedString(source, "menuExport", IDS_MD_BOOKMARK_MANAGER_MENU_EXPORT);
  AddLocalizedString(source, "menuHelpCenter",
                     IDS_MD_BOOKMARK_MANAGER_MENU_HELP_CENTER);
  AddLocalizedString(source, "menuImport", IDS_MD_BOOKMARK_MANAGER_MENU_IMPORT);
  AddLocalizedString(source, "menuOpenAllNewTab",
                     IDS_MD_BOOKMARK_MANAGER_MENU_OPEN_ALL);
  AddLocalizedString(source, "menuOpenAllNewWindow",
                     IDS_MD_BOOKMARK_MANAGER_MENU_OPEN_ALL_NEW_WINDOW);
  AddLocalizedString(source, "menuOpenAllIncognito",
                     IDS_MD_BOOKMARK_MANAGER_MENU_OPEN_ALL_INCOGNITO);
  AddLocalizedString(source, "menuOpenNewTab",
                     IDS_MD_BOOKMARK_MANAGER_MENU_OPEN_IN_NEW_TAB);
  AddLocalizedString(source, "menuOpenNewWindow",
                     IDS_MD_BOOKMARK_MANAGER_MENU_OPEN_IN_NEW_WINDOW);
  AddLocalizedString(source, "menuOpenIncognito",
                     IDS_MD_BOOKMARK_MANAGER_MENU_OPEN_INCOGNITO);
  AddLocalizedString(source, "menuRename", IDS_MD_BOOKMARK_MANAGER_MENU_RENAME);
  AddLocalizedString(source, "menuShowInFolder",
                     IDS_MD_BOOKMARK_MANAGER_MENU_SHOW_IN_FOLDER);
  AddLocalizedString(source, "menuSort", IDS_MD_BOOKMARK_MANAGER_MENU_SORT);
  AddLocalizedString(source, "moreActionsButtonTitle",
                     IDS_MD_BOOKMARK_MANAGER_MORE_ACTIONS);
  AddLocalizedString(source, "moreActionsButtonAxLabel",
                     IDS_MD_BOOKMARK_MANAGER_MORE_ACTIONS_AX_LABEL);
  AddLocalizedString(source, "noSearchResults", IDS_SEARCH_NO_RESULTS);
  AddLocalizedString(source, "openDialogBody",
                     IDS_BOOKMARK_BAR_SHOULD_OPEN_ALL);
  AddLocalizedString(source, "openDialogConfirm",
                     IDS_MD_BOOKMARK_MANAGER_OPEN_DIALOG_CONFIRM);
  AddLocalizedString(source, "openDialogTitle",
                     IDS_MD_BOOKMARK_MANAGER_OPEN_DIALOG_TITLE);
  AddLocalizedString(source, "organizeButtonTitle",
                     IDS_BOOKMARK_MANAGER_ORGANIZE_MENU);
  AddLocalizedString(source, "renameFolderTitle",
                     IDS_MD_BOOKMARK_MANAGER_FOLDER_RENAME_TITLE);
  AddLocalizedString(source, "searchPrompt",
                     IDS_BOOKMARK_MANAGER_SEARCH_BUTTON);
  AddLocalizedString(source, "sidebarAxLabel",
                     IDS_MD_BOOKMARK_MANAGER_SIDEBAR_AX_LABEL);
  AddLocalizedString(source, "sidebarNodeCollapseAxLabel",
                     IDS_MD_BOOKMARK_MANAGER_SIDEBAR_NODE_COLLAPSE_AX_LABEL);
  AddLocalizedString(source, "sidebarNodeExpandAxLabel",
                     IDS_MD_BOOKMARK_MANAGER_SIDEBAR_NODE_EXPAND_AX_LABEL);
  AddLocalizedString(source, "searchResults", IDS_SEARCH_RESULTS);
  AddLocalizedString(source, "saveEdit", IDS_SAVE);
  AddLocalizedString(source, "title", IDS_MD_BOOKMARK_MANAGER_TITLE);
  AddLocalizedString(source, "toastFolderSorted",
                     IDS_MD_BOOKMARK_MANAGER_TOAST_FOLDER_SORTED);
  AddLocalizedString(source, "toastItemCopied",
                     IDS_MD_BOOKMARK_MANAGER_TOAST_ITEM_COPIED);
  AddLocalizedString(source, "toastItemDeleted",
                     IDS_MD_BOOKMARK_MANAGER_TOAST_ITEM_DELETED);
  AddLocalizedString(source, "toastUrlCopied",
                     IDS_MD_BOOKMARK_MANAGER_TOAST_URL_COPIED);
  AddLocalizedString(source, "undo", IDS_BOOKMARK_BAR_UNDO);

  // Resources.
  source->AddResourcePath("images/folder_open.svg",
                          IDR_MD_BOOKMARKS_IMAGES_FOLDER_OPEN_SVG);
  source->AddResourcePath("images/folder.svg",
                          IDR_MD_BOOKMARKS_IMAGES_FOLDER_SVG);
#if BUILDFLAG(OPTIMIZE_WEBUI)
  source->AddResourcePath("crisper.js", IDR_MD_BOOKMARKS_CRISPER_JS);
  source->SetDefaultResource(
      base::FeatureList::IsEnabled(features::kWebUIPolymer2)
          ? IDR_MD_BOOKMARKS_VULCANIZED_P2_HTML
          : IDR_MD_BOOKMARKS_VULCANIZED_HTML);
  source->UseGzip({"images/folder_open.svg", "images/folder.svg"});
#else
  source->AddResourcePath("actions.html", IDR_MD_BOOKMARKS_ACTIONS_HTML);
  source->AddResourcePath("actions.js", IDR_MD_BOOKMARKS_ACTIONS_JS);
  source->AddResourcePath("api_listener.html",
                          IDR_MD_BOOKMARKS_API_LISTENER_HTML);
  source->AddResourcePath("api_listener.js", IDR_MD_BOOKMARKS_API_LISTENER_JS);
  source->AddResourcePath("app.html", IDR_MD_BOOKMARKS_APP_HTML);
  source->AddResourcePath("app.js", IDR_MD_BOOKMARKS_APP_JS);
  source->AddResourcePath("command_manager.html",
                          IDR_MD_BOOKMARKS_COMMAND_MANAGER_HTML);
  source->AddResourcePath("command_manager.js",
                          IDR_MD_BOOKMARKS_COMMAND_MANAGER_JS);
  source->AddResourcePath("constants.html", IDR_MD_BOOKMARKS_CONSTANTS_HTML);
  source->AddResourcePath("constants.js", IDR_MD_BOOKMARKS_CONSTANTS_JS);
  source->AddResourcePath("debouncer.html", IDR_MD_BOOKMARKS_DEBOUNCER_HTML);
  source->AddResourcePath("debouncer.js", IDR_MD_BOOKMARKS_DEBOUNCER_JS);
  source->AddResourcePath("dialog_focus_manager.html",
                          IDR_MD_BOOKMARKS_DIALOG_FOCUS_MANAGER_HTML);
  source->AddResourcePath("dialog_focus_manager.js",
                          IDR_MD_BOOKMARKS_DIALOG_FOCUS_MANAGER_JS);
  source->AddResourcePath("dnd_manager.html",
                          IDR_MD_BOOKMARKS_DND_MANAGER_HTML);
  source->AddResourcePath("dnd_manager.js", IDR_MD_BOOKMARKS_DND_MANAGER_JS);
  source->AddResourcePath("edit_dialog.html",
                          IDR_MD_BOOKMARKS_EDIT_DIALOG_HTML);
  source->AddResourcePath("edit_dialog.js", IDR_MD_BOOKMARKS_EDIT_DIALOG_JS);
  source->AddResourcePath("folder_node.html",
                          IDR_MD_BOOKMARKS_FOLDER_NODE_HTML);
  source->AddResourcePath("folder_node.js", IDR_MD_BOOKMARKS_FOLDER_NODE_JS);
  source->AddResourcePath("item.html", IDR_MD_BOOKMARKS_ITEM_HTML);
  source->AddResourcePath("item.js", IDR_MD_BOOKMARKS_ITEM_JS);
  source->AddResourcePath("list.html", IDR_MD_BOOKMARKS_LIST_HTML);
  source->AddResourcePath("list.js", IDR_MD_BOOKMARKS_LIST_JS);
  source->AddResourcePath("mouse_focus_behavior.html",
                          IDR_MD_BOOKMARKS_MOUSE_FOCUS_BEHAVIOR_HTML);
  source->AddResourcePath("mouse_focus_behavior.js",
                          IDR_MD_BOOKMARKS_MOUSE_FOCUS_BEHAVIOR_JS);
  source->AddResourcePath("reducers.html", IDR_MD_BOOKMARKS_REDUCERS_HTML);
  source->AddResourcePath("reducers.js", IDR_MD_BOOKMARKS_REDUCERS_JS);
  source->AddResourcePath("router.html", IDR_MD_BOOKMARKS_ROUTER_HTML);
  source->AddResourcePath("router.js", IDR_MD_BOOKMARKS_ROUTER_JS);
  source->AddResourcePath("shared_style.html",
                          IDR_MD_BOOKMARKS_SHARED_STYLE_HTML);
  source->AddResourcePath("shared_vars.html",
                          IDR_MD_BOOKMARKS_SHARED_VARS_HTML);
  source->AddResourcePath("store.html", IDR_MD_BOOKMARKS_STORE_HTML);
  source->AddResourcePath("store.js", IDR_MD_BOOKMARKS_STORE_JS);
  source->AddResourcePath("store_client.html",
                          IDR_MD_BOOKMARKS_STORE_CLIENT_HTML);
  source->AddResourcePath("store_client.js", IDR_MD_BOOKMARKS_STORE_CLIENT_JS);
  source->AddResourcePath("toast_manager.html",
                          IDR_MD_BOOKMARKS_TOAST_MANAGER_HTML);
  source->AddResourcePath("toast_manager.js",
                          IDR_MD_BOOKMARKS_TOAST_MANAGER_JS);
  source->AddResourcePath("toolbar.html", IDR_MD_BOOKMARKS_TOOLBAR_HTML);
  source->AddResourcePath("toolbar.js", IDR_MD_BOOKMARKS_TOOLBAR_JS);
  source->AddResourcePath("util.html", IDR_MD_BOOKMARKS_UTIL_HTML);
  source->AddResourcePath("util.js", IDR_MD_BOOKMARKS_UTIL_JS);

  source->SetDefaultResource(IDR_MD_BOOKMARKS_BOOKMARKS_HTML);
#endif

  source->SetJsonPath("strings.js");

  return source;
}

}  // namespace

MdBookmarksUI::MdBookmarksUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  // Set up the chrome://bookmarks/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile,
                                CreateMdBookmarksUIHTMLSource(profile));

  auto plural_string_handler = std::make_unique<PluralStringHandler>();
  plural_string_handler->AddLocalizedString(
      "listChanged", IDS_MD_BOOKMARK_MANAGER_FOLDER_LIST_CHANGED);
  plural_string_handler->AddLocalizedString(
      "toastItemsDeleted", IDS_MD_BOOKMARK_MANAGER_TOAST_ITEMS_DELETED);
  plural_string_handler->AddLocalizedString(
      "toastItemsCopied", IDS_MD_BOOKMARK_MANAGER_TOAST_ITEMS_COPIED);
  web_ui->AddMessageHandler(std::move(plural_string_handler));

  web_ui->AddMessageHandler(std::make_unique<BookmarksMessageHandler>());
  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());
}

// static
base::RefCountedMemory* MdBookmarksUI::GetFaviconResourceBytes(
    ui::ScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      IDR_BOOKMARKS_FAVICON, scale_factor);
}
