// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_FAKE_BASE_TAB_STRIP_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_FAKE_BASE_TAB_STRIP_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/optional.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_types.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/base/models/list_selection_model.h"

class FakeBaseTabStripController : public TabStripController {
 public:
  FakeBaseTabStripController();
  FakeBaseTabStripController(const FakeBaseTabStripController&) = delete;
  FakeBaseTabStripController& operator=(const FakeBaseTabStripController&) =
      delete;
  ~FakeBaseTabStripController() override;

  void AddTab(int index, bool is_active);
  void AddPinnedTab(int index, bool is_active);
  void RemoveTab(int index);

  void MoveTabIntoGroup(int index,
                        base::Optional<tab_groups::TabGroupId> new_group);

  ui::ListSelectionModel* selection_model() { return &selection_model_; }

  void set_tab_strip(TabStrip* tab_strip) { tab_strip_ = tab_strip; }

  // TabStripController overrides:
  const ui::ListSelectionModel& GetSelectionModel() const override;
  int GetCount() const override;
  bool IsValidIndex(int index) const override;
  bool IsActiveTab(int index) const override;
  int GetActiveIndex() const override;
  bool IsTabSelected(int index) const override;
  bool IsTabPinned(int index) const override;
  void SelectTab(int index, const ui::Event& event) override;
  void ExtendSelectionTo(int index) override;
  void ToggleSelected(int index) override;
  void AddSelectionFromAnchorTo(int index) override;
  bool BeforeCloseTab(int index, CloseTabSource source) override;
  void CloseTab(int index) override;
  void MoveTab(int from_index, int to_index) override;
  void MoveGroup(const tab_groups::TabGroupId&, int to_index) override;
  bool ToggleTabGroupCollapsedState(
      const tab_groups::TabGroupId group,
      ToggleTabGroupCollapsedStateOrigin origin) override;
  void ShowContextMenuForTab(Tab* tab,
                             const gfx::Point& p,
                             ui::MenuSourceType source_type) override;
  int HasAvailableDragActions() const override;
  void OnDropIndexUpdate(int index, bool drop_before) override;
  void CreateNewTab() override;
  void CreateNewTabWithLocation(const std::u16string& loc) override;
  void StackedLayoutMaybeChanged() override;
  void OnStartedDragging(bool dragging_window) override;
  void OnStoppedDragging() override;
  void OnKeyboardFocusedTabChanged(base::Optional<int> index) override;
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
  base::Optional<int> GetFirstTabInGroup(
      const tab_groups::TabGroupId& group) const override;
  base::Optional<int> GetLastTabInGroup(
      const tab_groups::TabGroupId& group) const override;
  gfx::Range ListTabsInGroup(
      const tab_groups::TabGroupId& group) const override;
  void AddTabToGroup(int model_index,
                     const tab_groups::TabGroupId& group) override;
  void RemoveTabFromGroup(int model_index) override;
  bool IsFrameCondensed() const override;
  bool HasVisibleBackgroundTabShapes() const override;
  bool EverHasVisibleBackgroundTabShapes() const override;
  bool ShouldPaintAsActiveFrame() const override;
  bool CanDrawStrokes() const override;
  SkColor GetFrameColor(BrowserFrameActiveState active_state) const override;
  SkColor GetToolbarTopSeparatorColor() const override;
  base::Optional<int> GetCustomBackgroundId(
      BrowserFrameActiveState active_state) const override;
  std::u16string GetAccessibleTabName(const Tab* tab) const override;
  Profile* GetProfile() const override;
  const Browser* GetBrowser() const override;

 private:
  void SetActiveIndex(int new_index);

  TabStrip* tab_strip_ = nullptr;

  int num_tabs_ = 0;
  int active_index_ = -1;

  tab_groups::TabGroupVisualData fake_group_data_;
  std::vector<base::Optional<tab_groups::TabGroupId>> tab_groups_;

  ui::ListSelectionModel selection_model_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_FAKE_BASE_TAB_STRIP_CONTROLLER_H_
