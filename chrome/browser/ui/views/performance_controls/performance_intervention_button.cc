// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/performance_controls/performance_intervention_button.h"

#include <memory>

#include "base/functional/bind.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"
#include "chrome/browser/ui/performance_controls/performance_intervention_button_controller.h"
#include "chrome/browser/ui/views/performance_controls/performance_intervention_bubble.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_anchor.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

PerformanceInterventionButton::PerformanceInterventionButton(
    BrowserWindowInterface* browser)
    : ToolbarButton(
          base::BindRepeating(&PerformanceInterventionButton::OnClicked,
                              base::Unretained(this))),
      browser_(browser) {
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);
  SetFlipCanvasOnPaintForRTLUI(false);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_PERFORMANCE_INTERVENTION_BUTTON_ACCNAME));
  SetTooltipText(
      l10n_util::GetStringUTF16(IDS_PERFORMANCE_INTERVENTION_BUTTON_TOOLTIP));
  SetProperty(views::kElementIdentifierKey,
              kToolbarPerformanceInterventionButtonElementId);
  SetVisible(false);

  controller_ = std::make_unique<PerformanceInterventionButtonController>(
      this, browser_.get());

  if (menu_model()) {
    GetViewAccessibility().SetHasPopup(ax::mojom::HasPopup::kMenu);
  }
}

PerformanceInterventionButton::~PerformanceInterventionButton() = default;

void PerformanceInterventionButton::Show() {
  is_active_ = true;
  SetVisible(true);
  PreferredSizeChanged();
  CreateBubble();
}

void PerformanceInterventionButton::Hide() {
  SetVisible(false);
  PreferredSizeChanged();
}

bool PerformanceInterventionButton::IsButtonShowing() const {
  return GetVisible();
}

bool PerformanceInterventionButton::IsBubbleShowing() const {
  return bubble_dialog_model_host_ != nullptr;
}

void PerformanceInterventionButton::OnWidgetDestroying(views::Widget* widget) {
  PerformanceInterventionBubble::RecordCloseReason(widget->closed_reason());
  bubble_dialog_model_host_ = nullptr;
  scoped_widget_observation_.Reset();
}

void PerformanceInterventionButton::OnThemeChanged() {
  ToolbarButton::OnThemeChanged();
  UpdateIconColor();
}

void PerformanceInterventionButton::OnClicked() {
  is_active_ = false;
  UpdateIconColor();
  if (IsBubbleShowing()) {
    PerformanceInterventionBubble::CloseBubble(bubble_dialog_model_host_);
  } else {
    CreateBubble();
    RecordInterventionToolbarButtonClicked();
  }
}

void PerformanceInterventionButton::CreateBubble() {
  CHECK(GetWidget());
  bubble_dialog_model_host_ = PerformanceInterventionBubble::CreateBubble(
      views::BubbleAnchor(this), controller_.get());
  scoped_widget_observation_.Observe(bubble_dialog_model_host_->GetWidget());
}

void PerformanceInterventionButton::UpdateIconColor() {
  const ui::ColorId icon_color =
      is_active_ ? kColorPerformanceInterventionButtonIconActive
                 : kColorPerformanceInterventionButtonIconInactive;

  SetImageModel(
      ButtonState::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(features::IsRoundedIconsEnabled()
                                         ? kSpeedIcon
                                         : kPerformanceSpeedometerOldIcon,
                                     GetColorProvider()->GetColor(icon_color)));
}

BEGIN_METADATA(PerformanceInterventionButton)
END_METADATA
