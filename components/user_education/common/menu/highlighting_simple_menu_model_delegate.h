// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_MENU_HIGHLIGHTING_SIMPLE_MENU_MODEL_DELEGATE_H_
#define COMPONENTS_USER_EDUCATION_COMMON_MENU_HIGHLIGHTING_SIMPLE_MENU_MODEL_DELEGATE_H_

#include "base/types/pass_key.h"
#include "components/user_education/common/feature_promo/feature_promo_handle.h"
#include "ui/menus/simple_menu_model.h"

namespace user_education {

class HighlightingMenuButtonHelper;

// Base class for menu delegates that want to support user ed highlighting.
class HighlightingSimpleMenuModelDelegate
    : public ui::SimpleMenuModel::Delegate {
 public:
  ~HighlightingSimpleMenuModelDelegate() override;

  // SimpleMenuModel::Delegate:
  bool IsElementIdAlerted(ui::ElementIdentifier element_id) const final;
  void MenuClosed(ui::SimpleMenuModel*) final;

  // When an IPH is dismissed as a menu opens, indicates the highlight to show
  // in the menu (if any) as well as the promo handle (if any). The handle is
  // provided so no new promos can show while the menu is open.
  void SetHighlight(base::PassKey<HighlightingMenuButtonHelper>,
                    ui::ElementIdentifier highlighted_menu_identifier,
                    FeaturePromoHandle promo_handle);

 protected:
  // Called by `MenuClosed()` to provide derived classes an opportunity to
  // perform actions; do not override `MenuClosed()` directly.
  // Defaults to no-op.
  virtual void OnMenuClosed(ui::SimpleMenuModel*);

 private:
  ui::ElementIdentifier highlighted_menu_identifier_;
  FeaturePromoHandle promo_handle_;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_MENU_HIGHLIGHTING_SIMPLE_MENU_MODEL_DELEGATE_H_
