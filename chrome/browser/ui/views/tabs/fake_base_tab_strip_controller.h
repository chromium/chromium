// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_FAKE_BASE_TAB_STRIP_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_FAKE_BASE_TAB_STRIP_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/ui/tabs/tab_group_id.h"
#include "chrome/browser/ui/tabs/tab_group_visual_data.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "ui/base/models/list_selection_model.h"

class FakeBaseTabStripController : public TabStripController {
 public:
  FakeBaseTabStripController();
  ~FakeBaseTabStripController() override;

  void AddTab(int index, bool is_active);
  void AddPinnedTab(int index, bool is_active);
  void RemoveTab(int index);

  void MoveTabIntoGroup(int index, base::Optional<TabGroupId> new_group);

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
  void CloseTab(int index, CloseTabSource source) override;
  void MoveTab(int from_index, int to_index) override;
  void ShowContextMenuForTab(Tab* tab,
                             const gfx::Point& p,
                             ui::MenuSourceType source_type) override;
  int HasAvailableDragActions() const override;
  void OnDropIndexUpdate(int index, bool drop_before) override;
  void CreateNewTab() override;
  void CreateNewTabWithLocation(const base::string16& loc) override;
  void StackedLayoutMaybeChanged() override;
  void OnStartedDragging() override;
  void OnStoppedDragging() override;
  void OnKeyboardFocusedTabChanged(base::Optional<int> index) override;
  const TabGroupVisualData* GetVisualDataForGroup(
      TabGroupId group_id) const override;
  void SetVisualDataForGroup(TabGroupId group,
                             TabGroupVisualData visual_data) override;
  std::vector<int> ListTabsInGroup(TabGroupId group) const override;
  void UngroupAllTabsInGroup(TabGroupId group) override;
  void AddNewTabInGroup(TabGroupId group) override;
  bool IsFrameCondensed() const override;
  bool HasVisibleBackgroundTabShapes() const override;
  bool EverHasVisibleBackgroundTabShapes() const override;
  bool ShouldPaintAsActiveFrame() const override;
  bool CanDrawStrokes() const override;
  SkColor GetFrameColor(BrowserFrameActiveState active_state) const override;
  SkColor GetToolbarTopSeparatorColor() const override;
  base::Optional<int> GetCustomBackgroundId(
      BrowserFrameActiveState active_state) const override;
  base::string16 GetAccessibleTabName(const Tab* tab) const override;
  Profile* GetProfile() const override;
  const Browser* GetBrowser() const override;

 private:
  void SetActiveIndex(int new_index);

  TabStrip* tab_strip_ = nullptr;

  int num_tabs_ = 0;
  int active_index_ = -1;

  TabGroupVisualData fake_group_data_;
  std::vector<base::Optional<TabGroupId>> tab_groups_;

  ui::ListSelectionModel selection_model_;

  DISALLOW_COPY_AND_ASSIGN(FakeBaseTabStripController);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_FAKE_BASE_TAB_STRIP_CONTROLLER_H_
