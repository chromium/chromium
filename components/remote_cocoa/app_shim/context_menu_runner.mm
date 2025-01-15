// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/app_shim/context_menu_runner.h"

#include "base/strings/sys_string_conversions.h"
#include "components/remote_cocoa/app_shim/mojo_menu_model.h"
#include "ui/base/cocoa/menu_utils.h"

namespace remote_cocoa {

namespace {

// Retrieves an NSMenuItem which has the specified command_id. This function
// traverses the given `model` in the depth-first order. When this function
// finds an item whose command_id is the same as the given `command_id`, it
// returns the NSMenuItem associated with the item. This function emulates
// views::MenuItemViews::GetMenuItemByID() for Mac.
NSMenuItem* GetMenuItemById(ui::MenuModel* model,
                            NSMenu* menu,
                            int command_id) {
  for (size_t i = 0; i < model->GetItemCount(); ++i) {
    NSMenuItem* item = [menu itemAtIndex:i];
    if (model->GetCommandIdAt(i) == command_id) {
      return item;
    }

    ui::MenuModel* submenu = model->GetSubmenuModelAt(i);
    if (submenu && [item hasSubmenu]) {
      NSMenuItem* subitem =
          GetMenuItemById(submenu, [item submenu], command_id);
      if (subitem) {
        return subitem;
      }
    }
  }
  return nil;
}

}  // namespace

ContextMenuRunner::ContextMenuRunner(
    mojo::PendingRemote<mojom::MenuHost> host,
    mojo::PendingReceiver<mojom::Menu> receiver)
    : receiver_(this, std::move(receiver)), menu_host_(std::move(host)) {}

ContextMenuRunner::~ContextMenuRunner() {
  if (menu_controller_) {
    CHECK(!menu_controller_.isMenuOpen);
  }
}

void ContextMenuRunner::ShowMenu(mojom::ContextMenuPtr menu,
                                 NSWindow* window,
                                 NSView* target_view) {
  menu_model_ =
      std::make_unique<MojoMenuModel>(std::move(menu->items), menu_host_.get());
  menu_delegate_ = [[MenuControllerCocoaDelegateImpl alloc]
      initWithParams:std::move(menu->params)];
  menu_controller_ =
      [[MenuControllerCocoa alloc] initWithModel:menu_model_.get()
                                        delegate:menu_delegate_
                          useWithPopUpButtonCell:NO];

  if (!target_view) {
    target_view = window.contentView;
  }

  NSEvent* clickEvent =
      ui::EventForPositioningContextMenu(menu->anchor, window);

  ui::ShowContextMenu(menu_controller_.menu, clickEvent, target_view,
                      /*allow_nested_tasks=*/true);

  menu_host_->MenuClosed();
}

void ContextMenuRunner::Cancel() {
  if (menu_controller_) {
    [menu_controller_ cancel];
  }
}

void ContextMenuRunner::UpdateMenuItem(int32_t command_id,
                                       bool enabled,
                                       bool visible,
                                       const std::u16string& label) {
  NSMenuItem* item =
      GetMenuItemById(menu_model_.get(), menu_controller_.menu, command_id);
  if (!item) {
    return;
  }

  // Update the returned NSMenuItem directly so we can update it immediately.
  // There is no need to update the MenuModel as well, since the model is only
  // read from to create the initial NSMenu and never touched again later.
  item.enabled = enabled;
  item.title = base::SysUTF16ToNSString(label);
  item.hidden = !visible;
}

}  // namespace remote_cocoa
