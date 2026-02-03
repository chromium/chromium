// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_CONTROLLER_H_

#include <optional>

#include "chrome/browser/ui/tabs/tab_menu_model_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/browser/ui/views/tabs/tab_context_menu_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_types.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_drag_handler.h"
#include "components/tab_groups/tab_group_id.h"

class BrowserView;
class TabCollectionNode;
class TabGroup;

namespace tabs {
class TabInterface;
}

namespace tab_groups {
class TabGroupSyncService;
}

namespace views {
class View;
}

// VerticalTabStripController manages the behavior of the vertical tab strip. It
// performs a similar functionality as BrowserTabStripController.
class VerticalTabStripController : public TabContextMenuController::Delegate,
                                   public TabStripModelObserver {
 public:
  VerticalTabStripController(TabStripModel* model,
                             BrowserView* browser_view,
                             VerticalTabDragHandler& drag_handler,
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

  // This method should be called when the mouse has entered the tab strip. This
  // is used as a baseline for some metrics.
  void OnTabStripMouseEntered();

  // This method should be called when a tab has been pressed. This could be to
  // activate a tab, drag a tab, open a context menu or close a tab.
  void OnTabMousePressed();

  tab_groups::TabGroupSyncService* GetTabGroupSyncService();

  TabContextMenuController* GetTabContextMenuController() {
    return context_menu_controller_.get();
  }

  VerticalTabDragHandler& GetDragHandler() { return drag_handler_.get(); }

  // Notifies BrowserCommandController that the tab with keyboard focus has
  // changed.
  void TabKeyboardFocusChangedTo(const tabs::TabInterface* tab);

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

  // TabStripModelObserver:
  void OnTabGroupFocusChanged(
      std::optional<tab_groups::TabGroupId> new_focused_group_id,
      std::optional<tab_groups::TabGroupId> old_focused_group_id) override;

  // Used for seek time metrics from the time the mouse enters the tabstrip.
  std::optional<base::TimeTicks> mouse_entered_tabstrip_time_;

  std::unique_ptr<TabContextMenuController> context_menu_controller_;
  std::unique_ptr<TabMenuModelFactory> menu_model_factory_;

  raw_ptr<TabStripModel> model_;
  raw_ptr<BrowserView> browser_view_;
  const raw_ref<VerticalTabDragHandler> drag_handler_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_CONTROLLER_H_
