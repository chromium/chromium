// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/tab_menu_bridge.h"

#import <Cocoa/Cocoa.h>

#include "base/functional/callback.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/gfx/image/image_skia_util_mac.h"

using MenuItemCallback = base::RepeatingCallback<void(NSMenuItem*)>;

namespace {

void UpdateItemForWebContents(NSMenuItem* item,
                              content::WebContents* web_contents) {
  TabUIHelper* tab_ui_helper = TabUIHelper::FromWebContents(web_contents);

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

    item.title =
        l10n_util::GetNSStringF(title_id, tab_ui_helper->GetTitle(), emoji);
  } else {
    item.title = base::SysUTF16ToNSString(tab_ui_helper->GetTitle());
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

@interface TabMenuListener : NSObject
- (instancetype)initWithCallback:(MenuItemCallback)callback;
- (void)activateTab:(id)sender;
@end

@implementation TabMenuListener {
  MenuItemCallback _callback;
}

- (instancetype)initWithCallback:(MenuItemCallback)callback {
  if ((self = [super init])) {
    _callback = callback;
  }
  return self;
}

- (IBAction)activateTab:(id)sender {
  _callback.Run(sender);
}
@end

TabMenuBridge::TabMenuBridge(TabStripModel* model, NSMenuItem* menu_item)
    : model_(model), menu_item_(menu_item) {
  menu_listener_ = [[TabMenuListener alloc]
      initWithCallback:base::BindRepeating(
                           &TabMenuBridge::OnDynamicItemChosen,
                           // Unretained is safe here: this class owns
                           // MenuListener, which holds the callback
                           // being constructed here, so the callback
                           // will be destructed before this class.
                           base::Unretained(this))];
  model_->AddObserver(this);
}

TabMenuBridge::~TabMenuBridge() {
  if (model_)
    model_->RemoveObserver(this);
  RemoveMenuItems(DynamicMenuItems());
}

void TabMenuBridge::BuildMenu() {
  DCHECK(model_);
  AddDynamicItemsFromModel();
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
  NSMutableArray* recyclable_items = DynamicMenuItems();
  NSMenu* tabMenu = menu_item_.submenu;

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
    UpdateItemForWebContents(item, model_->GetWebContentsAt(i));

    if ([item menu] == nil) {
      [tabMenu addItem:item];
    }
  }

  RemoveMenuItems(recyclable_items);
}

void TabMenuBridge::OnDynamicItemChosen(NSMenuItem* item) {
  if (!model_)
    return;

  DCHECK_EQ(item.target, menu_listener_);
  int index = [menu_item_.submenu indexOfItem:item] - dynamic_items_start_;
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

  // If a single WebContents is being replaced, just regenerate that one menu
  // item.
  if (change.type() == TabStripModelChange::kReplaced) {
    const TabStripModelChange::Replace* replace = change.GetReplace();
    int menu_index = replace->index + dynamic_items_start_;
    UpdateItemForWebContents([menu_item_.submenu itemAtIndex:menu_index],
                             replace->new_contents);
    return;
  }

  AddDynamicItemsFromModel();
}

void TabMenuBridge::TabChangedAt(content::WebContents* contents,
                                 int index,
                                 TabChangeType change_type) {
  DCHECK(model_);

  // Ignore loading state changes - they happen very often during page load and
  // are used to drive the load spinner, which is not interesting to this menu.
  if (change_type == TabChangeType::kLoadingOnly)
    return;

  int menu_index = index + dynamic_items_start_;

  // It might seem like this can't happen but actually it can:
  // 1) Someone calls TabMenuModel::AddWebContents
  // 2) Some other observer (not this) is notified of the add
  // 3) That observer responds by doing something that eventually leads into
  //    UpdateWebContentsStateAt, while this class still hasn't observed the
  //    OnTabStripModelChanged (but the method that will notify us is on the
  //    stack)
  // 4) That UpdateWebContentsStateAt causes this object to observe a
  //    TabChangedAt for an index it hasn't yet been informed exists
  // As such, this code early-outs instead of DCHECKing. The newly-added
  // WebContents will be picked up later anyway when this object does get
  // notified of the addition.
  if (menu_index < 0 || menu_index >= menu_item_.submenu.numberOfItems)
    return;

  NSMenuItem* item = [menu_item_.submenu itemAtIndex:menu_index];
  UpdateItemForWebContents(item, contents);
}

void TabMenuBridge::OnTabStripModelDestroyed(TabStripModel* model) {
  model_->RemoveObserver(this);
  model_ = nullptr;
}
