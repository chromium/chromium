// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/reading_list_sub_menu_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/base/ui_base_features.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(BookmarkSubMenuModel,
                                      kShowBookmarkBarMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(BookmarkSubMenuModel,
                                      kShowBookmarkSidePanelItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(BookmarkSubMenuModel,
                                      kReadingListMenuItem);

// For views and cocoa, we have complex delegate systems to handle
// injecting the bookmarks to the bookmark submenu. This is done to support
// advanced interactions with the menu contents, like right click context menus.

BookmarkSubMenuModel::BookmarkSubMenuModel(
    ui::SimpleMenuModel::Delegate* delegate,
    Browser* browser)
    : SimpleMenuModel(delegate) {
  Build(browser);
}

BookmarkSubMenuModel::~BookmarkSubMenuModel() {}

void BookmarkSubMenuModel::Build(Browser* browser) {
  if (delegate()->IsCommandIdVisible(IDC_BOOKMARK_THIS_TAB) ||
      delegate()->IsCommandIdVisible(IDC_BOOKMARK_ALL_TABS)) {
    AddItemWithStringId(IDC_BOOKMARK_THIS_TAB, IDS_BOOKMARK_THIS_TAB);
    AddItemWithStringId(IDC_BOOKMARK_ALL_TABS, IDS_BOOKMARK_ALL_TABS);
    AddSeparator(ui::NORMAL_SEPARATOR);
  }
  AddItemWithStringId(IDC_SHOW_BOOKMARK_BAR,
                      browser->profile()->GetPrefs()->GetBoolean(
                          bookmarks::prefs::kShowBookmarkBar)
                          ? IDS_HIDE_BOOKMARK_BAR
                          : IDS_SHOW_BOOKMARK_BAR);
  SetElementIdentifierAt(GetIndexOfCommandId(IDC_SHOW_BOOKMARK_BAR).value(),
                         kShowBookmarkBarMenuItem);

    AddItemWithStringId(IDC_SHOW_BOOKMARK_SIDE_PANEL,
                        IDS_SHOW_BOOKMARK_SIDE_PANEL);
    SetElementIdentifierAt(
        GetIndexOfCommandId(IDC_SHOW_BOOKMARK_SIDE_PANEL).value(),
        kShowBookmarkSidePanelItem);

  AddItemWithStringId(IDC_SHOW_BOOKMARK_MANAGER, IDS_BOOKMARK_MANAGER);

#if !BUILDFLAG(IS_CHROMEOS)
  AddItemWithStringId(IDC_IMPORT_SETTINGS, IDS_IMPORT_SETTINGS_MENU_LABEL);
#endif

    AddSeparator(ui::NORMAL_SEPARATOR);

    reading_list_sub_menu_model_ =
        std::make_unique<ReadingListSubMenuModel>(delegate());
    AddSubMenuWithStringIdAndIcon(
        IDC_READING_LIST_MENU, IDS_READING_LIST_MENU,
        reading_list_sub_menu_model_.get(),
        ui::ImageModel::FromVectorIcon(kReadingListIcon));
    SetElementIdentifierAt(GetIndexOfCommandId(IDC_READING_LIST_MENU).value(),
                           kReadingListMenuItem);

    auto set_icon = [this](int command_id, const gfx::VectorIcon& vector_icon) {
      auto index = GetIndexOfCommandId(command_id);
      if (index) {
        SetIcon(index.value(), ui::ImageModel::FromVectorIcon(
                                   vector_icon, ui::kColorMenuIcon, 16));
      }
    };

    set_icon(IDC_BOOKMARK_THIS_TAB, kBookmarksListsMenuIcon);
    set_icon(IDC_BOOKMARK_ALL_TABS, kBookmarkAllTabsChromeRefreshIcon);
    set_icon(IDC_SHOW_BOOKMARK_BAR, kToolbarChromeRefreshIcon);
    set_icon(IDC_SHOW_BOOKMARK_MANAGER, kBookmarksManagerIcon);
    set_icon(IDC_SHOW_BOOKMARK_SIDE_PANEL, kBookmarksSidePanelRefreshIcon);
    set_icon(IDC_IMPORT_SETTINGS, kMenuBookChromeRefreshIcon);
}
