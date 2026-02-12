// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_BROWSER_TAB_STRIP_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_BROWSER_TAB_STRIP_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/hover_tab_selector.h"
#include "chrome/browser/ui/tabs/tab_menu_model_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/tabs/tab/tab_context_menu_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_types.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/tab_groups/tab_group_color.h"
#include "ui/base/mojom/menu_source_type.mojom-forward.h"
#include "ui/menus/simple_menu_model.h"

class BrowserFrameView;
class BrowserWindowInterface;
class Tab;
class TabGroup;

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace tab_groups {
class TabGroupId;
class TabGroupVisualData;
}  // namespace tab_groups

// An implementation of TabStripController that sources data from the
// WebContentses in a TabStripModel.
class BrowserTabStripController : public TabStripController,
                                  public TabStripModelObserver,
                                  public TabContextMenuController::Delegate {
 public:
  BrowserTabStripController(TabStripModel* model,
                            BrowserView* browser_view,
                            std::unique_ptr<TabMenuModelFactory>
                                menu_model_factory_override = nullptr);
  BrowserTabStripController(const BrowserTabStripController&) = delete;
  BrowserTabStripController& operator=(const BrowserTabStripController&) =
      delete;
  ~BrowserTabStripController() override;

  void InitFromModel(TabStrip* tabstrip);
  void Reset();

  // TabStripController implementation:
  ui::ListSelectionModel GetSelectionModel() const override;

  int GetCount() const override;
  bool IsValidIndex(int model_index) const override;
  bool IsActiveTab(int model_index) const override;
  std::optional<int> GetActiveIndex() const override;
  bool IsTabSelected(int model_index) const override;
  bool IsTabPinned(int model_index) const override;
  bool IsBrowserClosing() const override;
  void SelectTab(int model_index, const ui::Event& event) override;
  void RecordMetricsOnTabSelectionChange(
      std::optional<tab_groups::TabGroupId> group) override;
  void ExtendSelectionTo(int model_index) override;
  void ToggleSelected(int model_index) override;
  void AddSelectionFromAnchorTo(int model_index) override;
  void OnCloseTab(int model_index,
                  CloseTabSource source,
                  base::OnceCallback<void(CloseTabSource)> callback) override;
  void CloseTab(int model_index) override;
  void ToggleTabAudioMute(int model_index) override;
  void AddTabToGroup(int model_index,
                     const tab_groups::TabGroupId& group) override;
  void RemoveTabFromGroup(int model_index) override;
  void MoveTab(int start_index, int final_index) override;
  void MoveGroup(const tab_groups::TabGroupId& group, int final_index) override;
  using TabStripController::ToggleTabGroupCollapsedState;
  void ToggleTabGroupCollapsedState(
      tab_groups::TabGroupId group,
      ToggleTabGroupCollapsedStateOrigin origin) override;
  void ShowContextMenuForTab(Tab* tab,
                             const gfx::Point& p,
                             ui::mojom::MenuSourceType source_type) override;
  int HasAvailableDragActions() const override;
  void OnDropIndexUpdate(std::optional<int> index, bool drop_before) override;
  void CreateNewTab(NewTabTypes context) override;
  void OnStartedDragging() override;
  void OnStoppedDragging() override;
  void TabKeyboardFocusChangedTo(const tabs::TabInterface* tab) override;
  std::u16string GetGroupTitle(
      const tab_groups::TabGroupId& group_id) const override;
  std::u16string GetGroupContentString(
      const tab_groups::TabGroupId& group_id) const override;
  tab_groups::TabGroupColorId GetGroupColorId(
      const tab_groups::TabGroupId& group_id) const override;
  TabGroup* GetTabGroup(const tab_groups::TabGroupId& group_id) const override;
  bool IsGroupCollapsed(const tab_groups::TabGroupId& group) const override;

  std::optional<tab_groups::TabGroupId> GetFocusedGroup() const override;
  void SetFocusedGroup(std::optional<tab_groups::TabGroupId> group) override;

  void SetVisualDataForGroup(
      const tab_groups::TabGroupId& group,
      const tab_groups::TabGroupVisualData& visual_data) override;
  std::optional<int> GetFirstTabInGroup(
      const tab_groups::TabGroupId& group) const override;
  gfx::Range ListTabsInGroup(
      const tab_groups::TabGroupId& group_id) const override;
  std::u16string GetAccessibleTabName(const Tab* tab) const override;
  BrowserWindowInterface* GetBrowserWindowInterface() override;

  // Test-specific methods.
  void CloseContextMenuForTesting();

 private:
  // TabStripModelObserver implementation:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void OnTabWillBeAdded() override;
  void OnTabWillBeRemoved(tabs::TabInterface* tab, int index) override;
  void OnTabGroupChanged(const TabGroupChange& change) override;
  void OnTabChangedAt(tabs::TabInterface* contents,
                      int model_index,
                      TabChangeType change_type) override;
  void OnTabPinnedStateChanged(tabs::TabInterface* tab,
                               int model_index) override;
  void TabGroupedStateChanged(TabStripModel* tab_strip_model,
                              std::optional<tab_groups::TabGroupId> old_group,
                              std::optional<tab_groups::TabGroupId> new_group,
                              tabs::TabInterface* tab,
                              int index) override;
  void OnSplitTabChanged(const SplitTabChange& change) override;
  void OnTabGroupFocusChanged(
      std::optional<tab_groups::TabGroupId> new_focused_group_id,
      std::optional<tab_groups::TabGroupId> old_focused_group_id) override;

  BrowserFrameView* GetFrameView();
  const BrowserFrameView* GetFrameView() const;

  // Invokes tabstrip_->SetTabData.
  void SetTabDataAt(int model_index);

  // Adds tabs to the view model.
  void AddTabs(std::vector<std::pair<tabs::TabInterface*, int>> contents_list);

  void OnDiscardRingTreatmentEnabledChanged();

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

  raw_ptr<TabStripModel> model_;

  raw_ptr<TabStrip> tabstrip_;

  raw_ptr<BrowserView> browser_view_;

  // If non-NULL it means we're showing a menu for the tab.
  std::unique_ptr<TabContextMenuController> context_menu_controller_;

  // Helper for performing tab selection as a result of dragging over a tab.
  HoverTabSelector hover_tab_selector_;

  // Forces the tabs to use the regular (non-immersive) style and the
  // top-of-window views to be revealed when the user is dragging `tabstrip`'s
  // tabs.
  std::unique_ptr<ImmersiveRevealedLock> immersive_reveal_lock_;

  std::unique_ptr<TabMenuModelFactory> menu_model_factory_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_BROWSER_TAB_STRIP_CONTROLLER_H_
