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
#import "components/remote_cocoa/app_shim/menu_controller_cocoa_delegate_impl.h"
#include "content/public/browser/web_contents.h"
#import "ui/base/cocoa/menu_controller.h"
#include "ui/base/cocoa/menu_utils.h"
#include "ui/base/interaction/element_tracker_mac.h"
#include "ui/color/color_provider.h"
#include "ui/views/controls/menu/menu_controller_cocoa_delegate_params.h"
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

// Obj-C bridge class that is the target of all items in the context menu.
// Relies on the tag being set to the command id.
RenderViewContextMenuMacCocoa::RenderViewContextMenuMacCocoa(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params,
    NSView* parent_view)
    : RenderViewContextMenuMac(render_frame_host, params),
      parent_view_(parent_view) {
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

  menu_controller_delegate_ = [[MenuControllerCocoaDelegateImpl alloc]
      initWithParams:MenuControllerParamsForWidget(widget)];
  menu_controller_ =
      [[MenuControllerCocoa alloc] initWithModel:&menu_model_
                                        delegate:menu_controller_delegate_
                          useWithPopUpButtonCell:NO];

  NSPoint position =
      NSMakePoint(params_.x, NSHeight(parent_view_.bounds) - params_.y);
  position = [parent_view_ convertPoint:position toView:nil];
  NSEvent* clickEvent = ui::EventForPositioningContextMenuRelativeToWindow(
      position, [parent_view_ window]);

  ui::ShowContextMenu(menu_controller_.menu, clickEvent, parent_view_,
                      /*allow_nested_tasks=*/true,
                      views::ElementTrackerViews::GetContextForWidget(widget));
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
  item.enabled = enabled;
  item.title = base::SysUTF16ToNSString(title);
  item.hidden = hidden;
}
