// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_segmented_control.h"

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace {

constexpr int kBorderThickness = 1;
constexpr auto kInsideBorderInsets = gfx::Insets(1);

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(VerticalTabStripSegmentedControl,
                                      kSegmentedControlElementId);

VerticalTabStripSegmentedControl::VerticalTabStripSegmentedControl(
    BrowserWindowInterface* browser)
    : browser_(browser) {
  CHECK(browser_);
  SetProperty(views::kElementIdentifierKey, kSegmentedControlElementId);

  const int button_size = GetLayoutConstant(
      LayoutConstant::kVerticalTabStripTopContainerButtonSize);
  const int total_height =
      button_size + (kBorderThickness + kInsideBorderInsets.top()) * 2;
  const float corner_radius = total_height / 2.0f;

  SetBackground(views::CreatePillBackground(ui::kColorSysSurface));
  SetBorder(views::CreateRoundedRectBorder(kBorderThickness, corner_radius,
                                           ui::kColorSysOutline));

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kInsideBorderInsets, 0));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  auto create_button = [&](Segment segment, int tooltip_id,
                           ui::ElementIdentifier element_id) {
    auto button = std::make_unique<views::ImageButton>(
        base::BindRepeating(&VerticalTabStripSegmentedControl::OnButtonPressed,
                            base::Unretained(this), segment));
    button->SetTooltipText(l10n_util::GetStringUTF16(tooltip_id));
    button->SetProperty(views::kElementIdentifierKey, element_id);
    button->SetPreferredSize(gfx::Size(button_size, button_size));
    button->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
    button->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
    button->GetViewAccessibility().SetRole(ax::mojom::Role::kToggleButton);
    views::InstallCircleHighlightPathGenerator(button.get());
    views::ImageButton* button_ptr = AddChildView(std::move(button));
    layout->SetFlexForView(button_ptr, 1);
    return button_ptr;
  };

  tab_strip_button_ =
      create_button(Segment::kTabStrip, IDS_TAB_STRIP_BUTTON_TOOLTIP,
                    kVerticalTabStripTabStripButtonElementId);
  organizer_button_ = create_button(Segment::kOrganizer, IDS_TOOLTIP_TAB_SEARCH,
                                    kTabSearchButtonElementId);

  if (auto* controller = OrganizerPanelController::From(browser_)) {
    organizer_state_subscription_ = controller->RegisterOnStateChanged(
        base::IgnoreArgs<OrganizerPanelController*>(
            base::BindRepeating(&VerticalTabStripSegmentedControl::
                                    UpdateActiveSegmentFromController,
                                base::Unretained(this))));
  }

  UpdateActiveSegmentFromController();
  UpdateButtonIcons();
  UpdateSegmentBackgrounds();
}

VerticalTabStripSegmentedControl::~VerticalTabStripSegmentedControl() = default;

void VerticalTabStripSegmentedControl::OnThemeChanged() {
  views::View::OnThemeChanged();
  UpdateButtonIcons();
  UpdateSegmentBackgrounds();
}

views::ImageButton* VerticalTabStripSegmentedControl::GetButton(
    Segment segment) {
  switch (segment) {
    case Segment::kTabStrip:
      return tab_strip_button_;
    case Segment::kOrganizer:
      return organizer_button_;
  }
}

void VerticalTabStripSegmentedControl::OnButtonPressed(Segment segment) {
  if (auto* controller = OrganizerPanelController::From(browser_)) {
    switch (segment) {
      case Segment::kTabStrip:
        controller->SetOrganizerVisible(false);
        break;
      case Segment::kOrganizer:
        controller->SetOrganizerVisible(true);
        break;
    }
  }
}

void VerticalTabStripSegmentedControl::UpdateActiveSegmentFromController() {
  Segment new_segment = Segment::kTabStrip;
  if (auto* controller = OrganizerPanelController::From(browser_)) {
    if (controller->IsOrganizerPanelVisible()) {
      new_segment = Segment::kOrganizer;
    }
  }

  if (active_segment_ != new_segment) {
    active_segment_ = new_segment;
    if (tab_strip_button_) {
      tab_strip_button_->GetViewAccessibility().SetIsSelected(
          active_segment_ == Segment::kTabStrip);
    }
    if (organizer_button_) {
      organizer_button_->GetViewAccessibility().SetIsSelected(
          active_segment_ == Segment::kOrganizer);
    }
    UpdateButtonIcons();
    UpdateSegmentBackgrounds();
  }
}

void VerticalTabStripSegmentedControl::UpdateButtonIcons() {
  if (!GetWidget() || !tab_strip_button_ || !organizer_button_) {
    return;
  }

  const int icon_size =
      GetLayoutConstant(LayoutConstant::kVerticalTabStripComboButtonIconSize);

  auto update_button_icon = [&](Segment segment, const gfx::VectorIcon& icon) {
    const bool is_active = (active_segment_ == segment);
    const ui::ColorId color_id =
        is_active ? ui::kColorSysOnTonalContainer : ui::kColorIcon;
    GetButton(segment)->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(icon, color_id, icon_size));
  };

  update_button_icon(Segment::kTabStrip, features::IsRoundedIconsEnabled()
                                             ? kTabIcon
                                             : kTabOldIcon);
  update_button_icon(Segment::kOrganizer, features::IsRoundedIconsEnabled()
                                              ? kManageSearchIcon
                                              : kTabSearchTabStripOldIcon);
}

void VerticalTabStripSegmentedControl::UpdateSegmentBackgrounds() {
  auto update_segment_background = [&](Segment segment) {
    if (views::ImageButton* button = GetButton(segment)) {
      button->SetBackground(
          active_segment_ == segment
              ? views::CreatePillBackground(ui::kColorSysTonalContainer)
              : nullptr);
    }
  };

  update_segment_background(Segment::kTabStrip);
  update_segment_background(Segment::kOrganizer);
}

BEGIN_METADATA(VerticalTabStripSegmentedControl)
END_METADATA
