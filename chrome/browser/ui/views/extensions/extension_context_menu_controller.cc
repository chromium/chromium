// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_context_menu_controller.h"

#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"

ExtensionContextMenuController::ExtensionContextMenuController(
    ToolbarActionView::Delegate* delegate,
    ToolbarActionViewController* controller)
    : delegate_(delegate), controller_(controller) {}

ExtensionContextMenuController::~ExtensionContextMenuController() = default;

void ExtensionContextMenuController::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  ui::MenuModel* model = controller_->GetContextMenu();

  // It's possible the action doesn't have a context menu.
  if (!model)
    return;

  int run_types =
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU;

  views::Widget* parent;
  if (delegate_ && delegate_->ShownInsideMenu()) {
    run_types |= views::MenuRunner::IS_NESTED;
    parent = delegate_->GetOverflowReferenceView()->GetWidget();
  } else {
    parent = source->GetWidget();
  }

  // Unretained() is safe here as ToolbarActionView will always outlive the
  // menu. Any action that would lead to the deletion of |this| first triggers
  // the closing of the menu through lost capture.
  menu_adapter_ = std::make_unique<views::MenuModelAdapter>(
      model, base::BindRepeating(&ExtensionContextMenuController::OnMenuClosed,
                                 base::Unretained(this)));

  menu_ = menu_adapter_->CreateMenu();

  menu_runner_ = std::make_unique<views::MenuRunner>(menu_, run_types);

  menu_runner_->RunMenuAt(
      parent,
      static_cast<views::MenuButtonController*>(
          views::Button::AsButton(source)->button_controller()),
      source->GetAnchorBoundsInScreen(), views::MenuAnchorPosition::kTopLeft,
      source_type);
}

bool ExtensionContextMenuController::IsMenuRunning() const {
  return menu_runner_ && menu_runner_->IsRunning();
}

void ExtensionContextMenuController::OnMenuClosed() {
  menu_runner_.reset();
  menu_ = nullptr;
  controller_->OnContextMenuClosed();
  menu_adapter_.reset();
}
