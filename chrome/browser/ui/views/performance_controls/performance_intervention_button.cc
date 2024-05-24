// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/performance_controls/performance_intervention_button.h"

#include <memory>
#include <string>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/performance_controls/performance_intervention_button_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/performance_controls/performance_intervention_bubble.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/grit/generated_resources.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

PerformanceInterventionButton::PerformanceInterventionButton(
    BrowserView* browser_view)
    : ToolbarButton(
          base::BindRepeating(&PerformanceInterventionButton::OnClicked,
                              base::Unretained(this))),
      browser_view_(browser_view) {
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);
  SetFlipCanvasOnPaintForRTLUI(false);
  // TODO(crbug.com/338620692): Replace placeholder accessibility name when
  // strings finalize.
  SetAccessibleName(std::u16string(),
                    ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
  SetProperty(views::kElementIdentifierKey,
              kToolbarPerformanceInterventionButtonElementId);
  SetVisible(false);

  controller_ = std::make_unique<PerformanceInterventionButtonController>(
      this, browser_view->browser());
}

PerformanceInterventionButton::~PerformanceInterventionButton() = default;

void PerformanceInterventionButton::Show() {
  SetVisible(true);
  PreferredSizeChanged();
  CreateBubble();
}

void PerformanceInterventionButton::Hide() {
  SetVisible(false);
  PreferredSizeChanged();
}

bool PerformanceInterventionButton::IsButtonShowing() {
  return GetVisible();
}

bool PerformanceInterventionButton::IsBubbleShowing() {
  return bubble_dialog_model_host_ != nullptr;
}

void PerformanceInterventionButton::OnWidgetDestroying(views::Widget* widget) {
  bubble_dialog_model_host_ = nullptr;
  scoped_widget_observation_.Reset();
}

void PerformanceInterventionButton::OnThemeChanged() {
  ToolbarButton::OnThemeChanged();
  SetImageModel(
      ButtonState::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(
          kMemorySaverChromeRefreshIcon,
          GetColorProvider()->GetColor(kColorDownloadToolbarButtonActive)));
}

void PerformanceInterventionButton::OnClicked() {
  if (IsBubbleShowing()) {
    PerformanceInterventionBubble::CloseBubble(bubble_dialog_model_host_);
  } else {
    CreateBubble();
  }
}

void PerformanceInterventionButton::CreateBubble() {
  CHECK(GetWidget());
  bubble_dialog_model_host_ = PerformanceInterventionBubble::CreateBubble(
      browser_view_->browser(), this, controller_.get());
  scoped_widget_observation_.Observe(bubble_dialog_model_host_->GetWidget());
}

BEGIN_METADATA(PerformanceInterventionButton)
END_METADATA
