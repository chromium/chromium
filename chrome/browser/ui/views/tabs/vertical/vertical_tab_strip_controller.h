// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_CONTROLLER_H_

#include "chrome/browser/ui/tabs/tab_menu_model_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/browser/ui/views/tabs/tab_context_menu_controller.h"

class BrowserView;
class TabCollectionNode;

namespace tabs {
class TabInterface;
}

namespace views {
class View;
}

// VerticalTabStripController manages the behavior of the vertical tab strip. It
// performs a similar functionality as BrowserTabStripController.
class VerticalTabStripController : public TabContextMenuController::Delegate {
 public:
  VerticalTabStripController(TabStripModel* model,
                             BrowserView* browser_view,
                             std::unique_ptr<TabMenuModelFactory>
                                 menu_model_factory_override = nullptr);
  VerticalTabStripController(const VerticalTabStripController&) = delete;
  VerticalTabStripController& operator=(const VerticalTabStripController&) =
      delete;
  ~VerticalTabStripController() override;

  void ShowContextMenuForNode(TabCollectionNode* collection_node,
                              views::View* source,
                              const gfx::Point& point,
                              ui::mojom::MenuSourceType source_type);

  void SelectTab(const tabs::TabInterface* tab_interface,
                 const TabStripUserGestureDetails& event);
  void CloseTab(const tabs::TabInterface* tab_interface);
  void ToggleSelected(const tabs::TabInterface* tab_interface);
  void AddSelectionFromAnchorTo(const tabs::TabInterface* tab_interface);
  void ExtendSelectionTo(const tabs::TabInterface* tab_interface);

  TabContextMenuController* GetTabContextMenuController() {
    return context_menu_controller_.get();
  }

 private:
  // TabContextMenuController::Delegate:
  bool IsContextMenuCommandChecked(
      TabStripModel::ContextMenuCommand command_id) override;
  bool IsContextMenuCommandEnabled(
      int index,
      TabStripModel::ContextMenuCommand command_id) override;
  bool IsContextMenuCommandAlerted(
      TabStripModel::ContextMenuCommand command_id) override;
  void ExecuteContextMenuCommand(int index,
                                 TabStripModel::ContextMenuCommand command_id,
                                 int event_flags) override;
  bool GetContextMenuAccelerator(int command_id,
                                 ui::Accelerator* accelerator) override;

  std::unique_ptr<TabContextMenuController> context_menu_controller_;
  std::unique_ptr<TabMenuModelFactory> menu_model_factory_;

  raw_ptr<TabStripModel> model_;
  raw_ptr<BrowserView> browser_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_CONTROLLER_H_
