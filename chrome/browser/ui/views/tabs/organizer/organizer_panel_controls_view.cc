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
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view_class_properties.h"

OrganizerPanelControlsView::OrganizerPanelControlsView(
    actions::ActionItem* root_action_item) {
  SetOrientation(views::LayoutOrientation::kHorizontal);
  SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  SetMainAxisAlignment(views::LayoutAlignment::kEnd);
  SetProperty(views::kFlexBehaviorKey,
              views::FlexSpecification(GetDefaultFlexRule()));

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
