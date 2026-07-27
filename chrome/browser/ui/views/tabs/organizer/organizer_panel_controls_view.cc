// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_controls_view.h"

#include <memory>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/tabs/vertical/top_container_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view_class_properties.h"

OrganizerPanelControlsView::OrganizerPanelControlsView(
    actions::ActionItem* root_action_item) {
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));

  toggle_organizer_panel_action_item_ =
      actions::ActionManager::Get().FindAction(kActionToggleOrganizerPanel,
                                               root_action_item);
  CHECK(toggle_organizer_panel_action_item_);

  organizer_button_ = AddChildView(std::make_unique<TopContainerButton>());
  organizer_button_->SetPaintToLayer();
  organizer_button_->layer()->SetFillsBoundsOpaquely(false);
  organizer_button_->SetCallback(
      base::BindRepeating(&OrganizerPanelControlsView::OnCloseButtonPressed,
                          base::Unretained(this)));
  organizer_button_->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  organizer_button_->SetImageModel(
      views::Button::ButtonState::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(features::IsRoundedIconsEnabled()
                                         ? kCloseSmallIcon
                                         : kCloseChromeRefreshOldIcon,
                                     ui::kColorIcon));
  organizer_button_->SetProperty(views::kElementIdentifierKey,
                                 kOrganizerPanelButtonElementId);
  UpdateTooltipText();

  ConfigureInkDrop(organizer_button_);

  SetProperty(views::kElementIdentifierKey,
              kOrganizerPanelControlsViewElementId);
}

OrganizerPanelControlsView::~OrganizerPanelControlsView() = default;

views::ProposedLayout OrganizerPanelControlsView::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layout;
  gfx::Size host_size =
      gfx::Size(size_bounds.width().is_bounded() ? size_bounds.width().value()
                                                 : parent()->width(),
                GetLayoutConstant(
                    LayoutConstant::kVerticalTabStripTopButtonContainerHeight));

  CHECK(organizer_button_);

  const gfx::Size organizer_button_pref_size =
      organizer_button_->GetPreferredSize();

  int current_x = host_size.width();
  int current_y = host_size.height();

  // Calculate bounds to right-align the button horizontally and center it
  // vertically within the available space.
  gfx::Rect organizer_button_bounds(
      current_x - organizer_button_pref_size.width(),
      current_y -
          (GetLayoutConstant(
               LayoutConstant::kVerticalTabStripTopButtonContainerHeight) +
           organizer_button_pref_size.height()) /
              2,
      organizer_button_pref_size.width(), organizer_button_pref_size.height());
  layout.child_layouts.emplace_back(
      organizer_button_.get(), organizer_button_->GetVisible(),
      organizer_button_bounds, views::SizeBounds(organizer_button_pref_size));

  layout.host_size = host_size;

  return layout;
}

bool OrganizerPanelControlsView::IsPositionInWindowCaption(
    const gfx::Point& point) {
  if (organizer_button_ && IsHitInView(organizer_button_, point)) {
    return false;
  }

  return true;
}

void OrganizerPanelControlsView::UpdateTooltipText() {
  auto organizer_button_text =
      std::u16string(toggle_organizer_panel_action_item_->GetText());
  organizer_button_->GetViewAccessibility().SetName(organizer_button_text);
  organizer_button_->SetTooltipText(organizer_button_text);
}

void OrganizerPanelControlsView::SetButtonOpacity(float opacity) {
  organizer_button_->layer()->SetOpacity(opacity);
}

void OrganizerPanelControlsView::OnCloseButtonPressed() {
  toggle_organizer_panel_action_item_->InvokeAction();
}

BEGIN_METADATA(OrganizerPanelControlsView)
END_METADATA
