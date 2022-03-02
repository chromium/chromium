// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_FAKE_TAB_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_FAKE_TAB_CONTROLLER_H_

#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/gfx/color_palette.h"

class FakeTabController : public TabController {
 public:
  FakeTabController() = default;
  FakeTabController(const FakeTabController&) = delete;
  FakeTabController& operator=(const FakeTabController&) = delete;
  ~FakeTabController() override = default;

  void set_active_tab(bool value) { active_tab_ = value; }
  void set_paint_throbber_to_layer(bool value) {
    paint_throbber_to_layer_ = value;
  }

  const ui::ListSelectionModel& GetSelectionModel() const override;
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
  void ShowContextMenuForTab(Tab* tab,
                             const gfx::Point& p,
                             ui::MenuSourceType source_type) override {}
  bool IsActiveTab(const Tab* tab) const override;
  bool IsTabSelected(const Tab* tab) const override;
  bool IsTabPinned(const Tab* tab) const override;
  bool IsTabFirst(const Tab* tab) const override;
  bool IsFocusInTabs() const override;
  void MaybeStartDrag(
      TabSlotView* source,
      const ui::LocatedEvent& event,
      const ui::ListSelectionModel& original_selection) override {}
  void ContinueDrag(views::View* view, const ui::LocatedEvent& event) override {
  }
  bool EndDrag(EndDragReason reason) override;
  Tab* GetTabAt(const gfx::Point& point) override;
  const Tab* GetAdjacentTab(const Tab* tab, int offset) override;
  void OnMouseEventInTab(views::View* source,
                         const ui::MouseEvent& event) override {}
  void UpdateHoverCard(Tab* tab, HoverCardUpdateType update_type) override {}
  bool ShowDomainInHoverCards() const override;
  bool HoverCardIsShowingForTab(Tab* tab) override;
  int GetBackgroundOffset() const override;
  bool ShouldPaintAsActiveFrame() const override;
  int GetStrokeThickness() const override;
  bool CanPaintThrobberToLayer() const override;
  bool HasVisibleBackgroundTabShapes() const override;
  SkColor GetTabSeparatorColor() const override;
  SkColor GetTabBackgroundColor(
      TabActive active,
      BrowserFrameActiveState active_state) const override;
  SkColor GetTabForegroundColor(TabActive active,
                                SkColor background_color) const override;
  absl::optional<int> GetCustomBackgroundId(
      BrowserFrameActiveState active_state) const override;
  gfx::Rect GetTabAnimationTargetBounds(const Tab* tab) override;
  std::u16string GetAccessibleTabName(const Tab* tab) const override;
  float GetHoverOpacityForTab(float range_parameter) const override;
  float GetHoverOpacityForRadialHighlight() const override;

  std::u16string GetGroupTitle(
      const tab_groups::TabGroupId& group_id) const override;

  tab_groups::TabGroupColorId GetGroupColorId(
      const tab_groups::TabGroupId& group_id) const override;

  SkColor GetPaintedGroupColor(
      const tab_groups::TabGroupColorId& color_id) const override;

  void SetTabColors(SkColor bg_color_active,
                    SkColor fg_color_active,
                    SkColor bg_color_inactive,
                    SkColor fg_color_inactive) {
    tab_bg_color_active_ = bg_color_active;
    tab_fg_color_active_ = fg_color_active;
    tab_bg_color_inactive_ = bg_color_inactive;
    tab_fg_color_inactive_ = fg_color_inactive;
  }

 private:
  ui::ListSelectionModel selection_model_;
  bool active_tab_ = false;
  bool paint_throbber_to_layer_ = true;

  SkColor tab_bg_color_active_ = gfx::kPlaceholderColor;
  SkColor tab_fg_color_active_ = gfx::kPlaceholderColor;
  SkColor tab_bg_color_inactive_ = gfx::kPlaceholderColor;
  SkColor tab_fg_color_inactive_ = gfx::kPlaceholderColor;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_FAKE_TAB_CONTROLLER_H_
