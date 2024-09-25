// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill_prediction_improvements/prediction_improvements_loading_state_view.h"

#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_cell_utils.h"
#include "chrome/browser/ui/views/autofill_prediction_improvements/prediction_improvements_animated_gradient_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/image_view.h"

namespace autofill_prediction_improvements {

PredictionImprovementsLoadingStateView::PredictionImprovementsLoadingStateView(
    const autofill::Suggestion& suggestion) {
  SetInsideBorderInsets(
      gfx::Insets(autofill::PopupBaseView::ArrowHorizontalMargin()));

  if (std::unique_ptr<views::ImageView> icon =
          autofill::popup_cell_utils::GetIconImageView(suggestion)) {
    AddChildView(std::move(icon));
    autofill::popup_cell_utils::AddSpacerWithSize(
        *this, autofill::PopupBaseView::ArrowHorizontalMargin(),
        /*resize=*/false);
  }

  AddChildView(std::make_unique<PredictionImprovementsAnimatedGradientView>());
}

PredictionImprovementsLoadingStateView::
    ~PredictionImprovementsLoadingStateView() = default;

BEGIN_METADATA(PredictionImprovementsLoadingStateView)
END_METADATA

}  // namespace autofill_prediction_improvements
