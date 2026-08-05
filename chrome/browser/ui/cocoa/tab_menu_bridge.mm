// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/tab_menu_bridge.h"

#import <Cocoa/Cocoa.h>

#include "base/functional/callback.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/cocoa/group_menu_util.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#include "ui/gfx/mac/menu_text_elider_mac.h"

using MenuItemCallback = base::RepeatingCallback<void(NSMenuItem*)>;

namespace {

void UpdateItemForWebContents(NSMenuItem* item,
                              content::WebContents* web_contents,
                              TabStripModel* tab_strip_model) {
  tabs::TabInterface* const tab_interface =
      tabs::TabInterface::GetFromContents(web_contents);
  TabUIHelper* const tab_ui_helper = TabUIHelper::From(tab_interface);

  auto* audio_helper = RecentlyAudibleHelper::FromWebContents(web_contents);
  if (audio_helper && audio_helper->WasRecentlyAudible()) {
    // If this webcontents is or was recently playing audio, append either a
    // speaker-playing-sound icon or a muted-speaker icon to its title to make
    // it easy to find the tabs playing sound in the Tab menu.
    int title_id;
    std::u16string emoji;
    if (web_contents->IsAudioMuted()) {
      title_id = IDS_WINDOW_AUDIO_MUTING_MAC;
      emoji = u"\U0001F507";
    } else {
      title_id = IDS_WINDOW_AUDIO_PLAYING_MAC;
      emoji = u"\U0001F50A";
    }

    item.title = l10n_util::GetNSStringF(
        title_id, gfx::ElideMenuItemTitle(tab_ui_helper->GetTitle()), emoji);
  } else {
    item.title = base::SysUTF16ToNSString(
        gfx::ElideMenuItemTitle(tab_ui_helper->GetTitle()));
  }


  item.image = NSImageFromImageSkia(
      tab_ui_helper->GetFavicon().Rasterize(&web_contents->GetColorProvider()));
}

void RemoveMenuItems(NSArray* menu_items) {
  NSMenu* tab_menu = [[menu_items firstObject] menu];

  for (NSMenuItem* item in menu_items) {
    [tab_menu removeItem:item];
  }
}

}  // namespace

@interface TabMenuListener : NSObject <NSMenuDelegate>
@property(nonatomic, readonly, getter=isMenuOpen) BOOL menuOpen;
@property(nonatomic, assign) BOOL rebuildMenu;

- (instancetype)initWithCallback:(MenuItemCallback)callback
                 rebuildCallback:
                     (base::RepeatingCallback<void()>)rebuildCallback;
- (void)activateTab:(id)sender;
@end

@implementation TabMenuListener {
  MenuItemCallback _callback;
  base::RepeatingCallback<void()> _rebuildCallback;
}

@synthesize menuOpen = _menuOpen;
@synthesize rebuildMenu = _rebuildMenu;

- (instancetype)initWithCallback:(MenuItemCallback)callback
                 rebuildCallback:
                     (base::RepeatingCallback<void()>)rebuildCallback {
  if ((self = [super init])) {
    _callback = callback;
    _rebuildCallback = rebuildCallback;
  }
  return self;
}

- (void)menuNeedsUpdate:(NSMenu*)menu {
  if (_rebuildMenu) {
    _rebuildCallback.Run();
    _rebuildMenu = NO;
  }
}

- (IBAction)activateTab:(id)sender {
  _callback.Run(sender);
}

- (void)menuWillOpen:(NSMenu*)menu {
  _menuOpen = YES;
}

- (void)menuDidClose:(NSMenu*)menu {
  _menuOpen = NO;
}
@end

TabMenuBridge::TabMenuBridge(NSMenuItem* menu_item) : menu_item_(menu_item) {
  menu_listener_ = [[TabMenuListener alloc]
      initWithCallback:base::BindRepeating(
                           &TabMenuBridge::OnDynamicItemChosen,
                           // Unretained is safe here: this class owns
                           // MenuListener, which holds the callback
                           // being constructed here, so the callback
                           // will be destructed before this class.
                           base::Unretained(this))
       rebuildCallback:base::BindRepeating(
                           &TabMenuBridge::AddDynamicItemsFromModel,
                           // Unretained is safe here: this class owns
                           // MenuListener, which holds the callback
                           // being constructed here, so the callback
                           // will be destructed before this class.
                           base::Unretained(this))];
  [menu_item_.submenu setDelegate:menu_listener_];
}

TabMenuBridge::~TabMenuBridge() {
  [menu_item_.submenu setDelegate:nil];
  if (model_) {
    model_->RemoveObserver(this);
  }
  RemoveMenuItems(DynamicMenuItems());
}

void TabMenuBridge::SetTabStripModel(TabStripModel* model) {
  if (model_ == model) {
    return;
  }

  if (model_) {
    model_->RemoveObserver(this);
  }

  model_ = model;

  if (model_) {
    model_->AddObserver(this);
    AddDynamicItemsFromModel();
  } else {
    RemoveMenuItems(DynamicMenuItems());
    menu_item_to_tab_.clear();
    tab_to_menu_item_.clear();
  }
}

void TabMenuBridge::SetForceRebuildMenuForTesting(bool force) {
  force_rebuild_menu_ = force;
}

NSMutableArray* TabMenuBridge::DynamicMenuItems() {
  NSMenu* tabMenu = menu_item_.submenu;
  NSMutableArray* array =
      [[NSMutableArray alloc] initWithCapacity:[tabMenu numberOfItems]];

  for (NSMenuItem* item in menu_item_.submenu.itemArray) {
    if (item.target == menu_listener_) {
      [array addObject:item];
    }
  }

  return array;
}

void TabMenuBridge::AddDynamicItemsFromModel() {
  if (!model_) {
    return;
  }

  NSMutableArray* recyclable_items = DynamicMenuItems();
  NSMenu* tabMenu = menu_item_.submenu;

  menu_item_to_tab_.clear();
  tab_to_menu_item_.clear();
  dynamic_items_start_ = tabMenu.numberOfItems - recyclable_items.count;
  for (int i = 0; i < model_->count(); ++i) {
    NSMenuItem* item;

    if (recyclable_items.count) {
      item = [recyclable_items firstObject];
      [recyclable_items removeObjectAtIndex:0];
      item.state = NSControlStateValueOff;
    } else {
      item = [[NSMenuItem alloc] initWithTitle:@""
                                        action:@selector(activateTab:)
                                 keyEquivalent:@""];
      [item setTarget:menu_listener_];
    }

    if (model_->active_index() == i) {
      [item setState:NSControlStateValueOn];
    }

    tabs::TabInterface* tab = model_->GetTabAtIndex(i);
    UpdateItemForWebContents(item, tab->GetContents(), model_);
    menu_item_to_tab_[item] = tab;
    tab_to_menu_item_[tab] = item;

    if ([item menu] == nil) {
      [tabMenu addItem:item];
    }
  }

  RemoveMenuItems(recyclable_items);
}

void TabMenuBridge::OnDynamicItemChosen(NSMenuItem* item) {
  if (!model_) {
    return;
  }

  DCHECK_EQ(item.target, menu_listener_);
  auto it = menu_item_to_tab_.find(item);
  if (it == menu_item_to_tab_.end()) {
    return;
  }

  int index = model_->GetIndexOfTab(it->second);
  if (index == TabStripModel::kNoTab) {
    return;
  }

  model_->ActivateTabAt(index,
                        TabStripUserGestureDetails(
                            TabStripUserGestureDetails::GestureType::kTabMenu));
}

void TabMenuBridge::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  DCHECK(tab_strip_model);
  DCHECK_EQ(tab_strip_model, model_);

  if (!force_rebuild_menu_ && ![menu_listener_ isMenuOpen]) {
    [menu_listener_ setRebuildMenu:YES];
    // When tabs are removed while the menu is not open, erase their entries
    // from the map to release raw_ptr<TabInterface> before the tabs are freed.
    // For other changes (moves, inserts, etc.), keep the existing map so that
    // clicks on stale menu items can still activate the correct tab.
    if (change.type() == TabStripModelChange::kRemoved) {
      for (const auto& removed_tab : change.GetRemove()->contents) {
        auto it = tab_to_menu_item_.find(removed_tab.tab);
        if (it != tab_to_menu_item_.end()) {
          menu_item_to_tab_.erase(it->second);
          tab_to_menu_item_.erase(it);
        }
      }
    }
    return;
  }

  // If a single WebContents is being replaced, just regenerate that one menu
  // item.
  if (change.type() == TabStripModelChange::kReplaced) {
    const TabStripModelChange::Replace* replace = change.GetReplace();
    int menu_index = replace->index + dynamic_items_start_;
    UpdateItemForWebContents([menu_item_.submenu itemAtIndex:menu_index],
                             replace -> new_contents, model_);
    return;
  }

  AddDynamicItemsFromModel();
}

void TabMenuBridge::OnTabChangedAt(tabs::TabInterface* tab,
                                   TabChangeType change_type) {
  DCHECK(model_);

  // Ignore loading state changes - they happen very often during page load and
  // are used to drive the load spinner, which is not interesting to this menu.
  if (change_type == TabChangeType::kLoadingOnly) {
    return;
  }

  if (!force_rebuild_menu_ && ![menu_listener_ isMenuOpen]) {
    [menu_listener_ setRebuildMenu:YES];
    return;
  }

  auto it = tab_to_menu_item_.find(tab);
  if (it == tab_to_menu_item_.end()) {
    // If OnTabChangedAt fires before this observer has observed a newly added
    // tab, it will not be in the map yet. Early-out safely here; the item will
    // be created when we process the addition notification.
    return;
  }

  NSMenuItem* item = it->second;
  UpdateItemForWebContents(item, tab->GetContents(), model_);
}

// If a tab group is changed, update group indicator for each tab.
void TabMenuBridge::OnTabGroupChanged(const TabGroupChange& change) {
  if (!force_rebuild_menu_ && ![menu_listener_ isMenuOpen]) {
    [menu_listener_ setRebuildMenu:YES];
    return;
  }

  AddDynamicItemsFromModel();
}

// If a tab is moved into or outside the group, then update group indicator for
// each tab.
void TabMenuBridge::TabGroupedStateChanged(
    TabStripModel* tab_strip_model,
    std::optional<tab_groups::TabGroupId> old_group,
    std::optional<tab_groups::TabGroupId> new_group,
    tabs::TabInterface* tab,
    int index) {
  DCHECK(tab_strip_model);
  DCHECK_EQ(tab_strip_model, model_);

  if (!force_rebuild_menu_ && ![menu_listener_ isMenuOpen]) {
    [menu_listener_ setRebuildMenu:YES];
    return;
  }

  AddDynamicItemsFromModel();
}

void TabMenuBridge::OnTabStripModelDestroyed(TabStripModel* model) {
  model_->RemoveObserver(this);
  model_ = nullptr;
  menu_item_to_tab_.clear();
  tab_to_menu_item_.clear();
}
