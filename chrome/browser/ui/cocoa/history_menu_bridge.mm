// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/history_menu_bridge.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/app/chrome_command_ids.h"  // IDC_HISTORY_MENU
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#import "chrome/browser/ui/cocoa/history_menu_cocoa_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
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
    : icon_requested(false),
      icon_task_id(base::CancelableTaskTracker::kBadTaskId),
      menu_item(nil),
      session_id(SessionID::InvalidValue()) {}

HistoryMenuBridge::HistoryItem::HistoryItem(const HistoryItem& copy)
    : title(copy.title),
      url(copy.url),
      icon_requested(false),
      icon_task_id(base::CancelableTaskTracker::kBadTaskId),
      menu_item(nil),
      session_id(copy.session_id) {}

HistoryMenuBridge::HistoryItem::~HistoryItem() {
}

HistoryMenuBridge::HistoryMenuBridge(Profile* profile)
    : controller_([[HistoryMenuCocoaController alloc] initWithBridge:this]),
      profile_(profile),
      history_service_(NULL),
      tab_restore_service_(NULL),
      create_in_progress_(false),
      need_recreate_(false),
      history_service_observer_(this) {
  // If we don't have a profile, do not bother initializing our data sources.
  // This shouldn't happen except in unit tests.
  if (profile_) {
    // Check to see if the history service is ready. Because it loads async, it
    // may not be ready when the Bridge is created. If this happens, register
    // for a notification that tells us the HistoryService is ready.
    history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
        profile_, ServiceAccessType::EXPLICIT_ACCESS);
    if (hs) {
      history_service_observer_.Add(hs);
      if (hs->BackendLoaded()) {
        history_service_ = hs;
        Init();
      }
    }

    tab_restore_service_ = TabRestoreServiceFactory::GetForProfile(profile_);
    if (tab_restore_service_) {
      tab_restore_service_->AddObserver(this);
      // If the tab entries are already loaded, invoke the observer method to
      // build the "Recently Closed" section. Otherwise it will be when the
      // backend loads.
      if (!tab_restore_service_->IsLoaded())
        tab_restore_service_->LoadTabsFromLastSession();
      else
        TabRestoreServiceChanged(tab_restore_service_);
    }
  }

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  default_favicon_.reset(
      [rb.GetNativeImageNamed(IDR_DEFAULT_FAVICON).ToNSImage() retain]);

  // Set the static icons in the menu.
  NSMenuItem* item = [HistoryMenu() itemWithTag:IDC_SHOW_HISTORY];
  [item setImage:rb.GetNativeImageNamed(IDR_HISTORY_FAVICON).ToNSImage()];

  [HistoryMenu() setDelegate:controller_];
}

// Note that all requests sent to either the history service or the favicon
// service will be automatically cancelled by their respective Consumers, so
// task cancellation is not done manually here in the dtor.
HistoryMenuBridge::~HistoryMenuBridge() {
  // Unregister ourselves as observers and notifications.
  DCHECK(profile_);

  if (tab_restore_service_)
    tab_restore_service_->RemoveObserver(this);

  // Since the map owns the HistoryItems, delete anything that still exists.
  std::map<NSMenuItem*, HistoryItem*>::iterator it = menu_item_map_.begin();
  while (it != menu_item_map_.end()) {
    HistoryItem* item  = it->second;
    menu_item_map_.erase(it++);
    delete item;
  }
}

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
    // If this is a window, create a submenu for all of its tabs.
    if (entry->type == sessions::TabRestoreService::WINDOW) {
      const auto* entry_win =
          static_cast<sessions::TabRestoreService::Window*>(entry.get());
      const std::vector<std::unique_ptr<sessions::TabRestoreService::Tab>>&
          tabs = entry_win->tabs;
      if (tabs.empty())
        continue;

      // Create the item for the parent/window. Do not set the title yet because
      // the actual number of items that are in the menu will not be known until
      // things like the NTP are filtered out, which is done when the tab items
      // are actually created.
      HistoryItem* item = new HistoryItem();
      item->session_id = entry_win->id;

      // Create the submenu.
      base::scoped_nsobject<NSMenu> submenu([[NSMenu alloc] init]);

      // Create standard items within the window submenu.
      NSString* restore_title = l10n_util::GetNSString(
          IDS_HISTORY_CLOSED_RESTORE_WINDOW_MAC);
      base::scoped_nsobject<NSMenuItem> restore_item(
          [[NSMenuItem alloc] initWithTitle:restore_title
                                     action:@selector(openHistoryMenuItem:)
                              keyEquivalent:@""]);
      [restore_item setTarget:controller_.get()];
      // Duplicate the HistoryItem otherwise the different NSMenuItems will
      // point to the same HistoryItem, which would then be double-freed when
      // removing the items from the map or in the dtor.
      HistoryItem* dup_item = new HistoryItem(*item);
      menu_item_map_.insert(std::make_pair(restore_item.get(), dup_item));
      [submenu addItem:restore_item.get()];
      [submenu addItem:[NSMenuItem separatorItem]];

      // Loop over the window's tabs and add them to the submenu.
      NSInteger subindex = [[submenu itemArray] count];
      for (const auto& tab : tabs) {
        HistoryItem* tab_item = HistoryItemForTab(*tab);
        if (tab_item) {
          item->tabs.push_back(tab_item);
          AddItemToMenu(tab_item, submenu.get(), kRecentlyClosed + 1,
                        subindex++);
        }
      }

      // Now that the number of tabs that has been added is known, set the title
      // of the parent menu item.
      item->title = l10n_util::GetPluralStringFUTF16(
          IDS_RECENTLY_CLOSED_WINDOW, item->tabs.size());

      // Sometimes it is possible for there to not be any subitems for a given
      // window; if that is the case, do not add the entry to the main menu.
      if ([[submenu itemArray] count] > 2) {
        // Create the menu item parent.
        NSMenuItem* parent_item =
            AddItemToMenu(item, menu, kRecentlyClosed, index++);
        [parent_item setSubmenu:submenu.get()];
        ++added_count;
      }
    } else if (entry->type == sessions::TabRestoreService::TAB) {
      const auto& tab = static_cast<sessions::TabRestoreService::Tab&>(*entry);
      HistoryItem* item = HistoryItemForTab(tab);
      if (item) {
        AddItemToMenu(item, menu, kRecentlyClosed, index++);
        ++added_count;
      }
    }
  }
}

void HistoryMenuBridge::TabRestoreServiceDestroyed(
    sessions::TabRestoreService* service) {
  // Intentionally left blank. We hold a weak reference to the service.
}

void HistoryMenuBridge::ResetMenu() {
  NSMenu* menu = HistoryMenu();
  ClearMenuSection(menu, kVisited);
  ClearMenuSection(menu, kRecentlyClosed);
}

void HistoryMenuBridge::BuildMenu() {
  // If the history service is ready, use it. Otherwise, a Notification will
  // force an update when it's loaded.
  if (history_service_)
    CreateMenu();
}

HistoryMenuBridge::HistoryItem* HistoryMenuBridge::HistoryItemForMenuItem(
    NSMenuItem* item) {
  std::map<NSMenuItem*, HistoryItem*>::iterator it = menu_item_map_.find(item);
  if (it != menu_item_map_.end()) {
    return it->second;
  }
  return NULL;
}

void HistoryMenuBridge::SetIsMenuOpen(bool flag) {
  is_menu_open_ = flag;
  if (!is_menu_open_ && need_recreate_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&HistoryMenuBridge::CreateMenu, base::Unretained(this)));
  }
}

history::HistoryService* HistoryMenuBridge::service() {
  return history_service_;
}

Profile* HistoryMenuBridge::profile() {
  return profile_;
}

NSMenu* HistoryMenuBridge::HistoryMenu() {
  NSMenu* history_menu = [[[NSApp mainMenu] itemWithTag:IDC_HISTORY_MENU]
                            submenu];
  return history_menu;
}

void HistoryMenuBridge::ClearMenuSection(NSMenu* menu, NSInteger tag) {
  for (NSMenuItem* menu_item in [menu itemArray]) {
    if ([menu_item tag] == tag  && [menu_item target] == controller_.get()) {
      // This is an item that should be removed, so find the corresponding model
      // item.
      HistoryItem* item = HistoryItemForMenuItem(menu_item);

      // Cancel favicon requests that could hold onto stale pointers. Also
      // remove the item from the mapping.
      if (item) {
        CancelFaviconRequest(item);
        menu_item_map_.erase(menu_item);
        delete item;
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

NSMenuItem* HistoryMenuBridge::AddItemToMenu(HistoryItem* item,
                                             NSMenu* menu,
                                             NSInteger tag,
                                             NSInteger index) {
  // Elide the title of the history item, or use the URL if there is none.
  const std::string& url = item->url.possibly_invalid_spec();
  const base::string16& full_title = item->title;
  const base::string16& title =
      full_title.empty() ? base::UTF8ToUTF16(url) : full_title;

  item->menu_item.reset(
      [[NSMenuItem alloc] initWithTitle:base::SysUTF16ToNSString(title)
                                 action:nil
                          keyEquivalent:@""]);
  [item->menu_item setTarget:controller_];
  [item->menu_item setAction:@selector(openHistoryMenuItem:)];
  [item->menu_item setTag:tag];
  if (item->icon.get())
    [item->menu_item setImage:item->icon.get()];
  else if (item->tabs.empty())
    [item->menu_item setImage:default_favicon_.get()];

  // Add a tooltip if the history item is for a single tab.
  if (item->tabs.empty()) {
    NSString* tooltip = [NSString stringWithFormat:@"%@\n%@",
        base::SysUTF16ToNSString(full_title), base::SysUTF8ToNSString(url)];
    [item->menu_item setToolTip:tooltip];
  }

  [menu insertItem:item->menu_item.get() atIndex:index];
  menu_item_map_.insert(std::make_pair(item->menu_item.get(), item));

  return item->menu_item.get();
}

void HistoryMenuBridge::Init() {
  DCHECK(history_service_);
}

void HistoryMenuBridge::CreateMenu() {
  // If we're currently running CreateMenu(), wait until it finishes.
  // If the menu is currently open, wait until it closes.
  if (create_in_progress_ || is_menu_open_)
    return;
  create_in_progress_ = true;
  need_recreate_ = false;

  DCHECK(history_service_);

  history::QueryOptions options;
  options.max_count = kVisitedCount;
  options.SetRecentDayRange(kVisitedScope);

  history_service_->QueryHistory(
      base::string16(), options,
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
  NSMenu* menu = HistoryMenu();
  ClearMenuSection(menu, kVisited);
  NSInteger top_item = [menu indexOfItemWithTag:kVisitedTitle] + 1;

  size_t count = results.size();
  for (size_t i = 0; i < count; ++i) {
    const history::URLResult& result = results[i];

    HistoryItem* item = new HistoryItem;
    item->title = result.title();
    item->url = result.url();

    // Need to explicitly get the favicon for each row.
    GetFaviconForHistoryItem(item);

    // This will add |item| to the |menu_item_map_|, which takes ownership.
    AddItemToMenu(item, HistoryMenu(), kVisited, top_item + i);
  }

  // We are already invalid by the time we finished, darn.
  if (need_recreate_)
    CreateMenu();

  create_in_progress_ = false;
}

HistoryMenuBridge::HistoryItem* HistoryMenuBridge::HistoryItemForTab(
    const sessions::TabRestoreService::Tab& entry) {
  DCHECK(!entry.navigations.empty());

  const sessions::SerializedNavigationEntry& current_navigation =
      entry.navigations.at(entry.current_navigation_index);
  HistoryItem* item = new HistoryItem();
  item->title = current_navigation.title();
  item->url = current_navigation.virtual_url();
  item->session_id = entry.id;

  // Tab navigations don't come with icons, so we always have to request them.
  GetFaviconForHistoryItem(item);

  return item;
}

void HistoryMenuBridge::GetFaviconForHistoryItem(HistoryItem* item) {
  favicon::FaviconService* service = FaviconServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
  base::CancelableTaskTracker::TaskId task_id =
      service->GetFaviconImageForPageURL(
          item->url,
          base::Bind(
              &HistoryMenuBridge::GotFaviconData, base::Unretained(this), item),
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
    item->icon.reset([image retain]);
    [item->menu_item setImage:item->icon.get()];
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
                                     ui::PageTransition transition,
                                     const history::URLRow& row,
                                     const history::RedirectList& redirects,
                                     base::Time visit_time) {
  OnHistoryChanged();
}

void HistoryMenuBridge::OnURLsModified(history::HistoryService* history_service,
                                       const history::URLRows& changed_urls) {
  OnHistoryChanged();
}

void HistoryMenuBridge::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  OnHistoryChanged();
}

void HistoryMenuBridge::OnHistoryServiceLoaded(
    history::HistoryService* history_service) {
  history_service_ = history_service;
  Init();
}
