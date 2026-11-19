// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/status_icons/status_icon_mac.h"

#import <AppKit/AppKit.h>
#include <Foundation/Foundation.h>
#include <objc/message.h>
#include <objc/runtime.h>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/mac/mac_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/browser/ui/cocoa/status_icons/status_icons_features.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#import "ui/menus/cocoa/menu_controller.h"
#include "ui/message_center/public/cpp/notifier_id.h"

static const char kStatusItemControllerKey = 0;
static void (*g_orig_mouse_down)(id, SEL, NSEvent*) = nullptr;
static void (*g_orig_right_mouse_down)(id, SEL, NSEvent*) = nullptr;

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

// Observes `NSMenuDidEndTrackingNotification` on `menu` so deferred rebuilds
// flush as soon as menu tracking finishes.
- (void)setObservedMenu:(NSMenu*)menu;

// Handles primary mouse-down events before AppKit opens `item.menu` when
// `open_menu_with_secondary_click()` is enabled.
- (void)handleMouseDown:(NSEvent*)event;

// Handles right mouse-down events.
- (void)handleRightMouseDown:(NSEvent*)event;

// Target/action handler for menu-less status items (including AXPress /
// performClick:).
- (void)handleClick:(id)sender;
@end

@implementation StatusItemController {
  raw_ptr<StatusIconMac> _statusIcon;  // weak
  NSStatusBarButton* __weak _button;
}

+ (void)initialize {
  // Ensure any future possible subclasses don't cause this to be called more
  // than once.
  if (self != [StatusItemController class]) {
    return;
  }

  // Keep `item.menu` populated even when `open_menu_with_secondary_click()` is
  // true so macOS `Ctrl+F8` ("Move focus to status menus", which checks
  // `-[NSStatusItem _participatesInNavigationLoop]` -> `item.menu != nil`) and
  // VoiceOver `AXPress` (`Ctrl+Option+Space`) work natively like other status
  // bar extras. Because AppKit opens `item.menu` inside `-[NSControl
  // mouseDown:]` whenever `item.menu` is non-nil, swizzle `mouseDown:` on
  // `NSStatusBarButton` to dispatch plain left-clicks directly when
  // `open_menu_with_secondary_click()` is enabled while forwarding
  // Control-clicks (and standard menu items) to the original `mouseDown:`.
  Method mouse_down_method =
      class_getInstanceMethod([NSStatusBarButton class], @selector(mouseDown:));
  g_orig_mouse_down = reinterpret_cast<void (*)(id, SEL, NSEvent*)>(
      method_getImplementation(mouse_down_method));
  IMP mouse_down_imp =
      imp_implementationWithBlock(^(id object_self, NSEvent* event) {
        StatusItemController* controller =
            [StatusItemController controllerForButton:object_self];
        if (!controller) {
          g_orig_mouse_down(object_self, @selector(mouseDown:), event);
          return;
        }
        [controller handleMouseDown:event];
      });
  class_replaceMethod([NSStatusBarButton class], @selector(mouseDown:),
                      mouse_down_imp,
                      method_getTypeEncoding(mouse_down_method));

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
  Method right_mouse_down_method = class_getInstanceMethod(
      [NSStatusBarButton class], @selector(rightMouseDown:));
  g_orig_right_mouse_down = reinterpret_cast<void (*)(id, SEL, NSEvent*)>(
      method_getImplementation(right_mouse_down_method));
  IMP right_mouse_down_imp = imp_implementationWithBlock(^(id object_self,
                                                           NSEvent* event) {
    StatusItemController* controller =
        [StatusItemController controllerForButton:object_self];
    if (!controller) {
      g_orig_right_mouse_down(object_self, @selector(rightMouseDown:), event);
      return;
    }
    [controller handleRightMouseDown:event];
  });
  class_replaceMethod([NSStatusBarButton class], @selector(rightMouseDown:),
                      right_mouse_down_imp,
                      method_getTypeEncoding(right_mouse_down_method));
}

- (instancetype)initWithStatusIconMac:(StatusIconMac*)icon
                   forStatusBarButton:(NSStatusBarButton*)button {
  if (!(self = [super init])) {
    return nil;
  }
  objc_setAssociatedObject(button, &kStatusItemControllerKey, self,
                           OBJC_ASSOCIATION_RETAIN);

  _statusIcon = icon;
  _button = button;

  // Hide the status bar icon when transitioning into fullscreen and show it
  // after leaving fullscreen. This is a workaround for a macOS 26.x-only bug
  // where entering fullscreen with a visible status bar icon can cause an
  // _NSFullScreenTransitionOverlayWindow to remain visible after exiting from
  // fullscreen (crbug.com/494614152).
  //
  // Displaying the icon any earlier (e.g., on NSWindowWillExitFullScreen) may
  // also trigger this bug, so the icon isn't shown until fullscreen is exited.
  if (base::FeatureList::IsEnabled(features::kHideStatusIconMacInFullscreen)) {
    if (@available(macOS 26, *)) {
      if (!@available(macOS 27, *)) {
        [NSNotificationCenter.defaultCenter
            addObserver:self
               selector:@selector(windowWillEnterFullScreen:)
                   name:NSWindowWillEnterFullScreenNotification
                 object:nil];
        [NSNotificationCenter.defaultCenter
            addObserver:self
               selector:@selector(windowDidExitFullScreen:)
                   name:NSWindowDidExitFullScreenNotification
                 object:nil];
      }
    }
  }
  return self;
}

+ (instancetype)controllerForButton:(NSStatusBarButton*)button {
  return objc_getAssociatedObject(button, &kStatusItemControllerKey);
}

- (void)reset {
  [NSNotificationCenter.defaultCenter removeObserver:self];
  _statusIcon = nullptr;
  _button = nil;
}

- (void)dealloc {
  [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)setObservedMenu:(NSMenu*)menu {
  [NSNotificationCenter.defaultCenter
      removeObserver:self
                name:NSMenuDidEndTrackingNotification
              object:nil];
  if (menu) {
    [NSNotificationCenter.defaultCenter
        addObserver:self
           selector:@selector(menuDidEndTracking:)
               name:NSMenuDidEndTrackingNotification
             object:menu];
  }
}

- (void)menuDidEndTracking:(NSNotification*)notification {
  if (_statusIcon) {
    _statusIcon->OnMenuDidEndTracking();
  }
}

- (void)handleMouseDown:(NSEvent*)event {
  if (!_statusIcon) {
    return;
  }

  NSStatusBarButton* button = _button;
  if (!button) {
    return;
  }

  if (_statusIcon->HasStatusIconMenu() &&
      _statusIcon->open_menu_with_secondary_click() &&
      !(event.modifierFlags & NSEventModifierFlagControl)) {
    _statusIcon->DispatchClickEvent();
    return;
  }

  if (_statusIcon->HasStatusIconMenu() &&
      _statusIcon->MaybeRunOpenMenuForTesting()) {
    return;
  }

  g_orig_mouse_down(button, @selector(mouseDown:), event);
}

- (void)handleRightMouseDown:(NSEvent*)event {
  if (!_statusIcon) {
    return;
  }

  if (_statusIcon->HasStatusIconMenu()) {
    if (_statusIcon->MaybeRunOpenMenuForTesting()) {
      return;
    }
    [_button performClick:nil];
  }
}

- (void)handleClick:(id)sender {
  CHECK(_statusIcon);

  if (!_statusIcon->HasStatusIconMenu()) {
    // If there is no menu at all, dispatch the click (covers both mouse clicks
    // and VoiceOver AXPress / performClick:).
    _statusIcon->DispatchClickEvent();
  }
}

- (void)windowWillEnterFullScreen:(NSNotification*)notification {
  if (_statusIcon) {
    _statusIcon->SetVisible(false);
  }
}

- (void)windowDidExitFullScreen:(NSNotification*)notification {
  if (_statusIcon) {
    _statusIcon->SetVisible(true);
  }
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
  NSString* ns_tool_tip = base::SysUTF16ToNSString(tool_tip);
  item().button.toolTip = ns_tool_tip;
  item().button.accessibilityLabel = ns_tool_tip;
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
}

void StatusIconMac::SetImageTemplate(bool is_template) {
  [item().button.image setTemplate:is_template];
}

void StatusIconMac::SetVisible(bool visible) {
  if (item_) {
    item_.visible = visible;
  }
}

void StatusIconMac::OnMenuStateChanged() {
  if ([menu_ isMenuOpen]) {
    menu_needs_rebuild_ = true;
    return;
  }
  // Recreate menu to reflect changes to the menu model.
  if (StatusIconMenuModel* model = observation_.GetSource()) {
    CreateMenu(model);
  }
}

void StatusIconMac::OnMenuDidEndTracking() {
  if (!menu_needs_rebuild_) {
    return;
  }
  // AppKit posts `NSMenuDidEndTrackingNotification` before dispatching the
  // selected `NSMenuItem`'s action (`-[MenuControllerCocoa itemSelected:]`),
  // and `NSMenuItem.target` is weak. Defer replacing `menu_` to the next
  // run-loop turn so the closing `MenuControllerCocoa` stays alive to deliver
  // `ActivatedAt()`.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&StatusIconMac::FlushDeferredMenuRebuild,
                                weak_factory_.GetWeakPtr()));
}

void StatusIconMac::FlushDeferredMenuRebuild() {
  if (!menu_needs_rebuild_ || [menu_ isMenuOpen]) {
    return;
  }
  if (StatusIconMenuModel* model = observation_.GetSource()) {
    CreateMenu(model);
  }
}

bool StatusIconMac::MaybeRunOpenMenuForTesting() {
  if (!open_menu_for_testing_) {
    return false;
  }
  open_menu_for_testing_.Run();
  return true;
}

void StatusIconMac::SetOpenMenuForTesting(base::RepeatingClosure callback) {
  open_menu_for_testing_ = std::move(callback);
}

bool StatusIconMac::HasStatusIconMenu() {
  return menu_ != nil;
}

void StatusIconMac::UpdatePlatformContextMenu(StatusIconMenuModel* model) {
  observation_.Reset();
  menu_needs_rebuild_ = false;
  weak_factory_.InvalidateWeakPtrs();
  if (!model) {
    if (item_) {
      [[StatusItemController controllerForButton:item_.button]
          setObservedMenu:nil];
      item_.menu = nil;
    }
    menu_ = nil;
    return;
  }

  observation_.Observe(model);
  CreateMenu(model);
}

void StatusIconMac::CreateMenu(ui::MenuModel* model) {
  CHECK(model, base::NotFatalUntil::M162);
  menu_needs_rebuild_ = false;
  weak_factory_.InvalidateWeakPtrs();
  menu_ = [[MenuControllerCocoa alloc] initWithModel:model
                                       isContextMenu:NO
                                            delegate:nil];
  NSStatusItem* status_item = item();
  status_item.menu = menu_.menu;
  [[StatusItemController controllerForButton:status_item.button]
      setObservedMenu:menu_.menu];
}
