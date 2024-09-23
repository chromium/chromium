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
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_types.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/tab_groups/tab_group_color.h"
#include "ui/base/models/simple_menu_model.h"

class Browser;
class BrowserNonClientFrameView;
class Tab;

namespace content {
class WebContents;
}  // namespace content

namespace tab_groups {
class TabGroupId;
class TabGroupVisualData;
}  // namespace tab_groups

namespace ui {
class ListSelectionModel;
}

// An implementation of TabStripController that sources data from the
// WebContentses in a TabStripModel.
class BrowserTabStripController : public TabStripController,
                                  public TabStripModelObserver {
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

  TabStripModel* model() const { return model_; }

  bool IsCommandEnabledForTab(TabStripModel::ContextMenuCommand command_id,
                              const Tab* tab) const;
  void ExecuteCommandForTab(TabStripModel::ContextMenuCommand command_id,
                            const Tab* tab);
  bool IsTabPinned(const Tab* tab) const;

  // TabStripController implementation:
  const ui::ListSelectionModel& GetSelectionModel() const override;
  int GetCount() const override;
  bool IsValidIndex(int model_index) const override;
  bool IsActiveTab(int model_index) const override;
  std::optional<int> GetActiveIndex() const override;
  bool IsTabSelected(int model_index) const override;
  bool IsTabPinned(int model_index) const override;
  void SelectTab(int model_index, const ui::Event& event) override;
  void ExtendSelectionTo(int model_index) override;
  void ToggleSelected(int model_index) override;
  void AddSelectionFromAnchorTo(int model_index) override;
  void OnCloseTab(int model_index,
                  CloseTabSource source,
                  base::OnceCallback<void()> callback) override;
  void CloseTab(int model_index) override;
  void ToggleTabAudioMute(int model_index) override;
  void AddTabToGroup(int model_index,
                     const tab_groups::TabGroupId& group) override;
  void RemoveTabFromGroup(int model_index) override;
  void MoveTab(int start_index, int final_index) override;
  void MoveGroup(const tab_groups::TabGroupId& group, int final_index) override;
  void ToggleTabGroupCollapsedState(
      const tab_groups::TabGroupId group,
      ToggleTabGroupCollapsedStateOrigin origin) override;
  void ShowContextMenuForTab(Tab* tab,
                             const gfx::Point& p,
                             ui::MenuSourceType source_type) override;
  int HasAvailableDragActions() const override;
  void OnDropIndexUpdate(std::optional<int> index, bool drop_before) override;
  void CreateNewTab() override;
  void CreateNewTabWithLocation(const std::u16string& loc) override;
  void OnStartedDragging(bool dragging_window) override;
  void OnStoppedDragging() override;
  void OnKeyboardFocusedTabChanged(std::optional<int> index) override;
  std::u16string GetGroupTitle(
      const tab_groups::TabGroupId& group_id) const override;
  std::u16string GetGroupContentString(
      const tab_groups::TabGroupId& group_id) const override;
  tab_groups::TabGroupColorId GetGroupColorId(
      const tab_groups::TabGroupId& group_id) const override;
  bool IsGroupCollapsed(const tab_groups::TabGroupId& group) const override;

  void SetVisualDataForGroup(
      const tab_groups::TabGroupId& group,
      const tab_groups::TabGroupVisualData& visual_data) override;
  std::optional<int> GetFirstTabInGroup(
      const tab_groups::TabGroupId& group) const override;
  gfx::Range ListTabsInGroup(
      const tab_groups::TabGroupId& group_id) const override;
  bool IsFrameCondensed() const override;
  bool HasVisibleBackgroundTabShapes() const override;
  bool EverHasVisibleBackgroundTabShapes() const override;
  bool CanDrawStrokes() const override;
  SkColor GetFrameColor(BrowserFrameActiveState active_state) const override;
  std::optional<int> GetCustomBackgroundId(
      BrowserFrameActiveState active_state) const override;
  std::u16string GetAccessibleTabName(const Tab* tab) const override;
  Profile* GetProfile() const override;
  const Browser* GetBrowser() const override;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool IsLockedForOnTask() override;
#endif

  // TabStripModelObserver implementation:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void OnTabWillBeAdded() override;
  void OnTabWillBeRemoved(content::WebContents* contents, int index) override;
  void OnTabGroupChanged(const TabGroupChange& change) override;
  void TabChangedAt(content::WebContents* contents,
                    int model_index,
                    TabChangeType change_type) override;
  void TabPinnedStateChanged(TabStripModel* tab_strip_model,
                             content::WebContents* contents,
                             int model_index) override;
  void TabBlockedStateChanged(content::WebContents* contents,
                              int model_index) override;
  void TabGroupedStateChanged(std::optional<tab_groups::TabGroupId> group,
                              tabs::TabModel* tab,
                              int index) override;
  void SetTabNeedsAttentionAt(int index, bool attention) override;
  bool IsFrameButtonsRightAligned() const override;
  const Browser* browser() const { return browser_view_->browser(); }

  // Test-specific methods.
  void CloseContextMenuForTesting();

 private:
  class TabContextMenuContents;

  BrowserNonClientFrameView* GetFrameView();
  const BrowserNonClientFrameView* GetFrameView() const;

  // Invokes tabstrip_->SetTabData.
  void SetTabDataAt(content::WebContents* web_contents, int model_index);

  // Adds a tab.
  void AddTab(content::WebContents* contents, int index);

  void OnDiscardRingTreatmentEnabledChanged();

  raw_ptr<TabStripModel> model_;

  raw_ptr<TabStrip> tabstrip_;

  raw_ptr<BrowserView> browser_view_;

  // If non-NULL it means we're showing a menu for the tab.
  std::unique_ptr<TabContextMenuContents> context_menu_contents_;

  // Helper for performing tab selection as a result of dragging over a tab.
  HoverTabSelector hover_tab_selector_;

  // Forces the tabs to use the regular (non-immersive) style and the
  // top-of-window views to be revealed when the user is dragging |tabstrip|'s
  // tabs.
  std::unique_ptr<ImmersiveRevealedLock> immersive_reveal_lock_;

  PrefChangeRegistrar local_state_registrar_;

  std::unique_ptr<TabMenuModelFactory> menu_model_factory_;

  bool should_show_discard_indicator_ = true;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_BROWSER_TAB_STRIP_CONTROLLER_H_
