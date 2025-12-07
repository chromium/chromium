// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_CONTROLLER_H_

#include "chrome/browser/ui/tabs/tab_menu_model_factory.h"
#include "chrome/browser/ui/views/tabs/tab_context_menu_controller.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"

class BrowserView;
class TabCollectionNode;

namespace views {
class View;
}

// VerticalTabStripController manages the behavior of the vertical tab strip. It
// performs a similar functionality as BrowserTabStripController.
class VerticalTabStripController {
 public:
  VerticalTabStripController(TabStripModel* model,
                             BrowserView* browser_view,
                             std::unique_ptr<TabMenuModelFactory>
                                 menu_model_factory_override = nullptr);
  VerticalTabStripController(const VerticalTabStripController&) = delete;
  VerticalTabStripController& operator=(const VerticalTabStripController&) =
      delete;
  ~VerticalTabStripController();

  void ShowContextMenuForNode(TabCollectionNode* collection_node,
                              views::View* source,
                              const gfx::Point& point,
                              ui::mojom::MenuSourceType source_type);

  std::optional<int> GetIndexFromMojomTab(
      const tabs_api::mojom::Tab& mojom_tab);

  TabContextMenuController* GetTabContextMenuController() {
    return context_menu_controller_.get();
  }

 private:
  bool IsContextMenuCommandChecked(
      TabStripModel::ContextMenuCommand command_id);
  bool IsContextMenuCommandEnabled(
      int index,
      TabStripModel::ContextMenuCommand command_id);
  bool IsContextMenuCommandAlerted(
      TabStripModel::ContextMenuCommand command_id);
  void ExecuteContextMenuCommand(int index,
                                 TabStripModel::ContextMenuCommand command_id,
                                 int event_flags);
  bool GetContextMenuAccelerator(int command_id, ui::Accelerator* accelerator);

  std::unique_ptr<TabContextMenuController> context_menu_controller_;
  std::unique_ptr<TabMenuModelFactory> menu_model_factory_;

  raw_ptr<TabStripModel> model_;
  raw_ptr<BrowserView> browser_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_CONTROLLER_H_
