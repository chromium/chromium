// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_header_view.h"

#include <numeric>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/grit/generated_resources.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {
constexpr int kGroupHeaderCornerRadius = 8;
constexpr int kGroupHeaderHorizontalInset = 8;
constexpr int kIconSize = 16;

class VerticalTabGroupHeaderLabel : public views::Label {
  METADATA_HEADER(VerticalTabGroupHeaderLabel, views::Label)
 public:
  VerticalTabGroupHeaderLabel() {
    SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
    SetElideBehavior(gfx::FADE_TAIL);
    SetAutoColorReadabilityEnabled(false);
  }
};

BEGIN_METADATA(VerticalTabGroupHeaderLabel)
END_METADATA
}  // namespace

VerticalTabGroupHeaderView::VerticalTabGroupHeaderView(
    const tab_groups::TabGroupVisualData* tab_group_visual_data,
    base::RepeatingCallback<void(ToggleTabGroupCollapsedStateOrigin)>
        toggle_collapsed_state_callback)
    : group_header_label_(
          AddChildView(std::make_unique<VerticalTabGroupHeaderLabel>())),
      collapse_icon_(AddChildView(std::make_unique<views::ImageView>())),
      toggle_collapsed_state_callback_(
          std::move(toggle_collapsed_state_callback)) {
  SetProperty(views::kElementIdentifierKey, kTabGroupHeaderElementId);

  SetInteriorMargin(gfx::Insets::VH(0, kGroupHeaderHorizontalInset));
  group_header_label_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(
          views::MinimumFlexSizeRule::kScaleToMinimumSnapToZero,
          views::MaximumFlexSizeRule::kUnbounded));

  OnDataChanged(tab_group_visual_data);
}

VerticalTabGroupHeaderView::~VerticalTabGroupHeaderView() = default;

bool VerticalTabGroupHeaderView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_SPACE ||
      event.key_code() == ui::VKEY_RETURN) {
    toggle_collapsed_state_callback_.Run(
        ToggleTabGroupCollapsedStateOrigin::kKeyboard);
    return true;
  }
  return false;
}

bool VerticalTabGroupHeaderView::OnMousePressed(const ui::MouseEvent& event) {
  // Return true so that we receive subsequent MouseRelease event.
  return true;
}

void VerticalTabGroupHeaderView::OnMouseReleased(const ui::MouseEvent& event) {
  bool toggle_collapse =
      base::FeatureList::IsEnabled(tab_groups::kLeftClickOpensTabGroupBubble)
          ? event.IsRightMouseButton()
          : event.IsLeftMouseButton();
  if (toggle_collapse) {
    toggle_collapsed_state_callback_.Run(
        ToggleTabGroupCollapsedStateOrigin::kMouse);
  }
}

void VerticalTabGroupHeaderView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::EventType::kGestureTap) {
    toggle_collapsed_state_callback_.Run(
        ToggleTabGroupCollapsedStateOrigin::kGesture);
  }
  event->SetHandled();
}

void VerticalTabGroupHeaderView::OnDataChanged(
    const tab_groups::TabGroupVisualData* tab_group_visual_data) {
  group_header_label_->SetText(tab_group_visual_data->title());
  if (GetColorProvider()) {
    SkColor background_color = GetColorProvider()->GetColor(
        GetTabGroupTabStripColorId(tab_group_visual_data->color(),
                                   GetWidget()->ShouldPaintAsActive()));
    SkColor forground_color =
        color_utils::GetColorWithMaxContrast(background_color);
    group_header_label_->SetEnabledColor(forground_color);
    collapse_icon_->SetImage(
        ui::ImageModel::FromVectorIcon(tab_group_visual_data->is_collapsed()
                                           ? kKeyboardArrowDownChromeRefreshIcon
                                           : kKeyboardArrowUpChromeRefreshIcon,
                                       forground_color, kIconSize));
    SetBackground(views::CreateRoundedRectBackground(background_color,
                                                     kGroupHeaderCornerRadius));
  }
}

BEGIN_METADATA(VerticalTabGroupHeaderView)
END_METADATA
