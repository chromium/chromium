// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/renderer_context_menu/views/toolkit_delegate_views.h"

#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/submenu_view.h"

ToolkitDelegateViews::ToolkitDelegateViews() : menu_view_(nullptr) {}

ToolkitDelegateViews::~ToolkitDelegateViews() {}

void ToolkitDelegateViews::RunMenuAt(views::Widget* parent,
                                     const gfx::Point& point,
                                     ui::MenuSourceType type) {
  using Position = views::MenuAnchorPosition;
  Position anchor_position =
      (type == ui::MENU_SOURCE_TOUCH || type == ui::MENU_SOURCE_TOUCH_EDIT_MENU)
          ? Position::kBottomCenter
          : Position::kTopLeft;
  menu_runner_->RunMenuAt(parent, nullptr, gfx::Rect(point, gfx::Size()),
                          anchor_position, type);
}

void ToolkitDelegateViews::Init(ui::SimpleMenuModel* menu_model) {
  menu_adapter_.reset(new views::MenuModelAdapter(menu_model));
  menu_view_ = menu_adapter_->CreateMenu();
  menu_runner_.reset(new views::MenuRunner(
      menu_view_,
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU));
}

void ToolkitDelegateViews::Cancel() {
  DCHECK(menu_runner_);
  menu_runner_->Cancel();
}

void ToolkitDelegateViews::RebuildMenu() {
  menu_adapter_->BuildMenu(menu_view_);
}
