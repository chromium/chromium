// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/autofill_ai/autofill_ai_loading_state_view.h"

#include "chrome/browser/ui/views/autofill/popup/autofill_ai/autofill_ai_animated_gradient_view.h"
#include "chrome/browser/ui/views/autofill/popup/autofill_ai/autofill_ai_icon_image_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_cell_utils.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"

namespace autofill_ai {

AutofillAiLoadingStateView::AutofillAiLoadingStateView(
    const autofill::Suggestion& suggestion) {
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  GetViewAccessibility().SetRole(ax::mojom::Role::kStaticText);
  GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_LOADING_SUGGESTIONS_A11Y_HINT));

  SetInsideBorderInsets(
      gfx::Insets(autofill::PopupBaseView::ArrowHorizontalMargin()));
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);

  AddChildView(autofill_ai::CreateSmallAutofillAiIconImageView());
  autofill::popup_cell_utils::AddSpacerWithSize(
      *this, autofill::PopupBaseView::ArrowHorizontalMargin(),
      /*resize=*/false);

  AddChildView(std::make_unique<AutofillAiAnimatedGradientView>());
}

AutofillAiLoadingStateView::~AutofillAiLoadingStateView() = default;

BEGIN_METADATA(AutofillAiLoadingStateView)
END_METADATA

}  // namespace autofill_ai
