// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/history_menu_bridge.h"

#include <stddef.h>
#include <string>

#include "base/apple/foundation_util.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/app/chrome_command_ids.h"  // IDC_HISTORY_MENU
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/app_controller_mac.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#import "chrome/browser/ui/cocoa/history_menu_cocoa_controller.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/resources/grit/ui_resources.h"

namespace {

// Number of days to consider when getting the number of visited items.
const int kVisitedScope = 90;

// The number of visited results to get.
const int kVisitedCount = 15;

// The number of recently closed items to get.
const unsigned int kRecentlyClosedCount = 10;

}  // namespace

HistoryMenuBridge::HistoryItem::HistoryItem()
    : session_id(SessionID::InvalidValue()) {}

HistoryMenuBridge::HistoryItem::HistoryItem(const HistoryItem& copy)
    : title(copy.title), url(copy.url), session_id(copy.session_id) {}

HistoryMenuBridge::HistoryItem::~HistoryItem() = default;

HistoryMenuBridge::HistoryMenuBridge(Profile* profile)
    : controller_([[HistoryMenuCocoaController alloc] initWithBridge:this]),
      profile_(profile) {
  DCHECK(profile_);
  profile_dir_ = profile_->GetPath();

  if (auto* profile_manager = g_browser_process->profile_manager())
    profile_manager_observation_.Observe(profile_manager);

  // Set the static icons in the menu.
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  NSMenuItem* full_history_item = [HistoryMenu() itemWithTag:IDC_SHOW_HISTORY];
  [full_history_item
      setImage:rb.GetNativeImageNamed(IDR_HISTORY_FAVICON).ToNSImage()];

  // Set the visibility of menu items according to profile type.
  // "Recently Visited", "Recently Closed" and "Show Full History" sections
  // should be hidden for incognito mode, while incognito disclaimer should be
  // visible.
  SetVisibilityOfMenuItems();

  // If the profile is incognito, no need to set history and tab restore
  // services.
  if (profile_->IsOffTheRecord())
    return;

  // Check to see if the history service is ready. Because it loads async, it
  // may not be ready when the Bridge is created. If this happens, register for
  // a notification that tells us the HistoryService is ready.
  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
  if (hs) {
    history_service_observation_.Observe(hs);
    history_service_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
        profile_, ProfileKeepAliveOrigin::kHistoryMenuBridge);
    if (hs->BackendLoaded()) {
      history_service_ = hs;
      Init();
    }
  }

  tab_restore_service_ = TabRestoreServiceFactory::GetForProfile(profile_);
  if (tab_restore_service_) {
    tab_restore_service_observation_.Observe(tab_restore_service_.get());
    // If the tab entries are already loaded, invoke the observer method to
    // build the "Recently Closed" section. Otherwise it will be when the
    // backend loads.
    if (!tab_restore_service_->IsLoaded()) {
      tab_restore_service_keep_alive_ =
          std::make_unique<ScopedProfileKeepAlive>(
              profile_, ProfileKeepAliveOrigin::kHistoryMenuBridge);
      tab_restore_service_->LoadTabsFromLastSession();
    } else {
      TabRestoreServiceChanged(tab_restore_service_);
    }
  }

  default_favicon_ = rb.GetNativeImageNamed(IDR_DEFAULT_FAVICON).ToNSImage();

  [HistoryMenu() setDelegate:controller_];
}

// Note that all requests sent to either the history service or the favicon
// service will be automatically cancelled by their respective Consumers, so
// task cancellation is not done manually here in the dtor.
HistoryMenuBridge::~HistoryMenuBridge() = default;

void HistoryMenuBridge::TabRestoreServiceChanged(
    sessions::TabRestoreService* service) {
  const sessions::TabRestoreService::Entries& entries = service->entries();

  // Clear the history menu before rebuilding.
  NSMenu* menu = HistoryMenu();
  ClearMenuSection(menu, kRecentlyClosed);

  // Index for the next menu item.
  NSInteger index = [menu indexOfItemWithTag:kRecentlyClosedTitle] + 1;
  NSUInteger added_count = 0;

  for (const auto& entry : entries) {
    if (added_count >= kRecentlyClosedCount)
      break;
    if (entry->type == sessions::tab_restore::Type::WINDOW) {
      bool added = AddWindowEntryToMenu(
          static_cast<sessions::tab_restore::Window*>(entry.get()), menu,
          kRecentlyClosed, index);
      if (added) {
        ++index;
        ++added_count;
      }
    } else if (entry->type == sessions::tab_restore::Type::TAB) {
      const auto& tab = static_cast<sessions::tab_restore::Tab&>(*entry);
      std::unique_ptr<HistoryItem> item = HistoryItemForTab(tab);
      if (item) {
        AddItemToMenu(std::move(item), menu, kRecentlyClosed, index++);
        ++added_count;
      }
    } else if (entry->type == sessions::tab_restore::Type::GROUP) {
      bool added = AddGroupEntryToMenu(
          static_cast<sessions::tab_restore::Group*>(entry.get()), menu,
          kRecentlyClosed, index);
      if (added) {
        ++index;
        ++added_count;
      }
    }
  }

  tab_restore_service_keep_alive_.reset();
}

void HistoryMenuBridge::TabRestoreServiceDestroyed(
    sessions::TabRestoreService* service) {
  DCHECK_EQ(service, tab_restore_service_);
  tab_restore_service_observation_.Reset();
  tab_restore_service_ = nullptr;
}

void HistoryMenuBridge::TabRestoreServiceLoaded(
    sessions::TabRestoreService* service) {
  // `TabRestoreServiceChanged()` is not called if the tab restore service is
  // empty when it is loaded. The menu still needs to be updated and
  // `tab_restore_service_keep_alive_` must be released.
  TabRestoreServiceChanged(service);
}

void HistoryMenuBridge::ResetMenu() {
  NSMenu* menu = HistoryMenu();
  ClearMenuSection(menu, kVisited);
  ClearMenuSection(menu, kRecentlyClosed);
  DCHECK(menu_item_map_.empty());
}

void HistoryMenuBridge::BuildMenu() {
  // If the history service is ready, use it. Otherwise, a Notification will
  // force an update when it's loaded.
  if (history_service_)
    CreateMenu();
}

HistoryMenuBridge::HistoryItem* HistoryMenuBridge::HistoryItemForMenuItem(
    NSMenuItem* item) {
  auto it = menu_item_map_.find(item);
  if (it != menu_item_map_.end()) {
    return it->second.get();
  }
  return nullptr;
}

void HistoryMenuBridge::SetIsMenuOpen(bool flag) {
  is_menu_open_ = flag;
  if (!is_menu_open_ && need_recreate_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&HistoryMenuBridge::CreateMenu,
                                  weak_factory_.GetWeakPtr()));
  }
}

history::HistoryService* HistoryMenuBridge::service() {
  return history_service_;
}

Profile* HistoryMenuBridge::profile() {
  return profile_;
}

const base::FilePath& HistoryMenuBridge::profile_dir() const {
  return profile_dir_;
}

NSMenu* HistoryMenuBridge::HistoryMenu() {
  NSMenu* history_menu = [[[NSApp mainMenu] itemWithTag:IDC_HISTORY_MENU]
                            submenu];
  return history_menu;
}

void HistoryMenuBridge::ClearMenuSection(NSMenu* menu, NSInteger tag) {
  for (NSMenuItem* menu_item in [menu itemArray]) {
    if ([menu_item tag] == tag && [menu_item target] == controller_) {
      // This is an item that should be removed, so find the corresponding model
      // item.
      HistoryItem* item = HistoryItemForMenuItem(menu_item);

      // Cancel favicon requests that could hold onto stale pointers. Also
      // remove the item from the mapping.
      if (item) {
        CancelFaviconRequest(item);
        menu_item_map_.erase(menu_item);
      }

      // If this menu item has a submenu, recurse.
      if ([menu_item hasSubmenu]) {
        ClearMenuSection([menu_item submenu], tag + 1);
      }

      // Now actually remove the item from the menu.
      [menu removeItem:menu_item];
    }
  }
}

NSMenuItem* HistoryMenuBridge::AddItemToMenu(std::unique_ptr<HistoryItem> item,
                                             NSMenu* menu,
                                             NSInteger tag,
                                             NSInteger index) {
  // Elide the title of the history item, or use the URL if there is none.
  const std::string& url = item->url.possibly_invalid_spec();
  const std::u16string& full_title = item->title;
  const std::u16string& title =
      full_title.empty() ? base::UTF8ToUTF16(url) : full_title;

  item->menu_item =
      [[NSMenuItem alloc] initWithTitle:base::SysUTF16ToNSString(title)
                                 action:nil
                          keyEquivalent:@""];
  [item->menu_item setTarget:controller_];
  [item->menu_item setAction:@selector(openHistoryMenuItem:)];
  [item->menu_item setTag:tag];
  if (item->icon) {
    [item->menu_item setImage:item->icon];
  } else if (item->tabs.empty()) {
    [item->menu_item setImage:default_favicon_];
  }

  // Add a tooltip if the history item is for a single tab.
  if (item->tabs.empty()) {
    NSString* tooltip = [NSString stringWithFormat:@"%@\n%@",
        base::SysUTF16ToNSString(full_title), base::SysUTF8ToNSString(url)];
    [item->menu_item setToolTip:tooltip];
  }

  [menu insertItem:item->menu_item atIndex:index];

  NSMenuItem* menu_item = item->menu_item;
  auto it = menu_item_map_.emplace(menu_item, std::move(item));
  CHECK(it.second);
  return menu_item;
}

bool HistoryMenuBridge::AddWindowEntryToMenu(
    sessions::tab_restore::Window* window,
    NSMenu* menu,
    NSInteger tag,
    NSInteger index) {
  const std::vector<std::unique_ptr<sessions::tab_restore::Tab>>& tabs =
      window->tabs;
  if (tabs.empty())
    return false;

  // Create the item for the parent/window. Do not set the title yet because
  // the actual number of items that are in the menu will not be known until
  // things like the NTP are filtered out, which is done when the tab items
  // are actually created.
  auto item = std::make_unique<HistoryItem>();
  item->session_id = window->id;

  // Create the submenu.
  NSMenu* submenu = [[NSMenu alloc] init];
  int added_count = AddTabsToSubmenu(submenu, item.get(), tabs);

  // Sometimes it is possible for there to not be any subitems for a given
  // window; if that is the case, do not add the entry to the main menu.
  if (added_count == 0)
    return false;

  // Now that the number of tabs that has been added is known, set the title
  // of the parent menu item.
  item->title = l10n_util::GetPluralStringFUTF16(IDS_RECENTLY_CLOSED_WINDOW,
                                                 item->tabs.size());

  // Create the menu item parent.
  NSMenuItem* parent_item = AddItemToMenu(std::move(item), menu, tag, index);
  parent_item.submenu = submenu;
  return true;
}

bool HistoryMenuBridge::AddGroupEntryToMenu(sessions::tab_restore::Group* group,
                                            NSMenu* menu,
                                            NSInteger tag,
                                            NSInteger index) {
  const std::vector<std::unique_ptr<sessions::tab_restore::Tab>>& tabs =
      group->tabs;
  if (tabs.empty())
    return false;

  // Create the item for the parent/group.
  auto item = std::make_unique<HistoryItem>();
  item->session_id = group->id;

  // Set the title of the group.
  if (group->visual_data.title().empty()) {
    item->title = l10n_util::GetPluralStringFUTF16(
        IDS_RECENTLY_CLOSED_GROUP_UNNAMED, tabs.size());
  } else {
    item->title = l10n_util::GetPluralStringFUTF16(IDS_RECENTLY_CLOSED_GROUP,
                                                   tabs.size());
    item->title = base::ReplaceStringPlaceholders(
        item->title, {group->visual_data.title()}, nullptr);
  }

  // Set the icon of the group to the group color circle.
  const auto& color_provider =
      [AppController.sharedController lastActiveColorProvider];
  const ui::ColorId color_id =
      GetTabGroupContextMenuColorId(group->visual_data.color());
  gfx::ImageSkia group_icon = gfx::CreateVectorIcon(
      kTabGroupIcon, gfx::kFaviconSize, color_provider.GetColor(color_id));

  // Create the submenu.
  NSMenu* submenu = [[NSMenu alloc] init];
  AddTabsToSubmenu(submenu, item.get(), tabs);

  NSImage* image = NSImageFromImageSkia(group_icon);
  item->icon = image;
  [item->menu_item setImage:item->icon];

  // Create the menu item parent.
  NSMenuItem* parent_item = AddItemToMenu(std::move(item), menu, tag, index);
  [parent_item setSubmenu:submenu];
  return true;
}

int HistoryMenuBridge::AddTabsToSubmenu(
    NSMenu* submenu,
    HistoryItem* item,
    const std::vector<std::unique_ptr<sessions::tab_restore::Tab>>& tabs) {
  // Create standard items within the submenu.
  // Duplicate the HistoryItem otherwise the different NSMenuItems will
  // point to the same HistoryItem, which would then be double-freed when
  // removing the items from the map or in the dtor.
  auto restore_item = std::make_unique<HistoryItem>(*item);
  NSString* restore_title =
      l10n_util::GetNSString(IDS_HISTORY_CLOSED_RESTORE_WINDOW_MAC);
  restore_item->menu_item =
      [[NSMenuItem alloc] initWithTitle:restore_title
                                 action:@selector(openHistoryMenuItem:)
                          keyEquivalent:@""];
  NSMenuItem* restore_menu_item = restore_item->menu_item;
  [restore_menu_item setTag:kRecentlyClosed + 1];  // +1 for submenu item.
  [restore_menu_item setTarget:controller_];
  auto it = menu_item_map_.emplace(restore_menu_item, std::move(restore_item));
  CHECK(it.second);
  [submenu addItem:restore_menu_item];
  [submenu addItem:[NSMenuItem separatorItem]];

  // Loop over the tabs and add them to the submenu. This filters out
  // uninteresting tabs like the NTP.
  NSInteger subindex = [[submenu itemArray] count];
  int added_count = 0;
  for (const auto& tab : tabs) {
    std::unique_ptr<HistoryItem> tab_item = HistoryItemForTab(*tab);
    if (tab_item) {
      item->tabs.push_back(tab_item.get());
      AddItemToMenu(std::move(tab_item), submenu, kRecentlyClosed + 1,
                    subindex++);
      ++added_count;
    }
  }

  return added_count;
}

void HistoryMenuBridge::Init() {
  DCHECK(history_service_);
  need_recreate_ = true;
  CreateMenu();
}

void HistoryMenuBridge::CreateMenu() {
  // If we're currently running CreateMenu(), wait until it finishes.
  // If the menu is currently open, wait until it closes.
  // If the history service got torn down while our async task was queued, don't
  // do anything - the browser is exiting anyway.
  // If the current profile is incognito, do not fill the menu.
  if (create_in_progress_ || is_menu_open_ || !history_service_ ||
      profile_->IsOffTheRecord()) {
    return;
  }

  // Under the right conditions, such as the Speedometer 3 benchmark,
  // OnHistoryChanged() calls CreateMenu() many times in rapid succession. With
  // this timer, FinishCreateMenu() will execute once 750ms have elapsed
  // without a new CreateMenu() call. 750ms is long enough to coalesce the bulk
  // of the successive CreateMenu() requests, but not too long from a user's
  // perspective.
  finish_create_menu_timer_.Stop();
  finish_create_menu_timer_.Start(
      FROM_HERE, base::Milliseconds(750),
      base::BindOnce(&HistoryMenuBridge::FinishCreateMenu,
                     base::Unretained(this)));
}

void HistoryMenuBridge::FinishCreateMenu() {
  // If the user opens the menu right before we try to update it, defer the
  // update until later (SetIsMenuOpen() will call CreateMenu() as needed).
  if (is_menu_open_) {
    return;
  }
  create_in_progress_ = true;
  need_recreate_ = false;

  history::QueryOptions options;
  options.max_count = kVisitedCount;
  options.SetRecentDayRange(kVisitedScope);

  history_service_->QueryHistory(
      std::u16string(), options,
      base::BindOnce(&HistoryMenuBridge::OnVisitedHistoryResults,
                     base::Unretained(this)),
      &cancelable_task_tracker_);
}

void HistoryMenuBridge::OnHistoryChanged() {
  // History has changed, rebuild menu.
  need_recreate_ = true;
  CreateMenu();
}

void HistoryMenuBridge::OnVisitedHistoryResults(history::QueryResults results) {
  // It's possible for history_service_ to have been destroyed while our request
  // was waiting to be returned to us, because both the initial request *and the
  // delivery of the reply* from the service are async - i.e., this can happen:
  // 1. We call HistoryService::QueryHistory()
  // 2. That message loop runs, the query happens, the reply to us is posted
  // 3. HistoryService is destroyed
  // 4. The posted reply to us arrives
  // To guard against that, check for history_service_ here.
  if (!history_service_) {
    return;
  }

  NSMenu* menu = HistoryMenu();
  ClearMenuSection(menu, kVisited);
  NSInteger top_item = [menu indexOfItemWithTag:kVisitedTitle] + 1;

  size_t count = results.size();
  // Loop through all the items. Early out if the menu changes while we're
  // rebuilding it.
  for (size_t i = 0; i < count && !need_recreate_; ++i) {
    const history::URLResult& result = results[i];

    auto item = std::make_unique<HistoryItem>();
    item->title = result.title();
    item->url = result.url();

    // Need to explicitly get the favicon for each row.
    GetFaviconForHistoryItem(item.get());

    // This will add |item| to the |menu_item_map_|, which takes ownership.
    AddItemToMenu(std::move(item), HistoryMenu(), kVisited, top_item + i);
  }

  // We are already invalid by the time we finished, darn.
  if (need_recreate_) {
    CreateMenu();
  } else {
    history_service_keep_alive_.reset();
  }

  create_in_progress_ = false;
}

std::unique_ptr<HistoryMenuBridge::HistoryItem>
HistoryMenuBridge::HistoryItemForTab(const sessions::tab_restore::Tab& entry) {
  DCHECK(!entry.navigations.empty());

  const sessions::SerializedNavigationEntry& current_navigation =
      entry.navigations.at(entry.current_navigation_index);
  auto item = std::make_unique<HistoryItem>();
  item->title = current_navigation.title();
  item->url = current_navigation.virtual_url();
  item->session_id = entry.id;

  // Tab navigations don't come with icons, so we always have to request them.
  GetFaviconForHistoryItem(item.get());

  return item;
}

void HistoryMenuBridge::GetFaviconForHistoryItem(HistoryItem* item) {
  favicon::FaviconService* service = FaviconServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
  base::CancelableTaskTracker::TaskId task_id =
      service->GetFaviconImageForPageURL(
          item->url,
          base::BindOnce(&HistoryMenuBridge::GotFaviconData,
                         base::Unretained(this), item),
          &cancelable_task_tracker_);
  item->icon_task_id = task_id;
  item->icon_requested = true;
}

void HistoryMenuBridge::GotFaviconData(
    HistoryItem* item,
    const favicon_base::FaviconImageResult& image_result) {
  // Since we're going to do Cocoa-y things, make sure this is the main thread.
  DCHECK([NSThread isMainThread]);

  DCHECK(item);
  item->icon_requested = false;
  item->icon_task_id = base::CancelableTaskTracker::kBadTaskId;

  NSImage* image = image_result.image.AsNSImage();
  if (image) {
    item->icon = image;
    [item->menu_item setImage:item->icon];
  }
}

void HistoryMenuBridge::CancelFaviconRequest(HistoryItem* item) {
  DCHECK(item);
  if (item->icon_requested) {
    cancelable_task_tracker_.TryCancel(item->icon_task_id);
    item->icon_requested = false;
    item->icon_task_id = base::CancelableTaskTracker::kBadTaskId;
  }
}

void HistoryMenuBridge::OnURLVisited(history::HistoryService* history_service,
                                     const history::URLRow& url_row,
                                     const history::VisitRow& new_visit) {
  OnHistoryChanged();
}

void HistoryMenuBridge::OnURLsModified(history::HistoryService* history_service,
                                       const history::URLRows& changed_urls) {
  OnHistoryChanged();
}

void HistoryMenuBridge::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  OnHistoryChanged();
}

void HistoryMenuBridge::OnHistoryServiceLoaded(
    history::HistoryService* history_service) {
  history_service_ = history_service;
  Init();
}

void HistoryMenuBridge::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  history_service_observation_.Reset();
  history_service_ = nullptr;
}

void HistoryMenuBridge::SetVisibilityOfMenuItems() {
  NSMenu* menu = HistoryMenu();
  for (int i = 0; i < [menu numberOfItems]; i++) {
    NSMenuItem* item = [menu itemAtIndex:i];
    [item setHidden:!ShouldMenuItemBeVisible(item)];
  }
}

bool HistoryMenuBridge::ShouldMenuItemBeVisible(NSMenuItem* item) {
  int tag = [item tag];
  switch (tag) {
    // The common menu items for both profiles
    case IDC_HOME:
    case IDC_BACK:
    case IDC_FORWARD:
      return true;
    // The original profile specific menu items
    case kRecentlyClosedSeparator:
    case kRecentlyClosedTitle:
    case kVisitedSeparator:
    case kVisitedTitle:
    case kShowFullSeparator:
    case IDC_SHOW_HISTORY:
      return !profile_->IsOffTheRecord();
  }

  // When a new menu item is introduced, it should be added to one of the cases
  // above.
  NOTREACHED_IN_MIGRATION();
  return false;
}

void HistoryMenuBridge::OnProfileMarkedForPermanentDeletion(Profile* profile) {
  if (profile != profile_)
    return;
  ResetMenu();
}

void HistoryMenuBridge::OnProfileManagerDestroying() {
  profile_manager_observation_.Reset();
}

void HistoryMenuBridge::OnProfileWillBeDestroyed() {
  profile_ = nullptr;
  history_service_observation_.Reset();
  history_service_ = nullptr;
  tab_restore_service_observation_.Reset();
  tab_restore_service_ = nullptr;
  finish_create_menu_timer_.Stop();
}
