// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/status_icons/status_icon_mac.h"

#import <AppKit/AppKit.h>
#include <Foundation/Foundation.h>
#include <objc/message.h>
#include <objc/runtime.h>

#include "base/check.h"
#include "base/mac/mac_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#import "ui/menus/cocoa/menu_controller.h"
#include "ui/message_center/public/cpp/notifier_id.h"

static const char kStatusItemControllerKey = 0;

// This class bridges between the Objective-C API for status items and the C++
// classes integrating with Chromium. Owned by the NSStatusBarButton as an
// associated object with the key &kStatusItemControllerKey.
@interface StatusItemController : NSObject
// Designated initializer.
- (instancetype)initWithStatusIconMac:(StatusIconMac*)icon
                   forStatusBarButton:(NSStatusBarButton*)button;

// Returns the StatusItemController that is responsible for the specified
// `button`.
+ (instancetype)controllerForButton:(NSStatusBarButton*)button;

// Clears all internal state.
- (void)reset;

// Handles left mouse clicks. Takes a single parameter so that it can be the
// "action" of a "target/action" pair.
- (void)handleClick:(id)sender;
@end

@implementation StatusItemController {
  raw_ptr<StatusIconMac> _statusIcon;  // weak
}

+ (void)initialize {
  // Ensure any future possible subclasses don't cause this to be called more
  // than once.
  if (self != [StatusItemController class]) {
    return;
  }

  // NSStatusBarButton implements -rightMouseDown: by spinning its own event
  // loop. What that practically means, though, is that the status bar item
  // stays highlighted even after the mouse-up, and a subsequent
  // right-mouse-down unhighlights the button but doesn't trigger an action.
  // This is an issue in Cocoa known by the community; see posts such as:
  //
  // https://www.jessesquires.com/blog/2019/08/16/workaround-highlight-bug-nsstatusitem/
  // https://stackoverflow.com/questions/59635971/show-nsmenu-only-on-nsstatusbarbutton-right-click
  //
  // The standard workaround in these posts is to trigger the desired UI action
  // with the right-mouse-up rather than the right-mouse-down. While that's the
  // simplest workaround, that's not desirable.
  //
  // One might consider subclassing NSStatusBarButton to override
  // -rightMouseDown: and then use that subclass instead. Unfortunately, that
  // proposal is thwarted because the `button` property of NSStatusItem is
  // read-only.
  //
  // Therefore, swizzle the -[NSStatusBarButton rightMouseDown:] method to gain
  // control and achieve the desired behavior.
  IMP imp = imp_implementationWithBlock(^(id object_self, NSEvent* event) {
    StatusItemController* controller =
        [StatusItemController controllerForButton:object_self];
    [controller handleClick:nil isRightButton:YES];
  });

  class_replaceMethod([NSStatusBarButton class], @selector(rightMouseDown:),
                      imp, nullptr);
}

- (instancetype)initWithStatusIconMac:(StatusIconMac*)icon
                   forStatusBarButton:(NSStatusBarButton*)button {
  objc_setAssociatedObject(button, &kStatusItemControllerKey, self,
                           OBJC_ASSOCIATION_RETAIN);

  _statusIcon = icon;
  return self;
}

+ (instancetype)controllerForButton:(NSStatusBarButton*)button {
  return objc_getAssociatedObject(button, &kStatusItemControllerKey);
}

- (void)reset {
  _statusIcon = nullptr;
}

- (void)handleClick:(id)sender {
  [self handleClick:sender isRightButton:NO];
}

- (void)handleClick:(id)sender isRightButton:(BOOL)isRightButton {
  CHECK(_statusIcon);

  if (!_statusIcon->HasStatusIconMenu()) {
    // If there is no menu at all, just dispatch the click.
    _statusIcon->DispatchClickEvent();
  } else if (_statusIcon->open_menu_with_secondary_click()) {
    // Otherwise, there is a menu. For "open menu with a secondary click", plain
    // left-clicks are dispatched normally, and secondary clicks open the menu.
    NSEvent* event = NSApp.currentEvent;
    BOOL secondary_click =
        isRightButton || (event.type == NSEventTypeLeftMouseDown &&
                          event.modifierFlags & NSEventModifierFlagControl);
    if (secondary_click) {
      _statusIcon->CreateAndOpenMenu();
    } else if (event.type == NSEventTypeLeftMouseDown) {
      _statusIcon->DispatchClickEvent();
    }
  }
  // There is no "else" here. If there is a menu, but it's not opened with a
  // secondary click, it is set as the menu property on the item and code flow
  // never gets here.
}

@end

StatusIconMac::StatusIconMac() = default;

StatusIconMac::~StatusIconMac() {
  // If there is a status item, remove it from the status bar.
  if (item_) {
    // The controller has a raw_ptr to this object, so clear the pointer.
    // (Because it's not guaranteed that the association is the sole/last
    // reference to the controller, clearing the association is not sufficient.)
    StatusItemController* controller =
        [StatusItemController controllerForButton:item_.button];
    [controller reset];
    [NSStatusBar.systemStatusBar removeStatusItem:item_];
  }
}

NSStatusItem* StatusIconMac::item() {
  if (!item_) {
    item_ = [NSStatusBar.systemStatusBar
        statusItemWithLength:NSSquareStatusItemLength];
    NSStatusBarButton* item_button = item_.button;
    item_button.enabled = YES;
    NSButtonCell* item_button_cell = item_button.cell;
    item_button_cell.highlightsBy =
        NSContentsCellMask | NSChangeBackgroundCellMask;

    StatusItemController* controller =
        [[StatusItemController alloc] initWithStatusIconMac:this
                                         forStatusBarButton:item_button];
    item_button.target = controller;
    item_button.action = @selector(handleClick:);
  }
  return item_;
}

void StatusIconMac::SetImage(const gfx::ImageSkia& image) {
  if (!image.isNull()) {
    NSImage* ns_image = gfx::Image(image).AsNSImage();
    if (ns_image) {
      item().button.image = ns_image;
    }
  }
}

void StatusIconMac::SetToolTip(const std::u16string& tool_tip) {
  item().button.toolTip = base::SysUTF16ToNSString(tool_tip);
}

void StatusIconMac::DisplayBalloon(
    const gfx::ImageSkia& icon,
    const std::u16string& title,
    const std::u16string& contents,
    const message_center::NotifierId& notifier_id) {
  notification_.DisplayBalloon(ui::ImageModel::FromImageSkia(icon), title,
                               contents, notifier_id);
}

void StatusIconMac::SetOpenMenuWithSecondaryClick(
    bool open_menu_with_secondary_click) {
  open_menu_with_secondary_click_ = open_menu_with_secondary_click;
  [item().button
      sendActionOn:(NSEventMaskLeftMouseDown | NSEventMaskRightMouseDown)];
}

void StatusIconMac::SetImageTemplate(bool is_template) {
  [item().button.image setTemplate:is_template];
}

void StatusIconMac::OnMenuStateChanged() {
  // Recreate menu to reflect changes to the menu model.
  CreateMenu(menu_model_);
}

bool StatusIconMac::HasStatusIconMenu() {
  return open_menu_with_secondary_click_ ? menu_model_ : menu_ != nil;
}

void StatusIconMac::CreateAndOpenMenu() {
  CreateMenu(menu_model_);
  [item().button performClick:nil];
  item().menu = nil;
}

void StatusIconMac::UpdatePlatformContextMenu(StatusIconMenuModel* model) {
  if (!model) {
    menu_ = nil;
  } else if (open_menu_with_secondary_click_) {
    menu_model_ = model;
  } else {
    if (model != menu_model_) {
      observation_.Reset();
      observation_.Observe(model);
      menu_model_ = model;
    }
    CreateMenu(model);
  }
}

void StatusIconMac::CreateMenu(ui::MenuModel* model) {
  DCHECK(model);
  menu_ = [[MenuControllerCocoa alloc] initWithModel:model delegate:nil];
  item().menu = menu_.menu;
}
