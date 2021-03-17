// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RENDERER_CONTEXT_MENU_VIEWS_TOOLKIT_DELEGATE_VIEWS_H_
#define COMPONENTS_RENDERER_CONTEXT_MENU_VIEWS_TOOLKIT_DELEGATE_VIEWS_H_

#include <memory>

#include "base/macros.h"
#include "components/renderer_context_menu/render_view_context_menu_base.h"
#include "ui/base/ui_base_types.h"

namespace gfx {
class Point;
}

namespace views {
class MenuItemView;
class MenuModelAdapter;
class MenuRunner;
class Widget;
}

namespace ui {
class SimpleMenuModel;
}

class ToolkitDelegateViews : public RenderViewContextMenuBase::ToolkitDelegate {
 public:
  ToolkitDelegateViews();
  ~ToolkitDelegateViews() override;

  void RunMenuAt(views::Widget* parent,
                 const gfx::Point& point,
                 ui::MenuSourceType type);
  views::MenuItemView* menu_view() { return menu_view_; }

 protected:
  // ToolkitDelegate:
  void Init(ui::SimpleMenuModel* menu_model) override;

 private:
  // ToolkitDelegate:
  void Cancel() override;
  void RebuildMenu() override;

  std::unique_ptr<views::MenuModelAdapter> menu_adapter_;
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // Weak. Owned by menu_runner_;
  views::MenuItemView* menu_view_;

  DISALLOW_COPY_AND_ASSIGN(ToolkitDelegateViews);
};

#endif  // COMPONENTS_RENDERER_CONTEXT_MENU_VIEWS_TOOLKIT_DELEGATE_VIEWS_H_
