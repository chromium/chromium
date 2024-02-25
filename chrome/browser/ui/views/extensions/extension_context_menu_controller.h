// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_CONTEXT_MENU_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_CONTEXT_MENU_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "ui/views/context_menu_controller.h"

class ToolbarActionViewController;

namespace views {
class MenuModelAdapter;
class MenuRunner;
}  // namespace views

class ExtensionContextMenuController : public views::ContextMenuController {
 public:
  explicit ExtensionContextMenuController(
      ToolbarActionViewController* controller,
      extensions::ExtensionContextMenuModel::ContextMenuSource
          context_menu_source);

  ExtensionContextMenuController(const ExtensionContextMenuController&) =
      delete;
  ExtensionContextMenuController& operator=(
      const ExtensionContextMenuController&) = delete;

  ~ExtensionContextMenuController() override;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  bool IsMenuRunning() const;

 private:
  // Callback for MenuModelAdapter.
  void OnMenuClosed();

  // Responsible for converting the context menu model into |menu_|.
  std::unique_ptr<views::MenuModelAdapter> menu_adapter_;

  // Responsible for running the menu.
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // This controller contains the data for the extension's context menu.
  const raw_ptr<ToolbarActionViewController> controller_;

  // Location where the context menu is open from.
  extensions::ExtensionContextMenuModel::ContextMenuSource context_menu_source_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_CONTEXT_MENU_CONTROLLER_H_
