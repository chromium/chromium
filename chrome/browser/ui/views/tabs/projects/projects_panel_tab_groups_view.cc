// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_tab_groups_view.h"

#include <algorithm>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/pickle.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_drag_data.h"
#include "chrome/browser/ui/views/tabs/projects/layout_constants.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_controller.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_no_tab_groups_view.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_tab_groups_item_view.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_utils.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/button_drag_utils.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace {
constexpr gfx::Insets kNoTabsInteriorMargins = gfx::Insets::TLBR(0, 8, 8, 8);

// Whether animations should be disabled.
static bool disable_animations_for_testing_ = false;

}  // namespace

ProjectsPanelTabGroupsView::ProjectsPanelTabGroupsView(
    actions::ActionItem* root_action_item,
    views::ActionViewController* action_view_controller,
    ProjectsPanelTabGroupsItemView::TabGroupPressedCallback
        tab_group_button_callback,
    ProjectsPanelTabGroupsItemView::MoreButtonPressedCallback
        more_button_callback,
    TabGroupMovedCallback tab_group_moved_callback,
    DragUpdatedCallback drag_updated_callback,
    DragExitedCallback drag_exited_callback)
    : tab_group_button_callback_(std::move(tab_group_button_callback)),
      more_button_callback_(std::move(more_button_callback)),
      tab_group_moved_callback_(std::move(tab_group_moved_callback)),
      drag_updated_callback_(std::move(drag_updated_callback)),
      drag_exited_callback_(std::move(drag_exited_callback)) {
  layout_ = SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout_->SetOrientation(views::LayoutOrientation::kVertical);

  SetProperty(views::kElementIdentifierKey,
              kProjectsPanelTabGroupsViewElementId);
}

ProjectsPanelTabGroupsView::~ProjectsPanelTabGroupsView() = default;

void ProjectsPanelTabGroupsView::SetTabGroups(
    const std::vector<tab_groups::SavedTabGroup>& tab_groups) {
  // Remove the no tab groups view if it was showing.
  if (no_tab_groups_view_) {
    RemoveChildViewT(std::exchange(no_tab_groups_view_, nullptr));
  }

  // Remove all item views.
  for (auto& item_view : item_views_) {
    RemoveChildViewT(std::exchange(item_view, nullptr));
  }
  item_views_.clear();

  if (tab_groups.empty()) {
    no_tab_groups_view_ =
        AddChildView(std::make_unique<ProjectsPanelNoTabGroupsView>());
    no_tab_groups_view_->SetProperty(views::kMarginsKey,
                                     kNoTabsInteriorMargins);
  } else {
    for (const auto& group : tab_groups) {
      auto* item =
          AddChildView(std::make_unique<ProjectsPanelTabGroupsItemView>(
              group, tab_group_button_callback_, more_button_callback_));
      item->set_drag_controller(this);
      if (disable_animations_for_testing_) {
        item->disable_animations_for_testing();  // IN-TEST
      }
      item_views_.push_back(item);
    }
  }
}

void ProjectsPanelTabGroupsView::SetInsideBorderInsets(
    const gfx::Insets& insets) {
  layout_->set_inside_border_insets(insets);
}

bool ProjectsPanelTabGroupsView::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  *formats = ui::OSExchangeData::PICKLED_DATA;
  format_types->insert(tab_groups::SavedTabGroupDragData::GetFormatType());
  return true;
}

bool ProjectsPanelTabGroupsView::AreDropTypesRequired() {
  return true;
}

bool ProjectsPanelTabGroupsView::CanDrop(const ui::OSExchangeData& data) {
  return data.HasCustomFormat(
      tab_groups::SavedTabGroupDragData::GetFormatType());
}

void ProjectsPanelTabGroupsView::OnDragEntered(
    const ui::DropTargetEvent& event) {
  if (!drop_info_) {
    drop_info_ = std::make_unique<DropInfo>();
  }
}

int ProjectsPanelTabGroupsView::OnDragUpdated(
    const ui::DropTargetEvent& event) {
  drag_updated_callback_.Run(event.location());

  if (!drop_info_) {
    return static_cast<int>(ui::mojom::DragOperation::kNone);
  }

  drop_info_->x = event.x();
  drop_info_->y = event.y();

  DropLocation location;
  CalculateDropLocation(event, &location);

  if (drop_info_->location.Equals(location)) {
    return static_cast<int>(drop_info_->location.operation);
  }

  drop_info_->location = location;
  SchedulePaint();

  return static_cast<int>(drop_info_->location.operation);
}

void ProjectsPanelTabGroupsView::OnDragExited() {
  drag_exited_callback_.Run();
  drop_info_.reset();
  SchedulePaint();
}

views::View::DropCallback ProjectsPanelTabGroupsView::GetDropCallback(
    const ui::DropTargetEvent& event) {
  if (!drop_info_ ||
      drop_info_->location.operation == ui::mojom::DragOperation::kNone) {
    return base::NullCallback();
  }

  const size_t drop_index = *drop_info_->location.index;

  drop_info_.reset();
  SchedulePaint();

  return base::BindOnce(&ProjectsPanelTabGroupsView::OnDrop,
                        weak_ptr_factory_.GetWeakPtr(), drop_index);
}

void ProjectsPanelTabGroupsView::OnDrop(
    size_t drop_index,
    const ui::DropTargetEvent& event,
    ui::mojom::DragOperation& output_drag_op,
    std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner) {
  base::Uuid guid;
  std::optional<tab_groups::SavedTabGroupDragData> drag_data =
      tab_groups::SavedTabGroupDragData::ReadFromOSExchangeData(&event.data());

  if (drag_data.has_value()) {
    guid = drag_data->guid();
  }

  if (!guid.is_valid()) {
    output_drag_op = ui::mojom::DragOperation::kNone;
    return;
  }

  // Find the current index of the group item.
  std::optional<size_t> current_index;
  for (size_t i = 0; i < item_views_.size(); ++i) {
    if (item_views_[i]->guid() == guid) {
      current_index = i;
      break;
    }
  }

  if (!current_index.has_value()) {
    output_drag_op = ui::mojom::DragOperation::kNone;
    return;
  }

  int target_index = static_cast<int>(drop_index);
  // If the drop index is greater than the current index, then the target drop
  // index will be one greater than the group's actual new position.
  if (current_index.value() < drop_index) {
    target_index--;
  }

  tab_group_moved_callback_.Run(guid, target_index);
  output_drag_op = ui::mojom::DragOperation::kMove;
}

void ProjectsPanelTabGroupsView::PaintChildren(
    const views::PaintInfo& paint_info) {
  views::View::PaintChildren(paint_info);

  auto indicator_bounds = GetDropIndicatorBounds();
  if (indicator_bounds.has_value()) {
    ui::PaintRecorder recorder(paint_info.context(), size());
    recorder.canvas()->FillRect(*indicator_bounds,
                                GetColorProvider()->GetColor(
                                    kColorProjectsPanelTabGroupsDropIndicator));
  }
}

void ProjectsPanelTabGroupsView::WriteDragDataForView(
    views::View* sender,
    const gfx::Point& press_pt,
    ui::OSExchangeData* data) {
  auto* item = views::AsViewClass<ProjectsPanelTabGroupsItemView>(sender);
  gfx::ImageSkia drag_image = item->GetDragImage();
  if (!drag_image.isNull() && !drag_image.size().IsEmpty()) {
    data->provider().SetDragImage(drag_image, press_pt.OffsetFromOrigin());
  }
  item->SetIsDragging(true);

  base::Pickle data_pickle;
  data_pickle.WriteString(item->guid().AsLowercaseString());
  data->SetPickledData(tab_groups::SavedTabGroupDragData::GetFormatType(),
                       data_pickle);
}

int ProjectsPanelTabGroupsView::GetDragOperationsForView(views::View* sender,
                                                         const gfx::Point& p) {
  return ui::DragDropTypes::DRAG_MOVE;
}

bool ProjectsPanelTabGroupsView::CanStartDragForView(views::View* sender,
                                                     const gfx::Point& press_pt,
                                                     const gfx::Point& p) {
  return true;
}

std::optional<gfx::Rect>
ProjectsPanelTabGroupsView::GetDropIndicatorBoundsForTesting() const {
  return GetDropIndicatorBounds();
}

// static
void ProjectsPanelTabGroupsView::disable_animations_for_testing() {
  disable_animations_for_testing_ = true;
}

void ProjectsPanelTabGroupsView::CalculateDropLocation(
    const ui::DropTargetEvent& event,
    DropLocation* location) {
  *location = DropLocation();
  location->operation = ui::mojom::DragOperation::kMove;

  if (item_views_.empty()) {
    location->index = 0;
    return;
  }

  // Iterate through all item views to find the drop index.
  size_t index = 0;
  std::optional<size_t> dragging_index;
  for (auto child : item_views_) {
    if (child->is_dragging()) {
      dragging_index = index;
    }

    int child_y = child->y();
    int child_h = child->height();

    if (!location->index.has_value() && event.y() < child_y + child_h / 2) {
      location->index = index;
    }
    index++;
  }

  if (!location->index.has_value()) {
    location->index = index;
  }

  if (dragging_index.has_value() &&
      (location->index == dragging_index.value() ||
       location->index == dragging_index.value() + 1)) {
    location->operation = ui::mojom::DragOperation::kNone;
  }
}

std::optional<gfx::Rect> ProjectsPanelTabGroupsView::GetDropIndicatorBounds()
    const {
  if (drop_info_ && drop_info_->location.index.has_value() &&
      drop_info_->location.operation != ui::mojom::DragOperation::kNone) {
    // Draw an indicator in the list where the dropped item will move to.
    int x = 0;
    // The inside border insets are not accounted for in the width, so we must
    // factor them in here.
    int w = width() - layout_->inside_border_insets().width();
    int y = 0;

    size_t index = *drop_info_->location.index;

    if (item_views_.empty()) {
      y = 0;
    } else if (index < item_views_.size()) {
      y = item_views_[index]->y();
    } else {
      y = item_views_.back()->bounds().bottom();
    }

    constexpr int kDropIndicatorHeight = 2;
    // Clamp y so the indicator is fully visible within the view.
    y = std::clamp(y, kDropIndicatorHeight / 2,
                   height() - kDropIndicatorHeight / 2);
    return gfx::Rect(x, y - kDropIndicatorHeight / 2, w, kDropIndicatorHeight);
  }

  return std::nullopt;
}

BEGIN_METADATA(ProjectsPanelTabGroupsView)
END_METADATA
