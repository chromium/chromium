// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_group_header.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_group_visual_data.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"
#include "chrome/browser/ui/views/tabs/tab_group_editor_bubble_view.h"
#include "chrome/browser/ui/views/tabs/tab_group_underline.h"
#include "chrome/browser/ui/views/tabs/tab_slot_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_layout.h"
#include "chrome/browser/ui/views/tabs/tab_strip_types.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

TabGroupHeader::TabGroupHeader(TabStrip* tab_strip, TabGroupId group)
    : tab_strip_(tab_strip) {
  DCHECK(tab_strip);

  set_group(group);

  // The size and color of the chip are set in VisualsChanged().
  title_chip_ = AddChildView(std::make_unique<views::View>());

  // The text and color of the title are set in VisualsChanged().
  title_ = title_chip_->AddChildView(std::make_unique<views::Label>());
  title_->SetCollapseWhenHidden(true);
  title_->SetAutoColorReadabilityEnabled(false);
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_->SetElideBehavior(gfx::FADE_TAIL);

  VisualsChanged();
}

bool TabGroupHeader::OnMousePressed(const ui::MouseEvent& event) {
  // Ignore the click if the editor is already open. Do this so clicking
  // on us again doesn't re-trigger the editor.
  //
  // Though the bubble is deactivated before we receive a mouse event,
  // the actual widget destruction happens in a posted task. That task
  // gets run after we receive the mouse event. If this sounds brittle,
  // that's because it is!
  if (editor_bubble_tracker_.is_open())
    return false;

  tab_strip_->MaybeStartDrag(this, event, tab_strip_->GetSelectionModel());

  return true;
}

bool TabGroupHeader::OnMouseDragged(const ui::MouseEvent& event) {
  tab_strip_->ContinueDrag(this, event);
  return true;
}

void TabGroupHeader::OnMouseReleased(const ui::MouseEvent& event) {
  if (!dragging()) {
    editor_bubble_tracker_.Opened(
        TabGroupEditorBubbleView::Show(this, tab_strip_, group().value()));
  }
  tab_strip_->EndDrag(END_DRAG_COMPLETE);
}

void TabGroupHeader::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN: {
      if (!editor_bubble_tracker_.is_open()) {
        tab_strip_->MaybeStartDrag(this, *event,
                                   tab_strip_->GetSelectionModel());
      }
      break;
    }

    case ui::ET_GESTURE_SCROLL_UPDATE: {
      tab_strip_->ContinueDrag(this, *event);
      break;
    }

    case ui::ET_GESTURE_END: {
      if (!dragging()) {
        editor_bubble_tracker_.Opened(
            TabGroupEditorBubbleView::Show(this, tab_strip_, group().value()));
      }
      tab_strip_->EndDrag(END_DRAG_COMPLETE);
      break;
    }

    default:
      break;
  }
  event->SetHandled();
}

TabSlotView::ViewType TabGroupHeader::GetTabSlotViewType() const {
  return TabSlotView::ViewType::kTabGroupHeader;
}

TabSizeInfo TabGroupHeader::GetTabSizeInfo() const {
  TabSizeInfo size_info;
  // Group headers have a fixed width based on |title_|'s width.
  const int width = CalculateWidth();
  size_info.pinned_tab_width = width;
  size_info.min_active_width = width;
  size_info.min_inactive_width = width;
  size_info.standard_width = width;
  return size_info;
}

int TabGroupHeader::CalculateWidth() const {
  // We don't want tabs to visually overlap group headers, so we add that space
  // to the width to compensate. We don't want to actually remove the overlap
  // during layout however; that would cause an the margin to be visually uneven
  // when the header is in the first slot and thus wouldn't overlap anything to
  // the left.
  const int overlap_margin = TabStyle::GetTabOverlap() * 2;

  // The empty and non-empty chips have different sizes and corner radii, but
  // both should look nestled against the group stroke of the tab to the right.
  // This requires a +/- 2px adjustment to the width, which causes the tab to
  // the right to be positioned in the right spot.
  const TabGroupVisualData* data =
      tab_strip_->controller()->GetVisualDataForGroup(group().value());
  const int right_adjust = data->title().empty() ? 2 : -2;

  return overlap_margin + title_chip_->width() + right_adjust;
}

void TabGroupHeader::VisualsChanged() {
  const TabGroupVisualData* data =
      tab_strip_->controller()->GetVisualDataForGroup(group().value());

  if (data->title().empty()) {
    // If the title is empty, the chip is just a circle.
    title_->SetVisible(false);

    constexpr int kEmptyChipSize = 14;
    const int y = (GetLayoutConstant(TAB_HEIGHT) - kEmptyChipSize) / 2;

    title_chip_->SetBounds(TabGroupUnderline::GetStrokeInset(), y,
                           kEmptyChipSize, kEmptyChipSize);
    title_chip_->SetBackground(
        views::CreateRoundedRectBackground(data->color(), kEmptyChipSize / 2));
  } else {
    // If the title is set, the chip is a rounded rect that matches the active
    // tab shape, particularly the tab's corner radius.
    title_->SetVisible(true);
    title_->SetEnabledColor(
        color_utils::GetColorWithMaxContrast(data->color()));
    title_->SetText(data->title());

    // Set the radius such that the chip nestles snugly against the tab corner
    // radius, taking into account the group underline stroke.
    const int corner_radius =
        TabStyle::GetCornerRadius() - TabGroupUnderline::kStrokeThickness;

    // Clamp the width to a maximum of half the standard tab width (not counting
    // overlap).
    const int max_width =
        (TabStyle::GetStandardWidth() - TabStyle::GetTabOverlap()) / 2;
    const int text_width =
        std::min(title_->GetPreferredSize().width(), max_width);
    const int text_height = title_->GetPreferredSize().height();
    const int text_vertical_inset = 1;
    const int text_horizontal_inset = corner_radius + text_vertical_inset;

    const int y =
        (GetLayoutConstant(TAB_HEIGHT) - text_height) / 2 - text_vertical_inset;

    title_chip_->SetBounds(TabGroupUnderline::GetStrokeInset(), y,
                           text_width + 2 * text_horizontal_inset,
                           text_height + 2 * text_vertical_inset);
    title_chip_->SetBackground(
        views::CreateRoundedRectBackground(data->color(), corner_radius));

    title_->SetBounds(text_horizontal_inset, text_vertical_inset, text_width,
                      text_height);
  }
}

void TabGroupHeader::RemoveObserverFromWidget(views::Widget* widget) {
  widget->RemoveObserver(&editor_bubble_tracker_);
}

void TabGroupHeader::EditorBubbleTracker::Opened(views::Widget* bubble_widget) {
  DCHECK(bubble_widget);
  DCHECK(!is_open_);
  is_open_ = true;
  bubble_widget->AddObserver(this);
}

void TabGroupHeader::EditorBubbleTracker::OnWidgetDestroyed(
    views::Widget* widget) {
  is_open_ = false;
}
