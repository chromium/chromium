// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_MENU_ACTION_APP_MENU_H_
#define CHROME_BROWSER_UI_VIEWS_APP_MENU_ACTION_APP_MENU_H_

#include <memory>
#include <optional>
#include <set>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_metrics.h"
#include "chrome/browser/ui/views/app_menu/app_menu_drag_and_drop_delegate.h"
#include "ui/actions/action_id.h"
#include "ui/base/command_id_constants.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/controls/menu/menu_delegate.h"

class ActionAppMenuManager;
class AppMenuSearchBarView;
class AppMenuSearchController;
class BrowserWindowInterface;

namespace actions {
class ActionItem;
class BaseAction;
}  // namespace actions

namespace views {
class MenuButtonController;
class MenuItemView;
class MenuRunner;
}  // namespace views

// Coordinator class for the Block Style ChroMenu.
class ActionAppMenu : public views::MenuDelegate,
                      public AppMenuDragAndDropDelegate::Host {
 public:
  ActionAppMenu(BrowserWindowInterface* browser_window_interface,
                base::RepeatingClosure on_menu_closed_callback);
  ActionAppMenu(const ActionAppMenu&) = delete;
  ActionAppMenu& operator=(const ActionAppMenu&) = delete;
  ~ActionAppMenu() override;

  void RunMenu(views::MenuButtonController* host, int run_types);
  bool IsShowing() const;

  // AppMenuDragAndDropDelegate::Host:
  void UpdateMenuItem(actions::BaseAction* action,
                      actions::BaseAction* target_parent_action,
                      actions::BaseAction* insert_after = nullptr) override;
  void CloseMenu() override;

  // views::MenuDelegate:
  void ExecuteCommand(int id, int mouse_event_flags) override;
  void OnMenuClosed(views::MenuItemView* menu) override;
  void WillShowMenu(views::MenuItemView* menu) override;
  bool IsItemChecked(int id) const override;
  const gfx::FontList* GetLabelFontList(int id) const override;
  std::optional<SkColor> GetLabelColor(int id) const override;
  int GetMaxWidthForMenu(views::MenuItemView* menu) override;
  bool GetDropFormats(views::MenuItemView* menu,
                      int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  bool AreDropTypesRequired(views::MenuItemView* menu) override;
  bool CanDrop(views::MenuItemView* menu,
               const ui::OSExchangeData& data) override;
  ui::mojom::DragOperation GetDropOperation(views::MenuItemView* item,
                                            const ui::DropTargetEvent& event,
                                            DropPosition* position) override;
  views::View::DropCallback GetDropCallback(
      views::MenuItemView* menu,
      DropPosition position,
      const ui::DropTargetEvent& event) override;
  bool CanDrag(views::MenuItemView* menu) override;
  void WriteDragData(views::MenuItemView* sender,
                     ui::OSExchangeData* data) override;
  int GetDragOperations(views::MenuItemView* sender) override;
  bool ShouldCloseOnDragDropCompleted() override;

  views::MenuItemView* root_menu_item_for_testing() { return root_; }
  AppMenuSearchBarView* search_bar_for_testing() { return search_bar_; }
  AppMenuSearchController* search_controller_for_testing() {
    return search_controller_.get();
  }
  void SetTimerForTesting(base::ElapsedTimer timer);
  void ClearItemsBelowSearchBarForTesting();

 private:
  actions::BaseAction* GetActionForMenuItem(views::MenuItemView* menu) const;
  views::MenuItemView* GetMenuItemForAction(actions::BaseAction* action) const;
  AppMenuDragAndDropDelegate* GetDragAndDropDelegate(
      actions::BaseAction* action) const;
  void RemoveSubmenuActionsFromMap(views::MenuItemView* parent_menu_item);

  void CancelAndEvaluate(actions::ActionId action_id, int mouse_event_flags);

  void ClearItemsBelowSearchBar();

  // Recursively populates the menu item with the `base_action_item`'s
  // children.
  void PopulateMenu(views::MenuItemView* view_parent,
                    actions::BaseAction* base_action_item);

  // Appends (or inserts at `index`) and returns a menu item to the
  // `parent_menu_item` and adds the `base_action_item` to the command to action
  // map.
  views::MenuItemView* AppendMenuItem(
      actions::BaseAction* base_action_item,
      views::MenuItemView* parent_menu_item,
      std::optional<size_t> index = std::nullopt);

  // Configures the menu item to populate with the correct icon, text, and
  // padding. ConfigureMenuItem() should only be used for clickable menu items
  // within the action app menu or have a sub-menu.
  void ConfigureMenuItem(views::MenuItemView* menu_item,
                         actions::BaseAction* child_base,
                         bool round_top_corners,
                         bool round_bottom_corners,
                         bool add_top_padding,
                         bool add_bottom_padding);

  bool MaybePopulateSearchResults(const std::u16string& query);

  void PopulateSearchBar(views::MenuItemView* view_parent,
                         actions::ActionItem* search_action_item);
  void PopulateHeader(views::MenuItemView* view_parent,
                      actions::BaseAction* header_base_action);
  void PopulateFooter(views::MenuItemView* view_parent,
                      actions::ActionItem* footer_action_item);
  void PopulateBlockSection(views::MenuItemView* view_parent,
                            actions::ActionItem* block_action_item);
  void PopulateCustomRow(views::MenuItemView* view_parent,
                         actions::BaseAction* custom_action_item);
  void PopulateDivider(views::MenuItemView* view_parent,
                       actions::ActionItem* divider_action_item);

  // The browser window interface associated with this menu.
  raw_ptr<BrowserWindowInterface> browser_window_interface_;

  // Callback run when the menu is closed to notify the menu button.
  base::RepeatingClosure on_menu_closed_callback_;

  // Maps command/menu item IDs back to their corresponding BaseAction.
  base::flat_map<int, raw_ptr<actions::BaseAction>> command_to_action_map_;

  // Manages ActionItem and MenuItemView relationships.
  views::ActionViewController action_view_controller_;

  // Manages the widget and popup execution lifecycle of the menu.
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // The root menu item view. Owned by `menu_runner_`.
  raw_ptr<views::MenuItemView> root_ = nullptr;

  // The search bar view in the menu, if kChroMenuSearch is enabled.
  raw_ptr<AppMenuSearchBarView> search_bar_ = nullptr;

  // The states associated with the populated app menu.
  bool has_notification_header_ = false;
  size_t section_header_count_ = 0;

  struct ActionExecutionParams {
    actions::ActionId action_id;
    actions::ActionInvocationContext context;
  };

  // The action and context to execute when the menu is closed.
  std::optional<ActionExecutionParams> action_to_execute_on_close_;

  // Manages the ActionItem hierarchy and dynamic submenus.
  std::unique_ptr<ActionAppMenuManager> menu_manager_;

  // Search controller used when testing search or when search query is active.
  std::unique_ptr<AppMenuSearchController> search_controller_;

  // Records UMA histograms and user actions for menu interactions.
  ActionAppMenuMetrics metrics_;

  int next_id_ = COMMAND_ID_FIRST_UNBOUNDED;

  base::WeakPtrFactory<ActionAppMenu> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_ACTION_APP_MENU_H_
