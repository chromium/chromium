// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/toolbar/reading_list_sub_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/feature_utils.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/strings/grit/components_strings.h"
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

BookmarkSubMenuModel::~BookmarkSubMenuModel() = default;

void BookmarkSubMenuModel::Build(Browser* browser) {
  if (delegate()->IsCommandIdVisible(IDC_BOOKMARK_THIS_TAB) ||
      delegate()->IsCommandIdVisible(IDC_BOOKMARK_ALL_TABS)) {
    AddItemWithStringId(IDC_BOOKMARK_THIS_TAB, IDS_BOOKMARK_THIS_TAB);
    AddItemWithStringId(IDC_BOOKMARK_ALL_TABS, IDS_BOOKMARK_ALL_TABS);
    AddSeparator(ui::NORMAL_SEPARATOR);
  }
  if (base::FeatureList::IsEnabled(
          ntp_features::kNtpSimplificationBookmarkBar)) {
    bookmark_bar_sub_menu_model_ =
        std::make_unique<ui::SimpleMenuModel>(delegate());
    bookmark_bar_sub_menu_model_->AddCheckItemWithStringId(
        IDC_BOOKMARK_BAR_SUBMENU_ALWAYS_HIDE,
        IDS_BOOKMARK_BAR_SUBMENU_ALWAYS_HIDE);
    bookmark_bar_sub_menu_model_->AddCheckItemWithStringId(
        IDC_BOOKMARK_BAR_SUBMENU_ALWAYS_SHOW,
        IDS_BOOKMARK_BAR_SUBMENU_ALWAYS_SHOW);
    bookmark_bar_sub_menu_model_->AddCheckItemWithStringId(
        IDC_BOOKMARK_BAR_SUBMENU_ONLY_ON_NTP,
        IDS_BOOKMARK_BAR_SUBMENU_ONLY_ON_NTP);
    AddSubMenuWithStringIdAndIcon(
        IDC_BOOKMARK_BAR_SUBMENU, IDS_BOOKMARK_BAR_SUBMENU_LABEL,
        bookmark_bar_sub_menu_model_.get(),
        ui::ImageModel::FromVectorIcon(features::IsRoundedIconsEnabled()
                                           ? kToolbarIcon
                                           : kToolbarChromeRefreshOldIcon));
    SetElementIdentifierAt(
        GetIndexOfCommandId(IDC_BOOKMARK_BAR_SUBMENU).value(),
        kShowBookmarkBarMenuItem);
  } else {
    AddItemWithStringId(IDC_SHOW_BOOKMARK_BAR,
                        browser->profile()->GetPrefs()->GetBoolean(
                            bookmarks::prefs::kShowBookmarkBar)
                            ? IDS_HIDE_BOOKMARK_BAR
                            : IDS_SHOW_BOOKMARK_BAR);
    SetElementIdentifierAt(GetIndexOfCommandId(IDC_SHOW_BOOKMARK_BAR).value(),
                           kShowBookmarkBarMenuItem);
  }

  AddItemWithStringId(IDC_SHOW_BOOKMARK_SIDE_PANEL,
                      IDS_SHOW_BOOKMARK_SIDE_PANEL);
  SetElementIdentifierAt(
      GetIndexOfCommandId(IDC_SHOW_BOOKMARK_SIDE_PANEL).value(),
      kShowBookmarkSidePanelItem);

  if (features::IsMenuSimplificationEnabled()) {
    AddItemWithStringId(IDC_SHOW_BOOKMARK_MANAGER, IDS_BOOKMARK_MANAGER_V2);
  } else {
    AddItemWithStringId(IDC_SHOW_BOOKMARK_MANAGER, IDS_BOOKMARK_MANAGER);
  }

#if !BUILDFLAG(IS_CHROMEOS)
  AddItemWithStringId(IDC_IMPORT_SETTINGS, IDS_IMPORT_SETTINGS_MENU_LABEL);
#endif

  AddSeparator(ui::NORMAL_SEPARATOR);

  reading_list_sub_menu_model_ =
      std::make_unique<ReadingListSubMenuModel>(delegate());
  AddSubMenuWithStringIdAndIcon(
      AppMenuModel::kReadingListMenuPlaceholder, IDS_READING_LIST_MENU,
      reading_list_sub_menu_model_.get(),
      ui::ImageModel::FromVectorIcon(features::IsRoundedIconsEnabled()
                                         ? kListAltIcon
                                         : kReadingListOldIcon));
  SetElementIdentifierAt(
      GetIndexOfCommandId(AppMenuModel::kReadingListMenuPlaceholder).value(),
      kReadingListMenuItem);

  auto set_icon = [this](int command_id, const gfx::VectorIcon& vector_icon) {
    auto index = GetIndexOfCommandId(command_id);
    if (index) {
      SetIcon(index.value(), ui::ImageModel::FromVectorIcon(
                                 vector_icon, ui::kColorMenuIcon, 16));
    }
  };

  set_icon(IDC_BOOKMARK_THIS_TAB, features::IsRoundedIconsEnabled()
                                      ? kStarIcon
                                      : kBookmarksListsMenuOldIcon);
  set_icon(IDC_BOOKMARK_ALL_TABS, features::IsRoundedIconsEnabled()
                                      ? kHotelClassIcon
                                      : kBookmarkAllTabsChromeRefreshOldIcon);

  if (!base::FeatureList::IsEnabled(
          ntp_features::kNtpSimplificationBookmarkBar)) {
    set_icon(IDC_SHOW_BOOKMARK_BAR, features::IsRoundedIconsEnabled()
                                        ? kToolbarIcon
                                        : kToolbarChromeRefreshOldIcon);
  }

  set_icon(IDC_SHOW_BOOKMARK_MANAGER, features::IsRoundedIconsEnabled()
                                          ? kBookmarkManagerIcon
                                          : kBookmarksManagerOldIcon);
  set_icon(IDC_SHOW_BOOKMARK_SIDE_PANEL,
           features::IsRoundedIconsEnabled()
               ? kHotelClassIcon
               : kBookmarksSidePanelRefreshOldIcon);
  set_icon(IDC_IMPORT_SETTINGS, features::IsRoundedIconsEnabled()
                                    ? kMenuBookIcon
                                    : kMenuBookChromeRefreshOldIcon);
}
