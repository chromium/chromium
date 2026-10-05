// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_DRAG_AND_DROP_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_DRAG_AND_DROP_DELEGATE_H_

#include <set>

#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/view.h"

namespace actions {
class BaseAction;
}  // namespace actions

namespace ui {
class DropTargetEvent;
class OSExchangeData;
}  // namespace ui

// Delegate interface for handling drag-and-drop operations on action items
// within ActionAppMenu. Dynamic menus can implement this interface and attach
// it to the root action of their submenu via
// `AppMenuActionItem::kDragAndDropDelegateKey`.
class AppMenuDragAndDropDelegate {
 public:
  // Interface implemented by ActionAppMenu to allow the delegate to notify the
  // menu when the underlying data model changes while the menu is open.
  class Host {
   public:
    virtual ~Host() = default;

    virtual void UpdateMenuItem(
        actions::BaseAction* action,
        actions::BaseAction* target_parent_action,
        actions::BaseAction* insert_after = nullptr) = 0;
    virtual void CloseMenu() = 0;
  };

  virtual ~AppMenuDragAndDropDelegate() = default;

  virtual bool GetDropFormats(
      actions::BaseAction* action,
      int* formats,
      std::set<ui::ClipboardFormatType>* format_types) = 0;
  virtual bool AreDropTypesRequired(actions::BaseAction* action) = 0;
  virtual bool CanDrop(actions::BaseAction* action,
                       const ui::OSExchangeData& data) = 0;
  virtual ui::mojom::DragOperation GetDropOperation(
      actions::BaseAction* action,
      const ui::DropTargetEvent& event,
      views::MenuDelegate::DropPosition* position) = 0;
  virtual views::View::DropCallback GetDropCallback(
      actions::BaseAction* action,
      views::MenuDelegate::DropPosition position,
      const ui::DropTargetEvent& event) = 0;
  virtual bool CanDrag(actions::BaseAction* action) = 0;
  virtual void WriteDragData(actions::BaseAction* action,
                             ui::OSExchangeData* data) = 0;
  virtual int GetDragOperations(actions::BaseAction* action) = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_DRAG_AND_DROP_DELEGATE_H_
