// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/autofill_prediction_improvements/autofill_prediction_improvements_loading_state_view.h"

#include "chrome/browser/ui/views/autofill/popup/autofill_prediction_improvements/autofill_prediction_improvements_animated_gradient_view.h"
#include "chrome/browser/ui/views/autofill/popup/autofill_prediction_improvements/prediction_improvements_icon_image_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_cell_utils.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"

namespace autofill_prediction_improvements {

PredictionImprovementsLoadingStateView::PredictionImprovementsLoadingStateView(
    const autofill::Suggestion& suggestion) {
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  GetViewAccessibility().SetRole(ax::mojom::Role::kStaticText);
  GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_LOADING_SUGGESTIONS_A11Y_HINT));

  SetInsideBorderInsets(
      gfx::Insets(autofill::PopupBaseView::ArrowHorizontalMargin()));
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);

  AddChildView(autofill_prediction_improvements::
                   CreateSmallPredictionImprovementsIconImageView());
  autofill::popup_cell_utils::AddSpacerWithSize(
      *this, autofill::PopupBaseView::ArrowHorizontalMargin(),
      /*resize=*/false);

  AddChildView(std::make_unique<PredictionImprovementsAnimatedGradientView>());
}

PredictionImprovementsLoadingStateView::
    ~PredictionImprovementsLoadingStateView() = default;

BEGIN_METADATA(PredictionImprovementsLoadingStateView)
END_METADATA

}  // namespace autofill_prediction_improvements
