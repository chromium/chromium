// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_MENU_HIGHLIGHTING_MENU_BUTTON_HELPER_H_
#define COMPONENTS_USER_EDUCATION_COMMON_MENU_HIGHLIGHTING_MENU_BUTTON_HELPER_H_

#include "components/user_education/common/menu/highlighting_simple_menu_model_delegate.h"
#include "ui/base/interaction/element_identifier.h"

namespace user_education {

class FeaturePromoController;

// Can be instantiated by a button to handle the case where an IPH is anchored
// to the button, and then the button is clicked producing a menu.
//
// In this case, the IPH should have its bubble closed (so as not to fight with
// the menu) and possibly highlight a menu item associated with the promo.
class HighlightingMenuButtonHelper {
 public:
  virtual ~HighlightingMenuButtonHelper();

  // Maybe ends an IPH owned by `controller`, anchored to `button_element_id`,
  // and possibly highlights a menu item in `menu_model_delegate`.
  //
  // Most specializations of this class will hide this method and provide a
  // simpler API to look up the controller, etc.
  void MaybeHighlight(FeaturePromoController* controller,
                      ui::ElementIdentifier button_element_id,
                      HighlightingSimpleMenuModelDelegate* menu_model_delegate);
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_MENU_HIGHLIGHTING_MENU_BUTTON_HELPER_H_
