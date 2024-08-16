// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_FAKE_BASE_TAB_STRIP_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_FAKE_BASE_TAB_STRIP_CONTROLLER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_types.h"
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

  void AddTab(int index,
              TabActive is_active,
              TabPinned is_pinned = TabPinned::kUnpinned);
  void RemoveTab(int index);

  void MoveTabIntoGroup(int index,
                        std::optional<tab_groups::TabGroupId> new_group);

  ui::ListSelectionModel* selection_model() { return &selection_model_; }

  void set_tab_strip(TabStrip* tab_strip) { tab_strip_ = tab_strip; }

  // TabStripController overrides:
  const ui::ListSelectionModel& GetSelectionModel() const override;
  int GetCount() const override;
  bool IsValidIndex(int index) const override;
  bool IsActiveTab(int index) const override;
  std::optional<int> GetActiveIndex() const override;
  bool IsTabSelected(int index) const override;
  bool IsTabPinned(int index) const override;
  void SelectTab(int index, const ui::Event& event) override;
  void ExtendSelectionTo(int index) override;
  void ToggleSelected(int index) override;
  void AddSelectionFromAnchorTo(int index) override;
  void OnCloseTab(int index,
                  CloseTabSource source,
                  base::OnceCallback<void()> callback) override;
  void CloseTab(int index) override;
  void ToggleTabAudioMute(int index) override;
  void MoveTab(int from_index, int to_index) override;
  void MoveGroup(const tab_groups::TabGroupId&, int to_index) override;
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
      const tab_groups::TabGroupId& group) const override;
  void AddTabToGroup(int model_index,
                     const tab_groups::TabGroupId& group) override;
  void RemoveTabFromGroup(int model_index) override;
  bool IsFrameCondensed() const override;
  bool HasVisibleBackgroundTabShapes() const override;
  bool EverHasVisibleBackgroundTabShapes() const override;
  bool CanDrawStrokes() const override;
  bool IsFrameButtonsRightAligned() const override;
  SkColor GetFrameColor(BrowserFrameActiveState active_state) const override;
  std::optional<int> GetCustomBackgroundId(
      BrowserFrameActiveState active_state) const override;
  std::u16string GetAccessibleTabName(const Tab* tab) const override;
  Profile* GetProfile() const override;
  const Browser* GetBrowser() const override;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool IsLockedForOnTask() override;

  // Sets OnTask locked for testing purposes. Only relevant for non-web browser
  // scenarios.
  void SetLockedForOnTask(bool locked) { on_task_locked_ = locked; }
#endif

 private:
  void SetActiveIndex(int new_index);

  // If not nullptr, is kept in sync as |this| is changed.
  raw_ptr<TabStrip> tab_strip_ = nullptr;

  int num_tabs_ = 0;
  int num_pinned_tabs_ = 0;
  std::optional<int> active_index_ = std::nullopt;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool on_task_locked_ = false;
#endif

  tab_groups::TabGroupVisualData fake_group_data_;
  std::vector<std::optional<tab_groups::TabGroupId>> tab_groups_;

  ui::ListSelectionModel selection_model_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_FAKE_BASE_TAB_STRIP_CONTROLLER_H_
