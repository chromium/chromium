// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/menu/highlighting_menu_button_helper.h"

#include "base/types/pass_key.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/menu/highlighting_simple_menu_model_delegate.h"

namespace user_education {

HighlightingMenuButtonHelper::~HighlightingMenuButtonHelper() = default;

void HighlightingMenuButtonHelper::MaybeHighlight(
    FeaturePromoController* controller,
    ui::ElementIdentifier button_element_id,
    HighlightingSimpleMenuModelDelegate* menu_model_delegate) {
  if (!controller || !button_element_id) {
    return;
  }
  if (const auto* spec = controller->GetCurrentPromoSpecificationForAnchor(
          button_element_id)) {
    menu_model_delegate->SetHighlight(
        base::PassKey<HighlightingMenuButtonHelper>(),
        spec->highlighted_menu_identifier(),
        controller->CloseBubbleAndContinuePromo(*spec->feature()));
  }
}

}  // namespace user_education
