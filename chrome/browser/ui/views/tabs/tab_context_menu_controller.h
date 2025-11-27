// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTEXT_MENU_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTEXT_MENU_CONTROLLER_H_

#include <memory>

#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/menus/simple_menu_model.h"

namespace gfx {
class Point;
}

namespace views {
class MenuRunner;
class Widget;
}  // namespace views

// TabContextMenuController manages the logic for the TabContextMenu in both the
// vertical and horizontal tab strips. Given callbacks for the necessary
// commands, it creates and runs the menu. The typical usage for this class is
// as follows:
// 1. Create the TabContextMenuController using the callbacks.
// 2. Create the model from a menu_model_factory instance.
// 3. Load the model.
// 4. Call RunMenuAt.
class TabContextMenuController : public ui::SimpleMenuModel::Delegate {
 public:
  explicit TabContextMenuController(
      base::RepeatingCallback<bool(TabStripModel::ContextMenuCommand)>
          is_command_checked,
      base::RepeatingCallback<bool(TabStripModel::ContextMenuCommand)>
          is_command_enabled,
      base::RepeatingCallback<bool(TabStripModel::ContextMenuCommand)>
          is_command_alerted,
      base::RepeatingCallback<void(TabStripModel::ContextMenuCommand, int)>
          execute_command,
      base::RepeatingCallback<bool(int, ui::Accelerator*)> get_accelerator);

  ~TabContextMenuController() override;

  // Loads the menu model and initializes the menu runner. This must be called
  // before RunMenuAt.
  void LoadModel(std::unique_ptr<ui::SimpleMenuModel> model);

  // Runs the menu model at the specified point within the given widget.
  void RunMenuAt(const gfx::Point& point,
                 ui::mojom::MenuSourceType source_type,
                 views::Widget* widget);

  // Testing function to close the Tab Context Menu.
  void CloseMenu();

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsCommandIdAlerted(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;

 private:
  std::unique_ptr<ui::SimpleMenuModel> model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // These callbacks handle the menu's state and execution. Utilizing callbacks
  // allows for other TabStripControllers to utilize the
  // TabContextMenuController in differing contexts.
  base::RepeatingCallback<bool(TabStripModel::ContextMenuCommand)>
      is_command_checked_;
  base::RepeatingCallback<bool(TabStripModel::ContextMenuCommand)>
      is_command_enabled_;
  base::RepeatingCallback<bool(TabStripModel::ContextMenuCommand)>
      is_command_alerted_;
  base::RepeatingCallback<void(TabStripModel::ContextMenuCommand, int)>
      execute_command_;
  base::RepeatingCallback<bool(int, ui::Accelerator*)> get_accelerator_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTEXT_MENU_CONTROLLER_H_
