// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/performance_controls/performance_intervention_button.h"

#include <memory>
#include <string>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/performance_controls/performance_intervention_button_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/view_class_properties.h"

PerformanceInterventionButton::PerformanceInterventionButton()
    : ToolbarButton(PressedCallback()) {
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);
  SetFlipCanvasOnPaintForRTLUI(false);
  // TODO(crbug.com/338620692): Replace placeholder accessibility name when
  // strings finalize.
  SetAccessibleName(std::u16string(),
                    ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
  SetProperty(views::kElementIdentifierKey,
              kToolbarPerformanceInterventionButtonElementId);

  controller_ = std::make_unique<PerformanceInterventionButtonController>(this);
}

PerformanceInterventionButton::~PerformanceInterventionButton() = default;

void PerformanceInterventionButton::Show() {
  SetVisible(true);
  PreferredSizeChanged();
}

void PerformanceInterventionButton::Hide() {
  SetVisible(false);
  PreferredSizeChanged();
}

void PerformanceInterventionButton::OnThemeChanged() {
  views::View::OnThemeChanged();
  SetImageModel(
      ButtonState::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(
          kMemorySaverChromeRefreshIcon,
          GetColorProvider()->GetColor(kColorDownloadToolbarButtonActive)));
}

BEGIN_METADATA(PerformanceInterventionButton)
END_METADATA
