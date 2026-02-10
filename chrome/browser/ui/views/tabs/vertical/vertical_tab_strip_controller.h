// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_CONTROLLER_H_

#include <optional>

#include "chrome/browser/ui/tabs/tab_menu_model_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/tabs/tab/tab_context_menu_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_types.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_drag_handler.h"
#include "components/tab_groups/tab_group_id.h"

class BrowserView;
class TabCollectionNode;
class TabGroup;
class TabHoverCardController;

namespace tabs {
class TabInterface;
class VerticalTabStripStateController;
}

namespace tab_groups {
class TabGroupSyncService;
}

namespace views {
class View;
}

// VerticalTabStripController provides APIs for the views to integrate with rest
// of the browser.
class VerticalTabStripController : public TabContextMenuController::Delegate {
 public:
  VerticalTabStripController(TabStripModel* model,
                             BrowserView* browser_view,
                             VerticalTabDragHandler& drag_handler,
                             TabHoverCardController* hover_card_controller,
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
  void ToggleTabGroupCollapsedState(const TabGroup* group,
                                    ToggleTabGroupCollapsedStateOrigin origin);
  void ShowGroupEditorBubble(const TabCollectionNode* group_node);
  views::Widget* ShowGroupEditorBubble(const tab_groups::TabGroupId& group_id,
                                       views::View* anchor_view,
                                       bool stop_context_menu_propagation);
  bool IsCollapsed() const;

  tab_groups::TabGroupSyncService* GetTabGroupSyncService();

  tabs::VerticalTabStripStateController* GetStateController();

  TabContextMenuController* GetTabContextMenuController() {
    return context_menu_controller_.get();
  }

  VerticalTabDragHandler& GetDragHandler() { return drag_handler_.get(); }
  const VerticalTabDragHandler& GetDragHandler() const {
    return drag_handler_.get();
  }

  TabHoverCardController* GetHoverCardController() {
    return hover_card_controller_.get();
  }

  // Notifies BrowserCommandController that the tab with keyboard focus has
  // changed.
  void TabKeyboardFocusChangedTo(const tabs::TabInterface* tab);

  void TabGroupFocusChanged(
      std::optional<tab_groups::TabGroupId> new_focused_group_id,
      std::optional<tab_groups::TabGroupId> old_focused_group_id);

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

  void RecordMetricsOnTabSelectionChange(
      std::optional<tab_groups::TabGroupId> group);

  std::unique_ptr<TabContextMenuController> context_menu_controller_;
  std::unique_ptr<TabMenuModelFactory> menu_model_factory_;

  raw_ptr<TabStripModel> model_;
  raw_ptr<BrowserView> browser_view_;
  const raw_ref<VerticalTabDragHandler> drag_handler_;
  raw_ptr<TabHoverCardController> hover_card_controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_CONTROLLER_H_
