// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/renderer_context_menu/render_view_context_menu_mac_cocoa.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/mac/mac_util.h"
#import "base/mac/scoped_objc_class_swizzler.h"
#import "base/mac/scoped_sending_event.h"
#include "base/macros.h"
#include "base/message_loop/message_loop_current.h"
#import "base/message_loop/message_pump_mac.h"
#include "base/no_destructor.h"
#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/mac/nsprocessinfo_additions.h"
#import "ui/base/cocoa/menu_controller.h"

namespace {

base::mac::ScopedObjCClassSwizzler* g_populatemenu_swizzler = nullptr;

// |g_filtered_entries_array| is only set during testing (see
// +[ChromeSwizzleServicesMenuUpdater storeFilteredEntriesForTestingInArray:]).
// Otherwise it remains nil.
NSMutableArray* g_filtered_entries_array = nil;

// Retrieves an NSMenuItem which has the specified command_id. This function
// traverses the given |model| in the depth-first order. When this function
// finds an item whose command_id is the same as the given |command_id|, it
// returns the NSMenuItem associated with the item. This function emulates
// views::MenuItemViews::GetMenuItemByID() for Mac.
NSMenuItem* GetMenuItemByID(ui::MenuModel* model,
                            NSMenu* menu,
                            int command_id) {
  for (int i = 0; i < model->GetItemCount(); ++i) {
    NSMenuItem* item = [menu itemAtIndex:i];
    if (model->GetCommandIdAt(i) == command_id)
      return item;

    ui::MenuModel* submenu = model->GetSubmenuModelAt(i);
    if (submenu && [item hasSubmenu]) {
      NSMenuItem* subitem =
          GetMenuItemByID(submenu, [item submenu], command_id);
      if (subitem)
        return subitem;
    }
  }
  return nil;
}

}  // namespace

// An AppKit-private class that adds Services items to contextual menus and
// the application Services menu.
@interface _NSServicesMenuUpdater : NSObject
- (void)populateMenu:(NSMenu*)menu
    withServiceEntries:(NSArray*)entries
            forDisplay:(BOOL)display;
@end

// An AppKit-private class representing a Services menu entry.
@interface _NSServiceEntry : NSObject
- (NSString*)bundleIdentifier;
@end

@implementation ChromeSwizzleServicesMenuUpdater

- (void)populateMenu:(NSMenu*)menu
    withServiceEntries:(NSArray*)entries
            forDisplay:(BOOL)display {
  NSMutableArray* remainingEntries = [NSMutableArray array];
  [g_filtered_entries_array removeAllObjects];

  // Remove some services.
  //   - Remove the ones from Safari, as they are redundant to the ones provided
  //     by Chromium, and confusing to the user due to them switching apps
  //     upon their selection.
  //   - Remove the "Open URL" one provided by SystemUIServer, as it is
  //     redundant to the one provided by Chromium and has other serious issues.
  //     (https://crbug.com/960209)

  for (_NSServiceEntry* nextEntry in entries) {
    NSString* bundleIdentifier = [nextEntry bundleIdentifier];
    NSString* message = [nextEntry valueForKey:@"message"];
    bool shouldRemove =
        ([bundleIdentifier isEqualToString:@"com.apple.Safari"]) ||
        ([bundleIdentifier isEqualToString:@"com.apple.systemuiserver"] &&
         [message isEqualToString:@"openURL"]);

    if (!shouldRemove) {
      [remainingEntries addObject:nextEntry];
    } else {
      [g_filtered_entries_array addObject:nextEntry];
    }
  }

  // Pass the filtered array along to the _NSServicesMenuUpdater.
  g_populatemenu_swizzler->InvokeOriginal<void, NSMenu*, NSArray*, BOOL>(
      self, _cmd, menu, remainingEntries, display);
}

+ (void)storeFilteredEntriesForTestingInArray:(NSMutableArray*)array {
  [g_filtered_entries_array release];
  g_filtered_entries_array = [array retain];
}

+ (void)load {
  // Swizzling should not happen in renderer processes.
  if (![[NSProcessInfo processInfo] cr_isMainBrowserOrTestProcess])
    return;

  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    // Confirm that the AppKit's private _NSServiceEntry class exists. This
    // class cannot be accessed at link time and is not guaranteed to exist in
    // past or future AppKits so use NSClassFromString() to locate it. Also
    // check that the class implements the bundleIdentifier method. The browser
    // test checks for all of this as well, but the checks here ensure that we
    // don't crash out in the wild when running on some future version of OS X.
    // Odds are a developer will be running a newer version of OS X sooner than
    // the bots - NOTREACHED() will get them to tell us if compatibility breaks.
    if (![NSClassFromString(@"_NSServiceEntry")
            instancesRespondToSelector:@selector(bundleIdentifier)]) {
      NOTREACHED();
      return;
    }

    // Perform similar checks on the AppKit's private _NSServicesMenuUpdater
    // class.
    SEL targetSelector = @selector(populateMenu:withServiceEntries:forDisplay:);
    Class targetClass = NSClassFromString(@"_NSServicesMenuUpdater");
    if (![targetClass instancesRespondToSelector:targetSelector]) {
      NOTREACHED();
      return;
    }

    // Replace the populateMenu:withServiceEntries:forDisplay: method in
    // _NSServicesMenuUpdater with an implementation that can filter Services
    // menu entries from contextual menus and elsewhere. Place the swizzler into
    // a static so that it never goes out of scope, because the scoper's
    // destructor undoes the swizzling.
    Class swizzleClass = [ChromeSwizzleServicesMenuUpdater class];
    static base::NoDestructor<base::mac::ScopedObjCClassSwizzler>
        servicesMenuFilter(targetClass, swizzleClass, targetSelector);
    g_populatemenu_swizzler = servicesMenuFilter.get();
  });
}

@end

// OSX implemenation of the ToolkitDelegate.
// This simply (re)delegates calls to RVContextMenuMac because they do not
// have to be componentized.
class ToolkitDelegateMacCocoa : public RenderViewContextMenu::ToolkitDelegate {
 public:
  explicit ToolkitDelegateMacCocoa(RenderViewContextMenuMacCocoa* context_menu)
      : context_menu_(context_menu) {}

  ~ToolkitDelegateMacCocoa() override {}

 private:
  // ToolkitDelegate:
  void Init(ui::SimpleMenuModel* menu_model) override {
    context_menu_->InitToolkitMenu();
  }

  void Cancel() override { context_menu_->CancelToolkitMenu(); }

  void UpdateMenuItem(int command_id,
                      bool enabled,
                      bool hidden,
                      const base::string16& title) override {
    context_menu_->UpdateToolkitMenuItem(command_id, enabled, hidden, title);
  }

  RenderViewContextMenuMacCocoa* context_menu_;
  DISALLOW_COPY_AND_ASSIGN(ToolkitDelegateMacCocoa);
};

// Obj-C bridge class that is the target of all items in the context menu.
// Relies on the tag being set to the command id.
RenderViewContextMenuMacCocoa::RenderViewContextMenuMacCocoa(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params,
    NSView* parent_view)
    : RenderViewContextMenuMac(render_frame_host, params),
      parent_view_(parent_view) {
  auto delegate = std::make_unique<ToolkitDelegateMacCocoa>(this);
  set_toolkit_delegate(std::move(delegate));
}

RenderViewContextMenuMacCocoa::~RenderViewContextMenuMacCocoa() {}

void RenderViewContextMenuMacCocoa::Show() {
  menu_controller_.reset([[MenuControllerCocoa alloc] initWithModel:&menu_model_
                                             useWithPopUpButtonCell:NO]);

  gfx::Point params_position(params_.x, params_.y);

  // Synthesize an event for the click, as there is no certainty that
  // [NSApp currentEvent] will return a valid event.
  NSEvent* currentEvent = [NSApp currentEvent];
  NSWindow* window = [parent_view_ window];
  NSPoint position =
      NSMakePoint(params_position.x(),
                  NSHeight([parent_view_ bounds]) - params_position.y());
  position = [parent_view_ convertPoint:position toView:nil];
  NSTimeInterval eventTime = [currentEvent timestamp];
  NSEvent* clickEvent = [NSEvent mouseEventWithType:NSRightMouseDown
                                           location:position
                                      modifierFlags:NSRightMouseDownMask
                                          timestamp:eventTime
                                       windowNumber:[window windowNumber]
                                            context:nil
                                        eventNumber:0
                                         clickCount:1
                                           pressure:1.0];

  {
    // Make sure events can be pumped while the menu is up.
    base::MessageLoopCurrent::ScopedNestableTaskAllower allow;

    // Ensure the UI can update while the menu is fading out.
    base::ScopedPumpMessagesInPrivateModes pump_private;

    // One of the events that could be pumped is |window.close()|.
    // User-initiated event-tracking loops protect against this by
    // setting flags in -[CrApplication sendEvent:], but since
    // web-content menus are initiated by IPC message the setup has to
    // be done manually.
    base::mac::ScopedSendingEvent sendingEventScoper;

    // Show the menu.
    [NSMenu popUpContextMenu:[menu_controller_ menu]
                   withEvent:clickEvent
                     forView:parent_view_];
  }
}

void RenderViewContextMenuMacCocoa::CancelToolkitMenu() {
  [menu_controller_ cancel];
}

void RenderViewContextMenuMacCocoa::UpdateToolkitMenuItem(
    int command_id,
    bool enabled,
    bool hidden,
    const base::string16& title) {
  NSMenuItem* item =
      GetMenuItemByID(&menu_model_, [menu_controller_ menu], command_id);
  if (!item)
    return;

  // Update the returned NSMenuItem directly so we can update it immediately.
  [item setEnabled:enabled];
  [item setTitle:base::SysUTF16ToNSString(title)];
  [item setHidden:hidden];
  [[item menu] itemChanged:item];
}
