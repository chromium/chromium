// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/status_icons/status_icon_mac.h"

#import <AppKit/AppKit.h>

#include "base/check.h"
#include "base/mac/mac_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/skia/include/core/SkBitmap.h"
#import "ui/base/cocoa/menu_controller.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/message_center/public/cpp/notifier_id.h"

@interface StatusItemController : NSObject {
  raw_ptr<StatusIconMac> _statusIcon;  // weak
}
- (instancetype)initWithIcon:(StatusIconMac*)icon;
- (void)handleClick:(id)sender;

@end // @interface StatusItemController

@implementation StatusItemController

- (instancetype)initWithIcon:(StatusIconMac*)icon {
  _statusIcon = icon;
  return self;
}

- (void)handleClick:(id)sender {
  // Pass along the click notification to our owner.
  DCHECK(_statusIcon);
  // Bring up the status icon menu if there is one, relay the click event
  // otherwise.
  if (!_statusIcon->HasStatusIconMenu())
    _statusIcon->DispatchClickEvent();
}

@end

StatusIconMac::StatusIconMac() {
  controller_ = [[StatusItemController alloc] initWithIcon:this];
}

StatusIconMac::~StatusIconMac() {
  // Remove the status item from the status bar.
  if (item_) {
    [NSStatusBar.systemStatusBar removeStatusItem:item_];
  }
}

NSStatusItem* StatusIconMac::item() {
  if (!item_) {
    // Create a new status item.
    item_ = [NSStatusBar.systemStatusBar
        statusItemWithLength:NSSquareStatusItemLength];
    NSButton* item_button = item_.button;
    item_button.enabled = YES;
    item_button.target = controller_;
    item_button.action = @selector(handleClick:);
    NSButtonCell* item_button_cell = item_button.cell;
    item_button_cell.highlightsBy =
        NSContentsCellMask | NSChangeBackgroundCellMask;
  }
  return item_;
}

void StatusIconMac::SetImage(const gfx::ImageSkia& image) {
  if (!image.isNull()) {
    NSImage* ns_image = skia::SkBitmapToNSImage(*image.bitmap());
    if (ns_image) {
      item().button.image = ns_image;
    }
  }
}

void StatusIconMac::SetToolTip(const std::u16string& tool_tip) {
  // If we have a status icon menu, make the tool tip part of the menu instead
  // of a pop-up tool tip when hovering the mouse over the image.
  tool_tip_ = base::SysUTF16ToNSString(tool_tip);
  if (menu_) {
    SetToolTip(nil);
    CreateMenu([menu_ model], tool_tip_);
  } else {
    SetToolTip(tool_tip_);
  }
}

void StatusIconMac::DisplayBalloon(
    const gfx::ImageSkia& icon,
    const std::u16string& title,
    const std::u16string& contents,
    const message_center::NotifierId& notifier_id) {
  notification_.DisplayBalloon(ui::ImageModel::FromImageSkia(icon), title,
                               contents, notifier_id);
}

bool StatusIconMac::HasStatusIconMenu() {
  return menu_ != nil;
}

void StatusIconMac::UpdatePlatformContextMenu(StatusIconMenuModel* model) {
  if (!model) {
    menu_ = nil;
  } else {
    SetToolTip(nil);
    CreateMenu(model, tool_tip_);
  }
}

void StatusIconMac::CreateMenu(ui::MenuModel* model, NSString* tool_tip) {
  DCHECK(model);

  if (!tool_tip) {
    menu_ = [[MenuControllerCocoa alloc] initWithModel:model
                                              delegate:nil
                                useWithPopUpButtonCell:NO];
  } else {
    // When using a popup button cell menu controller, an extra blank item is
    // added at index 0. Use this item for the tooltip.
    menu_ = [[MenuControllerCocoa alloc] initWithModel:model
                                              delegate:nil
                                useWithPopUpButtonCell:YES];
    NSMenuItem* tool_tip_item = [[menu_ menu] itemAtIndex:0];
    [tool_tip_item setTitle:tool_tip];
  }
  item().menu = [menu_ menu];
}

void StatusIconMac::SetToolTip(NSString* tool_tip) {
  item().button.toolTip = tool_tip;
}
