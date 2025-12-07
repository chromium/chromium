// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/tab_group_menu_bridge.h"

#include "base/apple/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/app_controller_mac.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_metrics.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_action_context_desktop.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_menu_utils.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/grit/generated_resources.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#include "ui/gfx/paint_vector_icon.h"

using MenuItemCallback = base::RepeatingCallback<void(NSMenuItem*)>;

@interface MenuItemListener : NSObject
- (instancetype)initWithCallback:(MenuItemCallback)callback;
- (void)onMenuItem:(id)sender;
@end

@implementation MenuItemListener {
  MenuItemCallback _callback;
}

- (instancetype)initWithCallback:(MenuItemCallback)callback {
  if ((self = [super init])) {
    _callback = callback;
  }
  return self;
}

- (IBAction)onMenuItem:(id)sender {
  _callback.Run(sender);
}

@end

// This class is responsible to build the dynamic menu items of the tab groups
// menu and keep them updated according to tab group changes.
TabGroupMenuBridge::TabGroupMenuBridge(
    Profile* profile,
    tab_groups::TabGroupSyncService* tab_group_service)
    : profile_(profile),
      tab_group_service_(tab_group_service),
      favicon_service_(FaviconServiceFactory::GetForProfile(
          profile_,
          ServiceAccessType::EXPLICIT_ACCESS)) {
  observation_.Observe(tab_group_service_);

  menu_listener_ = [[MenuItemListener alloc]
      initWithCallback:base::BindRepeating(
                           &TabGroupMenuBridge::OnMenuItem,
                           // Unretained is safe here: this class owns
                           // MenuListener, which holds the callback
                           // being constructed here, so the callback
                           // will be destructed before this class.
                           base::Unretained(this))];
}

TabGroupMenuBridge::~TabGroupMenuBridge() {
  ResetMenu();
}

NSMenu* TabGroupMenuBridge::TabGroupsMenu() {
  NSMenu* tab_groups_menu =
      [[[NSApp mainMenu] itemWithTag:IDC_SAVED_TAB_GROUPS_MENU] submenu];
  return tab_groups_menu;
}

void TabGroupMenuBridge::ResetMenu() {
  favicon_tracker_.TryCancelAll();
  NSMenu* menu = TabGroupsMenu();
  // Remove all menu items except create new tab group.
  for (NSMenuItem* menu_item in [menu itemArray]) {
    if ([menu_item tag] != IDC_CREATE_NEW_TAB_GROUP) {
      menu_item_map_.erase(menu_item);
      [menu removeItem:menu_item];
    }
  }
}

void TabGroupMenuBridge::BuildMenu() {
  ResetMenu();

  NSMenu* menu = TabGroupsMenu();

  if (tab_group_service_->GetAllGroups().empty()) {
    return;
  }

  std::vector<base::Uuid> group_ids =
      tab_groups::TabGroupMenuUtils::GetGroupsForDisplaySortedByCreationTime(
          tab_group_service_);
  if (group_ids.empty()) {
    return;
  }

  [menu addItem:[NSMenuItem separatorItem]];

  for (const base::Uuid& uuid : group_ids) {
    const std::optional<tab_groups::SavedTabGroup> group =
        tab_group_service_->GetGroup(uuid);
    if (!group) {
      continue;
    }

    NSString* title = base::SysUTF16ToNSString(
        tab_groups::TabGroupMenuUtils::GetMenuTextForGroup(*group));

    // Add menu item for each group.
    NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title
                                                  action:nil
                                           keyEquivalent:@""];
    // Set the icon of the group to the group color circle.
    const auto& color_provider =
        [AppController.sharedController lastActiveColorProvider];
    const ui::ColorId color_id = GetTabGroupContextMenuColorId(group->color());
    gfx::ImageSkia group_icon = gfx::CreateVectorIcon(
        kTabGroupIcon, gfx::kFaviconSize, color_provider.GetColor(color_id));
    item.image = NSImageFromImageSkia(group_icon);

    NSMenu* submenu = [[NSMenu alloc] init];
    // Add static menu items for submenu.
    [submenu addItem:CreateStaticSubmenuItem(
                         IDS_OPEN_GROUP_IN_BROWSER_MENU,
                         TabGroupMenuAction::Type::OPEN_IN_BROWSER, uuid)];
    [submenu
        addItem:CreateStaticSubmenuItem(
                    group->local_group_id().has_value()
                        ? IDS_TAB_GROUP_HEADER_CXMENU_MOVE_GROUP_TO_NEW_WINDOW
                        : IDS_TAB_GROUP_HEADER_CXMENU_OPEN_GROUP_IN_NEW_WINDOW,
                    TabGroupMenuAction::Type::OPEN_OR_MOVE_TO_NEW_WINDOW,
                    uuid)];
    [submenu
        addItem:CreateStaticSubmenuItem(
                    group->is_pinned() ? IDS_TAB_GROUP_HEADER_CXMENU_UNPIN_GROUP
                                       : IDS_TAB_GROUP_HEADER_CXMENU_PIN_GROUP,
                    TabGroupMenuAction::Type::PIN_OR_UNPIN_GROUP, uuid)];
    bool is_owner =
        tab_groups::SavedTabGroupUtils::IsOwnerOfSharedTabGroup(profile_, uuid);
    [submenu addItem:CreateStaticSubmenuItem(
                         is_owner ? IDS_TAB_GROUP_HEADER_CXMENU_DELETE_GROUP
                                  : IDS_DATA_SHARING_LEAVE_GROUP,
                         is_owner ? TabGroupMenuAction::Type::DELETE_GROUP
                                  : TabGroupMenuAction::Type::LEAVE_GROUP,
                         uuid)];
    [submenu addItem:[NSMenuItem separatorItem]];

    // Add menu items for each tab in submenu.
    for (const tab_groups::SavedTabGroupTab& tab : group->saved_tabs()) {
      NSMenuItem* tab_menu_item = [[NSMenuItem alloc]
          initWithTitle:base::SysUTF16ToNSString(
                            tab_groups::TabGroupMenuUtils::GetMenuTextForTab(
                                tab))
                 action:@selector(onMenuItem:)
          keyEquivalent:@""];
      tab_menu_item.target = menu_listener_;

      const ui::ImageModel image = favicon::GetDefaultFaviconModel(
          GetTabGroupBookmarkColorId(group->color()));
      tab_menu_item.image =
          NSImageFromImageSkia(image.Rasterize(&color_provider));

      if (favicon_service_) {
        favicon_service_->GetFaviconImageForPageURL(
            tab.url(),
            base::BindOnce(&TabGroupMenuBridge::OnFaviconReady,
                           // Unretained is safe here because favicon_tracker_
                           // will cancel all ongoing requests before the menu
                           // is rebuilt or the class is destroyed.
                           base::Unretained(this),
                           base::Unretained(tab_menu_item)),
            &favicon_tracker_);
      }

      [submenu addItem:tab_menu_item];
      menu_item_map_.emplace(
          tab_menu_item,
          TabGroupMenuAction{TabGroupMenuAction::Type::OPEN_URL, tab.url()});
    }
    item.submenu = submenu;

    [menu addItem:item];
  }
}

void TabGroupMenuBridge::OnFaviconReady(
    NSMenuItem* menu_item,
    const favicon_base::FaviconImageResult& result) {
  if (!result.image.IsEmpty()) {
    menu_item.image = result.image.ToNSImage();
  }
}

void TabGroupMenuBridge::OnInitialized() {
  BuildMenu();
}

void TabGroupMenuBridge::OnTabGroupAdded(const tab_groups::SavedTabGroup& group,
                                         tab_groups::TriggerSource source) {
  BuildMenu();
}

void TabGroupMenuBridge::OnTabGroupUpdated(
    const tab_groups::SavedTabGroup& group,
    tab_groups::TriggerSource source) {
  BuildMenu();
}

void TabGroupMenuBridge::OnTabGroupRemoved(const base::Uuid& sync_id,
                                           tab_groups::TriggerSource source) {
  BuildMenu();
}

void TabGroupMenuBridge::OnMenuItem(NSMenuItem* item) {
  auto it = menu_item_map_.find(item);
  if (it == menu_item_map_.end()) {
    return;
  }

  Browser* browser = chrome::FindLastActive();
  if (!browser) {
    return;
  }

  tab_groups::TabGroupMenuAction action = it->second;
  tab_groups::SavedTabGroupUtils::PerformTabGroupMenuAction(
      action, tab_groups::TabGroupMenuContext::MAC_SYSTEM_MENU, browser,
      tab_group_service_);
}

NSMenuItem* TabGroupMenuBridge::CreateStaticSubmenuItem(
    int string_id,
    TabGroupMenuAction::Type type,
    const base::Uuid& uuid) {
  NSString* title = l10n_util::GetNSStringWithFixup(string_id);
  NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title
                                                action:@selector(onMenuItem:)
                                         keyEquivalent:@""];
  item.target = menu_listener_;
  menu_item_map_.emplace(item, TabGroupMenuAction{type, uuid});
  return item;
}
