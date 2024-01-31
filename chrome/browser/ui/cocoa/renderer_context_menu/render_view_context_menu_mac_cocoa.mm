// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/renderer_context_menu/render_view_context_menu_mac_cocoa.h"
#include "base/memory/raw_ptr.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/mac/mac_util.h"
#import "base/mac/scoped_sending_event.h"
#import "base/message_loop/message_pump_apple.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/current_thread.h"
#include "chrome/browser/headless/headless_mode_util.h"
#include "content/public/browser/web_contents.h"
#import "ui/base/cocoa/menu_controller.h"
#include "ui/base/interaction/element_tracker_mac.h"
#include "ui/color/color_provider.h"
#include "ui/views/controls/menu/menu_controller_cocoa_delegate_impl.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/widget/widget.h"

namespace {

// Retrieves an NSMenuItem which has the specified command_id. This function
// traverses the given |model| in the depth-first order. When this function
// finds an item whose command_id is the same as the given |command_id|, it
// returns the NSMenuItem associated with the item. This function emulates
// views::MenuItemViews::GetMenuItemByID() for Mac.
NSMenuItem* GetMenuItemByID(ui::MenuModel* model,
                            NSMenu* menu,
                            int command_id) {
  for (size_t i = 0; i < model->GetItemCount(); ++i) {
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

// macOS implementation of the ToolkitDelegate.
// This simply (re)delegates calls to RVContextMenuMac because they do not
// have to be componentized.
class ToolkitDelegateMacCocoa : public RenderViewContextMenu::ToolkitDelegate {
 public:
  explicit ToolkitDelegateMacCocoa(RenderViewContextMenuMacCocoa* context_menu)
      : context_menu_(context_menu) {}

  ToolkitDelegateMacCocoa(const ToolkitDelegateMacCocoa&) = delete;
  ToolkitDelegateMacCocoa& operator=(const ToolkitDelegateMacCocoa&) = delete;

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
                      const std::u16string& title) override {
    context_menu_->UpdateToolkitMenuItem(command_id, enabled, hidden, title);
  }

  raw_ptr<RenderViewContextMenuMacCocoa> context_menu_;
};

// Obj-C bridge class that is the target of all items in the context menu.
// Relies on the tag being set to the command id.
RenderViewContextMenuMacCocoa::RenderViewContextMenuMacCocoa(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params,
    NSView* parent_view)
    : RenderViewContextMenuMac(render_frame_host, params),
      parent_view_(parent_view) {
  auto delegate = std::make_unique<ToolkitDelegateMacCocoa>(this);
  set_toolkit_delegate(std::move(delegate));
}

RenderViewContextMenuMacCocoa::~RenderViewContextMenuMacCocoa() {
  if (menu_controller_)
    [menu_controller_ cancel];
}

void RenderViewContextMenuMacCocoa::Show() {
  views::Widget* widget = views::Widget::GetTopLevelWidgetForNativeView(
      source_web_contents_->GetNativeView());

  if (!widget || headless::IsHeadlessMode()) {
    return;
  }

  const ui::ColorProvider* color_provider = widget->GetColorProvider();

  menu_controller_delegate_ = [[MenuControllerCocoaDelegateImpl alloc] init];
  menu_controller_ =
      [[MenuControllerCocoa alloc] initWithModel:&menu_model_
                                        delegate:menu_controller_delegate_
                                   colorProvider:color_provider
                          useWithPopUpButtonCell:NO];

  // Synthesize an event for the click, as there is no certainty that
  // [NSApp currentEvent] will return a valid event.
  NSEvent* currentEvent = [NSApp currentEvent];
  NSWindow* window = [parent_view_ window];
  NSPoint position =
      NSMakePoint(params_.x, NSHeight(parent_view_.bounds) - params_.y);
  position = [parent_view_ convertPoint:position toView:nil];
  NSTimeInterval eventTime = [currentEvent timestamp];
  NSEvent* clickEvent = [NSEvent mouseEventWithType:NSEventTypeRightMouseDown
                                           location:position
                                      modifierFlags:0
                                          timestamp:eventTime
                                       windowNumber:[window windowNumber]
                                            context:nil
                                        eventNumber:0
                                         clickCount:1
                                           pressure:1.0];

  {
    // Make sure events can be pumped while the menu is up.
    base::CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop allow;

    // Ensure the UI can update while the menu is fading out.
    base::ScopedPumpMessagesInPrivateModes pump_private;

    // One of the events that could be pumped is |window.close()|.
    // User-initiated event-tracking loops protect against this by
    // setting flags in -[CrApplication sendEvent:], but since
    // web-content menus are initiated by IPC message the setup has to
    // be done manually.
    base::mac::ScopedSendingEvent sendingEventScoper;

    NSMenu* const menu = [menu_controller_ menu];
    ui::ElementTrackerMac::GetInstance()->NotifyMenuWillShow(
        menu, views::ElementTrackerViews::GetContextForWidget(widget));

    // Show the menu.
    [NSMenu popUpContextMenu:menu withEvent:clickEvent forView:parent_view_];
    ui::ElementTrackerMac::GetInstance()->NotifyMenuDoneShowing(menu);
  }
}

void RenderViewContextMenuMacCocoa::CancelToolkitMenu() {
  [menu_controller_ cancel];
}

void RenderViewContextMenuMacCocoa::UpdateToolkitMenuItem(
    int command_id,
    bool enabled,
    bool hidden,
    const std::u16string& title) {
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
