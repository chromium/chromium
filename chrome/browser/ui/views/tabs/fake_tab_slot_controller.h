// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_FAKE_TAB_SLOT_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_FAKE_TAB_SLOT_CONTROLLER_H_

#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_slot_controller.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/gfx/color_palette.h"

class TabContainer;
class TabStripController;

class FakeTabSlotController : public TabSlotController {
 public:
  explicit FakeTabSlotController(
      TabStripController* tab_strip_controller_ = nullptr);
  FakeTabSlotController(const FakeTabSlotController&) = delete;
  FakeTabSlotController& operator=(const FakeTabSlotController&) = delete;
  ~FakeTabSlotController() override = default;

  void set_tab_container(TabContainer* tab_container) {
    tab_container_ = tab_container;
  }
  void set_active_tab(Tab* tab) { active_tab_ = tab; }
  void set_paint_throbber_to_layer(bool value) {
    paint_throbber_to_layer_ = value;
  }

  const ui::ListSelectionModel& GetSelectionModel() const override;
  Tab* tab_at(int index) const override;
  void SelectTab(Tab* tab, const ui::Event& event) override {}
  void ExtendSelectionTo(Tab* tab) override {}
  void ToggleSelected(Tab* tab) override {}
  void AddSelectionFromAnchorTo(Tab* tab) override {}
  void CloseTab(Tab* tab, CloseTabSource source) override {}
  void ToggleTabAudioMute(Tab* tab) override {}
  void ShiftTabNext(Tab* tab) override {}
  void ShiftTabPrevious(Tab* tab) override {}
  void MoveTabFirst(Tab* tab) override {}
  void MoveTabLast(Tab* tab) override {}
  void ToggleTabGroupCollapsedState(
      const tab_groups::TabGroupId group,
      ToggleTabGroupCollapsedStateOrigin origin =
          ToggleTabGroupCollapsedStateOrigin::kMenuAction) override;
  void NotifyTabGroupEditorBubbleOpened() override {}
  void NotifyTabGroupEditorBubbleClosed() override {}

  void ShowContextMenuForTab(Tab* tab,
                             const gfx::Point& p,
                             ui::MenuSourceType source_type) override {}
  bool IsActiveTab(const Tab* tab) const override;
  bool IsTabSelected(const Tab* tab) const override;
  bool IsTabPinned(const Tab* tab) const override;
  bool IsTabFirst(const Tab* tab) const override;
  bool IsFocusInTabs() const override;
  bool ShouldCompactLeadingEdge() const override;
  void MaybeStartDrag(
      TabSlotView* source,
      const ui::LocatedEvent& event,
      const ui::ListSelectionModel& original_selection) override {}
  Liveness ContinueDrag(views::View* view,
                        const ui::LocatedEvent& event) override;
  bool EndDrag(EndDragReason reason) override;
  Tab* GetTabAt(const gfx::Point& point) override;
  const Tab* GetAdjacentTab(const Tab* tab, int offset) override;
  void OnMouseEventInTab(views::View* source,
                         const ui::MouseEvent& event) override {}
  void UpdateHoverCard(Tab* tab, HoverCardUpdateType update_type) override {}
  bool HoverCardIsShowingForTab(Tab* tab) override;
  int GetBackgroundOffset() const override;
  int GetStrokeThickness() const override;
  bool CanPaintThrobberToLayer() const override;
  bool HasVisibleBackgroundTabShapes() const override;
  SkColor GetTabSeparatorColor() const override;
  SkColor GetTabForegroundColor(TabActive active) const override;
  std::optional<int> GetCustomBackgroundId(
      BrowserFrameActiveState active_state) const override;
  std::u16string GetAccessibleTabName(const Tab* tab) const override;
  float GetHoverOpacityForTab(float range_parameter) const override;
  float GetHoverOpacityForRadialHighlight() const override;

  std::u16string GetGroupTitle(
      const tab_groups::TabGroupId& group) const override;
  std::u16string GetGroupContentString(
      const tab_groups::TabGroupId& group) const override;
  tab_groups::TabGroupColorId GetGroupColorId(
      const tab_groups::TabGroupId& group) const override;
  bool IsGroupCollapsed(const tab_groups::TabGroupId& group) const override;
  SkColor GetPaintedGroupColor(
      const tab_groups::TabGroupColorId& color_id) const override;
  void ShiftGroupLeft(const tab_groups::TabGroupId& group) override {}
  void ShiftGroupRight(const tab_groups::TabGroupId& group) override {}
  const Browser* GetBrowser() const override;
  int GetInactiveTabWidth() const override;
  bool IsFrameCondensed() const override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool IsLockedForOnTask() override;

  // Sets OnTask locked for testing purposes. Only relevant for non-web browser
  // scenarios.
  void SetLockedForOnTask(bool locked) { on_task_locked_ = locked; }
#endif

  void SetTabColors(SkColor fg_color_active, SkColor fg_color_inactive) {
    tab_fg_color_active_ = fg_color_active;
    tab_fg_color_inactive_ = fg_color_inactive;
  }

  void SetInactiveTabWidth(int width) { inactive_tab_width_ = width; }

 private:
  raw_ptr<TabStripController> tab_strip_controller_;
  raw_ptr<TabContainer, DanglingUntriaged> tab_container_;
  ui::ListSelectionModel selection_model_;
  raw_ptr<Tab, DanglingUntriaged> active_tab_ = nullptr;
  bool paint_throbber_to_layer_ = true;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool on_task_locked_ = false;
#endif

  SkColor tab_fg_color_active_ = gfx::kPlaceholderColor;
  SkColor tab_fg_color_inactive_ = gfx::kPlaceholderColor;

  int inactive_tab_width_ = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_FAKE_TAB_SLOT_CONTROLLER_H_
