// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/menu/highlighting_simple_menu_model_delegate.h"

namespace user_education {

HighlightingSimpleMenuModelDelegate::~HighlightingSimpleMenuModelDelegate() =
    default;

bool HighlightingSimpleMenuModelDelegate::IsElementIdAlerted(
    ui::ElementIdentifier element_id) const {
  return element_id == highlighted_menu_identifier_;
}

void HighlightingSimpleMenuModelDelegate::MenuClosed(
    ui::SimpleMenuModel* model) {
  promo_handle_.Release();
  OnMenuClosed(model);
}

void HighlightingSimpleMenuModelDelegate::SetHighlight(
    base::PassKey<HighlightingMenuButtonHelper>,
    ui::ElementIdentifier highlighted_menu_identifier,
    FeaturePromoHandle promo_handle) {
  highlighted_menu_identifier_ = highlighted_menu_identifier;
  promo_handle_ = std::move(promo_handle);
}

void HighlightingSimpleMenuModelDelegate::OnMenuClosed(
    ui::SimpleMenuModel* model) {
  // Default is to do nothing; derived classes can add additional behavior.
}

}  // namespace user_education
