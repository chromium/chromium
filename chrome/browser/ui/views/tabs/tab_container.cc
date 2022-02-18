// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_container.h"

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/rect_based_targeting_utils.h"
#include "ui/views/view_utils.h"

TabContainer::TabContainer(TabStripController* controller)
    : controller_(controller),
      layout_helper_(std::make_unique<TabStripLayoutHelper>(
          controller,
          base::BindRepeating(&TabContainer::tabs_view_model,
                              base::Unretained(this)))) {
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
}

TabContainer::~TabContainer() {
  RemoveAllChildViews();
}

Tab* TabContainer::AddTab(std::unique_ptr<Tab> tab,
                          int model_index,
                          TabPinned pinned) {
  absl::optional<tab_groups::TabGroupId> group = tab->group();
  Tab* tab_ptr = AddChildViewAt(
      std::move(tab), GetViewInsertionIndex(group, absl::nullopt, model_index));
  tabs_view_model_.Add(tab_ptr, model_index);
  layout_helper_->InsertTabAt(model_index, tab_ptr, pinned);
  return tab_ptr;
}

void TabContainer::MoveTab(Tab* tab, int from_model_index, int to_model_index) {
  ReorderChildView(tab, GetViewInsertionIndex(tab->group(), from_model_index,
                                              to_model_index));
  tabs_view_model_.Move(from_model_index, to_model_index);
  layout_helper_->MoveTab(tab->group(), from_model_index, to_model_index);
}

void TabContainer::RemoveTabFromViewModel(int index) {
  Tab* tab = GetTabAtModelIndex(index);
  tabs_view_model_.Remove(index);
  layout_helper_->RemoveTabAt(index, tab);
}

void TabContainer::MoveGroupHeader(TabGroupHeader* group_header,
                                   int first_tab_model_index) {
  const int header_index = GetIndexOf(group_header);
  const int first_tab_view_index =
      GetViewIndexForModelIndex(first_tab_model_index);

  // The header should be just before the first tab. If it isn't, reorder the
  // header such that it is. Note that the index to reorder to is different
  // depending on whether the header is before or after the tab, since the
  // header itself occupies an index.
  if (header_index < first_tab_view_index - 1)
    ReorderChildView(group_header, first_tab_view_index - 1);
  if (header_index > first_tab_view_index - 1)
    ReorderChildView(group_header, first_tab_view_index);
}

int TabContainer::GetModelIndexOf(const TabSlotView* slot_view) {
  return tabs_view_model_.GetIndexOfView(slot_view);
}

Tab* TabContainer::GetTabAtModelIndex(int index) const {
  return tabs_view_model_.view_at(index);
}

int TabContainer::GetTabCount() const {
  return tabs_view_model_.view_size();
}

void TabContainer::UpdateAccessibleTabIndices() {
  const int num_tabs = GetTabCount();
  for (int i = 0; i < num_tabs; ++i)
    GetTabAtModelIndex(i)->GetViewAccessibility().OverridePosInSet(i + 1,
                                                                   num_tabs);
}

void TabContainer::HandleLongTap(ui::GestureEvent* event) {
  event->target()->ConvertEventToTarget(this, event);
  gfx::Point local_point = event->location();
  Tab* tab = FindTabHitByPoint(local_point);
  if (tab) {
    ConvertPointToScreen(this, &local_point);
    controller_->ShowContextMenuForTab(tab, local_point, ui::MENU_SOURCE_TOUCH);
  }
}

bool TabContainer::IsRectInWindowCaption(const gfx::Rect& rect) {
  // If there is no control at this location, the hit is in the caption area.
  const views::View* v = GetEventHandlerForRect(rect);
  if (v == this)
    return true;

  // When the window has a top drag handle, a thin strip at the top of inactive
  // tabs and the new tab button is treated as part of the window drag handle,
  // to increase draggability.  This region starts 1 DIP above the top of the
  // separator.
  const int drag_handle_extension = TabStyle::GetDragHandleExtension(height());

  // Disable drag handle extension when tab shapes are visible.
  bool extend_drag_handle = !controller_->IsFrameCondensed() &&
                            !controller_->EverHasVisibleBackgroundTabShapes();

  // A hit on the tab is not in the caption unless it is in the thin strip
  // mentioned above.
  const int tab_index = tabs_view_model_.GetIndexOfView(v);
  if (IsValidModelIndex(tab_index)) {
    Tab* tab = GetTabAtModelIndex(tab_index);
    gfx::Rect tab_drag_handle = tab->GetMirroredBounds();
    tab_drag_handle.set_height(drag_handle_extension);
    return extend_drag_handle && !tab->IsActive() &&
           tab_drag_handle.Intersects(rect);
  }

  // |v| is some other view (e.g. a close button in a tab) and therefore |rect|
  // is in client area.
  return false;
}

gfx::Size TabContainer::GetMinimumSize() const {
  int minimum_width = layout_helper_->CalculateMinimumWidth();

  return gfx::Size(minimum_width, GetLayoutConstant(TAB_HEIGHT));
}

views::View* TabContainer::GetTooltipHandlerForPoint(
    const gfx::Point& point_in_tab_container_coords) {
  if (!HitTestPoint(point_in_tab_container_coords))
    return nullptr;

  // Return any view that isn't a Tab or this TabContainer immediately. We don't
  // want to interfere.
  views::View* v =
      View::GetTooltipHandlerForPoint(point_in_tab_container_coords);
  if (v && v != this && !views::IsViewClass<Tab>(v))
    return v;

  views::View* tab = FindTabHitByPoint(point_in_tab_container_coords);
  if (tab)
    return tab;

  return this;
}

views::View* TabContainer::TargetForRect(views::View* root,
                                         const gfx::Rect& rect) {
  CHECK_EQ(root, this);

  if (!views::UsePointBasedTargeting(rect))
    return views::ViewTargeterDelegate::TargetForRect(root, rect);
  const gfx::Point point(rect.CenterPoint());

  // Return any view that isn't a Tab or this TabStrip immediately. We don't
  // want to interfere.
  views::View* v = views::ViewTargeterDelegate::TargetForRect(root, rect);
  if (v && v != this && !views::IsViewClass<Tab>(v))
    return v;

  views::View* tab = FindTabHitByPoint(point);
  if (tab)
    return tab;

  return this;
}

int TabContainer::GetViewInsertionIndex(
    absl::optional<tab_groups::TabGroupId> group,
    absl::optional<int> from_model_index,
    int to_model_index) const {
  // -1 is treated a sentinel value to indicate a tab is newly added to the
  // beginning of the tab strip.
  if (to_model_index < 0)
    return 0;

  // If to_model_index is beyond the end of the tab strip, then the tab is
  // newly added to the end of the tab strip. In that case we can just return
  // one beyond the view index of the last existing tab.
  if (to_model_index >= GetTabCount())
    return (GetTabCount() ? GetViewIndexForModelIndex(GetTabCount() - 1) + 1
                          : 0);

  // If there is no from_model_index, then the tab is newly added in the
  // middle of the tab strip. In that case we treat it as coming from the end
  // of the tab strip, since new views are ordered at the end by default.
  if (!from_model_index.has_value())
    from_model_index = GetTabCount();

  DCHECK_NE(to_model_index, from_model_index.value());

  // Since we don't have an absolute mapping from model index to view index,
  // we anchor on the last known view index at the given to_model_index.
  Tab* other_tab = GetTabAtModelIndex(to_model_index);
  int other_view_index = GetViewIndexForModelIndex(to_model_index);

  if (other_view_index <= 0)
    return 0;

  // When moving to the right, just use the anchor index because the tab will
  // replace that position in both the model and the view. This happens
  // because the tab itself occupies a lower index that the other tabs will
  // shift into.
  if (to_model_index > from_model_index.value())
    return other_view_index;

  // When moving to the left, the tab may end up on either the left or right
  // side of a group header, depending on if it's in that group. This affects
  // its view index but not its model index, so we adjust the former only.
  if (other_tab->group().has_value() && other_tab->group() != group)
    return other_view_index - 1;

  return other_view_index;
}

int TabContainer::GetViewIndexForModelIndex(int model_index) const {
  return GetIndexOf(GetTabAtModelIndex(model_index));
}

bool TabContainer::IsPointInTab(
    Tab* tab,
    const gfx::Point& point_in_tab_container_coords) {
  if (!tab->GetVisible())
    return false;
  gfx::Point point_in_tab_coords(point_in_tab_container_coords);
  View::ConvertPointToTarget(this, tab, &point_in_tab_coords);
  return tab->HitTestPoint(point_in_tab_coords);
}

Tab* TabContainer::FindTabHitByPoint(
    const gfx::Point& point_in_tab_container_coords) {
  // Check all tabs, even closing tabs. Mouse events need to reach closing tabs
  // for users to be able to rapidly middle-click close several tabs.
  std::vector<Tab*> all_tabs = layout_helper_->GetTabs();

  // The display order doesn't necessarily match the child order, so we iterate
  // in display order.
  for (size_t i = 0; i < all_tabs.size(); ++i) {
    // If we don't first exclude points outside the current tab, the code below
    // will return the wrong tab if the next tab is selected, the following tab
    // is active, and |point_in_tab_container_coords| is in the overlap region
    // between the two.
    Tab* tab = all_tabs[i];
    if (!IsPointInTab(tab, point_in_tab_container_coords))
      continue;

    // Selected tabs render atop unselected ones, and active tabs render atop
    // everything.  Check whether the next tab renders atop this one and
    // |point_in_tab_container_coords| is in the overlap region.
    Tab* next_tab = i < (all_tabs.size() - 1) ? all_tabs[i + 1] : nullptr;
    if (next_tab &&
        (next_tab->IsActive() ||
         (next_tab->IsSelected() && !tab->IsSelected())) &&
        IsPointInTab(next_tab, point_in_tab_container_coords))
      return next_tab;

    // This is the topmost tab for this point.
    return tab;
  }

  return nullptr;
}

bool TabContainer::IsValidModelIndex(int model_index) const {
  return controller_->IsValidIndex(model_index);
}

BEGIN_METADATA(TabContainer, views::View)
END_METADATA
