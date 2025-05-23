// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/dragging/dragging_tabs_session.h"

#include "base/metrics/histogram_functions.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/tabs/dragging/drag_session_data.h"
#include "chrome/browser/ui/views/tabs/tab_slot_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "components/viz/common/frame_timing_details.h"
#include "ui/compositor/compositor.h"
#include "ui/views/widget/widget.h"

namespace {
constexpr char kDragAmongTabsPresentationTimeHistogram[] =
    "Browser.TabDragging.DragAmongTabsPresentationTime";

int CalculateMouseOffset(const DragSessionData& drag_data_,
                         float offset_to_width_ratio_) {
  std::vector<TabSlotView*> tabs_to_source(drag_data_.attached_views());
  TabSlotView* source_view = drag_data_.source_view_drag_data()->attached_view;
  tabs_to_source.erase(
      tabs_to_source.begin() + drag_data_.source_view_index_ + 1,
      tabs_to_source.end());
  const int new_x =
      TabStrip::GetSizeNeededForViews(tabs_to_source) - source_view->width() +
      base::ClampRound(offset_to_width_ratio_ * source_view->width());

  return new_x;
}

}  // namespace

DraggingTabsSession::DraggingTabsSession(DragSessionData drag_data,
                                         TabDragContext* attached_context,
                                         float offset_to_width_ratio_,
                                         bool initial_move,
                                         gfx::Point start_point_in_screen)
    : drag_data_(drag_data),
      attached_context_(attached_context),
      mouse_offset_(CalculateMouseOffset(drag_data_, offset_to_width_ratio_)),
      initial_move_(initial_move),
      last_move_attached_context_loc_(
          views::View::ConvertPointFromScreen(attached_context,
                                              start_point_in_screen)
              .x()),
      last_point_in_screen_(start_point_in_screen) {
  if (base::FeatureList::IsEnabled(tabs::kScrollableTabStrip) &&
      base::FeatureList::IsEnabled(tabs::kScrollableTabStripWithDragging)) {
    const int drag_with_scroll_mode = base::GetFieldTrialParamByFeatureAsInt(
        tabs::kScrollableTabStripWithDragging,
        tabs::kTabScrollingWithDraggingModeName,
        static_cast<int>(
            TabStripScrollSession::ScrollWithDragStrategy::kConstantSpeed));

    switch (drag_with_scroll_mode) {
      case static_cast<int>(
          TabStripScrollSession::ScrollWithDragStrategy::kConstantSpeed):
        tab_strip_scroll_session_ =
            std::make_unique<TabStripScrollSessionWithTimer>(
                *this, TabStripScrollSessionWithTimer::ScrollSessionTimerType::
                           kConstantTimer);
        break;
      case static_cast<int>(
          TabStripScrollSession::ScrollWithDragStrategy::kVariableSpeed):
        tab_strip_scroll_session_ =
            std::make_unique<TabStripScrollSessionWithTimer>(
                *this, TabStripScrollSessionWithTimer::ScrollSessionTimerType::
                           kVariableTimer);
        break;
      default:
        NOTREACHED();
    }
  }

  MoveAttachedImpl(start_point_in_screen, true);
}

DraggingTabsSession::~DraggingTabsSession() = default;

void DraggingTabsSession::MoveAttached(gfx::Point point_in_screen) {
  MoveAttachedImpl(point_in_screen, false);
}

gfx::Rect DraggingTabsSession::GetEnclosingRectForDraggedTabs() {
  CHECK_GT(drag_data_.tab_drag_data_.size(), 0UL);

  const TabSlotView* const last_tab =
      drag_data_.tab_drag_data_.back().attached_view;
  const TabSlotView* const first_tab =
      drag_data_.tab_drag_data_.front().attached_view;

  DCHECK(attached_context_);
  DCHECK(first_tab->parent() == attached_context_);

  const gfx::Point right_point_of_last_tab = last_tab->bounds().bottom_right();
  const gfx::Point left_point_of_first_tab = first_tab->bounds().origin();

  return gfx::Rect(left_point_of_first_tab.x(), 0,
                   right_point_of_last_tab.x() - left_point_of_first_tab.x(),
                   0);
}

gfx::Point DraggingTabsSession::GetLastPointInScreen() {
  return last_point_in_screen_;
}

views::View* DraggingTabsSession::GetAttachedContext() {
  return attached_context_;
}

views::ScrollView* DraggingTabsSession::GetScrollView() {
  return attached_context_->GetScrollView();
}

void DraggingTabsSession::MoveAttachedImpl(gfx::Point point_in_screen,
                                           bool just_attached) {
  last_point_in_screen_ = point_in_screen;

  const gfx::Point dragged_view_point = GetAttachedDragPoint(point_in_screen);

  std::vector<TabSlotView*> views(drag_data_.tab_drag_data_.size());
  for (size_t i = 0; i < drag_data_.tab_drag_data_.size(); ++i) {
    views[i] = drag_data_.tab_drag_data_[i].attached_view.get();
  }

  bool did_layout = false;

  const gfx::Point point_in_attached_context =
      views::View::ConvertPointFromScreen(attached_context_, point_in_screen);

  const int to_index = attached_context_->GetInsertionIndexForDraggedBounds(
      GetDraggedViewTabStripBounds(dragged_view_point),
      drag_data_.attached_views(), drag_data_.num_dragging_tabs());

  constexpr int kHorizontalMoveThreshold = 16;  // DIPs.
  const int threshold = base::ClampRound(
      static_cast<double>(
          attached_context_->GetTabAt(to_index)->bounds().width()) /
      TabStyle::Get()->GetStandardWidth(/*is_split=*/false) *
      kHorizontalMoveThreshold);

  // Update the model, moving the WebContents from one index to another. Do this
  // only if we have moved a minimum distance since the last reorder (to prevent
  // jitter), or if this the first move and the tabs are not consecutive, or if
  // we have just attached to a new tabstrip and need to move to the correct
  // initial position.
  if (just_attached ||
      (abs(point_in_attached_context.x() - last_move_attached_context_loc_) >
       threshold) ||
      (initial_move_ && !AreTabsConsecutive())) {
    TabStripModel* attached_model = attached_context_->GetTabStripModel();

    content::WebContents* last_contents =
        drag_data_.tab_drag_data_.back().contents;
    const int index_of_last_item =
        attached_model->GetIndexOfWebContents(last_contents);
    if (initial_move_) {
      // TabDragContext determines if the tabs needs to be animated
      // based on model position. This means we need to invoke
      // LayoutDraggedTabsAt before changing the model.
      attached_context_->LayoutDraggedViewsAt(
          views, drag_data_.source_view_drag_data()->attached_view,
          dragged_view_point, initial_move_);
      did_layout = true;
    }

    // Only record the metric when the tab is moved to a different index.
    if (!just_attached && index_of_last_item != to_index) {
      attached_context_->GetWidget()
          ->GetCompositor()
          ->RequestSuccessfulPresentationTimeForNextFrame(base::BindOnce(
              [](base::TimeTicks now,
                 const viz::FrameTimingDetails& frame_timing_details) {
                base::TimeTicks presentation_timestamp =
                    frame_timing_details.presentation_feedback.timestamp;
                UmaHistogramTimes(kDragAmongTabsPresentationTimeHistogram,
                                  presentation_timestamp - now);
              },
              base::TimeTicks::Now()));
    }

    if (drag_data_.group_drag_data_.has_value()) {
      attached_model->MoveGroupTo(drag_data_.group_drag_data_.value().group,
                                  to_index);
    } else {
      attached_model->MoveSelectedTabsTo(
          to_index, CalculateGroupForDraggedTabs(to_index));
    }

    // Move may do nothing in certain situations (such as when dragging pinned
    // tabs). Make sure the tabstrip actually changed before updating
    // `last_move_attached_context_loc_`.
    if (index_of_last_item !=
        attached_model->GetIndexOfWebContents(last_contents)) {
      last_move_attached_context_loc_ = point_in_attached_context.x();
    }
  }

  if (tab_strip_scroll_session_) {
    tab_strip_scroll_session_->MaybeStart();
  }

  if (!did_layout) {
    attached_context_->LayoutDraggedViewsAt(
        views, drag_data_.source_view_drag_data()->attached_view,
        dragged_view_point, initial_move_);
  }

  // Snap the non-dragged tabs to their ideal bounds now, otherwise those tabs
  // will animate to those bounds after attach, which looks flickery/bad. See
  // https://crbug.com/1360330.
  if (just_attached && !initial_move_) {
    attached_context_->ForceLayout();
  }

  initial_move_ = false;
}

gfx::Rect DraggingTabsSession::GetDraggedViewTabStripBounds(
    gfx::Point tab_strip_point) const {
  // attached_view is null when inserting into a new context.
  if (drag_data_.source_view_drag_data()->attached_view) {
    std::vector<gfx::Rect> all_bounds =
        attached_context_->CalculateBoundsForDraggedViews(
            drag_data_.attached_views());
    int total_width = all_bounds.back().right() - all_bounds.front().x();
    return gfx::Rect(
        tab_strip_point.x(), tab_strip_point.y(), total_width,
        drag_data_.source_view_drag_data()->attached_view->height());
  }

  return gfx::Rect(tab_strip_point.x(), tab_strip_point.y(),
                   TabStyle::Get()->GetStandardWidth(/*is_split=*/false),
                   GetLayoutConstant(TAB_HEIGHT));
}

bool DraggingTabsSession::AreTabsConsecutive() const {
  for (size_t i = 1; i < drag_data_.tab_drag_data_.size(); ++i) {
    const std::optional<int> previous_source_index =
        drag_data_.tab_drag_data_[i - 1].source_model_index;
    const std::optional<int> source_index =
        drag_data_.tab_drag_data_[i].source_model_index;
    if (previous_source_index.has_value() && source_index.has_value() &&
        previous_source_index.value() + 1 != source_index.value()) {
      return false;
    }
  }
  return true;
}

std::optional<tab_groups::TabGroupId>
DraggingTabsSession::CalculateGroupForDraggedTabs(int to_index) {
  TabStripModel* attached_model = attached_context_->GetTabStripModel();

  // Get the proposed tabstrip model assuming the selection has taken place.
  std::pair<std::optional<int>, std::optional<int>> adjacent_indices =
      attached_model->GetAdjacentTabsAfterSelectedMove(
          base::PassKey<DraggingTabsSession>(), to_index);

  const ui::ListSelectionModel::SelectedIndices& selected =
      attached_model->selection_model().selected_indices();

  // Pinned tabs cannot be grouped, so we only change the group membership of
  // unpinned tabs.
  std::vector<int> selected_unpinned;
  for (size_t selected_index : selected) {
    if (!attached_model->IsTabPinned(selected_index)) {
      selected_unpinned.push_back(selected_index);
    }
  }

  if (selected_unpinned.empty()) {
    return std::nullopt;
  }

  std::optional<tab_groups::TabGroupId> left_group =
      adjacent_indices.first.has_value()
          ? attached_model->GetTabGroupForTab(adjacent_indices.first.value())
          : std::nullopt;
  std::optional<tab_groups::TabGroupId> right_group =
      adjacent_indices.second.has_value()
          ? attached_model->GetTabGroupForTab(adjacent_indices.second.value())
          : std::nullopt;
  std::optional<tab_groups::TabGroupId> current_group =
      attached_model->GetTabGroupForTab(selected_unpinned[0]);

  if (left_group == right_group) {
    return left_group;
  }

  // If the tabs on the left and right have different group memberships,
  // including if one is ungrouped or nonexistent, change the group of the
  // dragged tab based on whether it is "leaning" toward the left or the
  // right of the gap. If the tab is centered in the gap, make the tab
  // ungrouped.

  const Tab* left_most_selected_tab =
      attached_context_->GetTabAt(selected_unpinned[0]);

  const int buffer = left_most_selected_tab->width() / 4;

  // The tab's bounds are larger than what visually appears in order to include
  // space for the rounded feet. Adding {tab_left_inset} to the horizontal
  // bounds of the tab results in the x position that would be drawn when there
  // are no feet showing.
  const int tab_left_inset = TabStyle::Get()->GetTabOverlap() / 2;

  const auto tab_bounds_in_drag_context_coords = [this](int model_index) {
    const Tab* const tab = attached_context_->GetTabAt(model_index);
    return ToEnclosingRect(views::View::ConvertRectToTarget(
        tab->parent(), attached_context_, gfx::RectF(tab->bounds())));
  };

  // Use the left edge for a reliable fallback, e.g. if this is the leftmost
  // tab or there is a group header to the immediate left.
  int left_edge =
      adjacent_indices.first.has_value()
          ? tab_bounds_in_drag_context_coords(adjacent_indices.first.value())
                    .right() -
                tab_left_inset
          : tab_left_inset;

  // Extra polish: Prefer staying in an existing group, if any. This prevents
  // tabs at the edge of the group from flickering between grouped and
  // ungrouped. It also gives groups a slightly "sticky" feel while dragging.
  if (left_group.has_value() && left_group == current_group) {
    left_edge += buffer;
  }
  if (right_group.has_value() && right_group == current_group &&
      left_edge > tab_left_inset) {
    left_edge -= buffer;
  }

  const int left_most_selected_x_position =
      left_most_selected_tab->x() + tab_left_inset;

  if (left_group.has_value() &&
      !attached_model->IsGroupCollapsed(left_group.value())) {
    // Take the dragged tabs out of left_group if they are at the rightmost edge
    // of the tabstrip. This happens when the tabstrip is full and the dragged
    // tabs are as far right as they can go without being pulled out into a new
    // window. In this case, since the dragged tabs can't move further right in
    // the tabstrip, it will never go "beyond" the left_group and therefore
    // never leave it unless we add this check. See crbug.com/1134376.
    // TODO(crbug.com/40842551): Update this to work better with Tab Scrolling
    // once dragging near the end of the tabstrip is cleaner.
    if (tab_bounds_in_drag_context_coords(selected_unpinned.back()).right() >=
        attached_context_->TabDragAreaEndX()) {
      return std::nullopt;
    }

    if (left_most_selected_x_position <= left_edge - buffer) {
      return left_group;
    }
  }
  if ((left_most_selected_x_position >= left_edge + buffer) &&
      right_group.has_value() &&
      !attached_model->IsGroupCollapsed(right_group.value())) {
    return right_group;
  }
  return std::nullopt;
}

gfx::Point DraggingTabsSession::GetAttachedDragPoint(
    gfx::Point point_in_screen) {
  const gfx::Point tab_loc =
      views::View::ConvertPointFromScreen(attached_context_, point_in_screen);
  const int x =
      attached_context_->GetMirroredXInView(tab_loc.x()) - mouse_offset_;

  // If the width needed for the `attached_views_` is greater than what is
  // available in the tab drag area the attached drag point should simply be the
  // beginning of the tab strip. Once attached the `attached_views_` will simply
  // overflow as usual (see https://crbug.com/1250184).
  const int max_x = std::max(
      0, attached_context_->GetTabDragAreaWidth() -
             TabStrip::GetSizeNeededForViews(drag_data_.attached_views()));
  return gfx::Point(std::clamp(x, 0, max_x), 0);
}
