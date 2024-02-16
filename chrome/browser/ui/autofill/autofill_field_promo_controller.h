// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_FIELD_PROMO_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_FIELD_PROMO_CONTROLLER_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_field_promo_view.h"
#include "chrome/browser/ui/autofill/autofill_popup_hide_helper.h"
#include "ui/base/interaction/element_identifier.h"

namespace autofill {

// This class is the controller for the `AutofillFieldPromoView`. Its main
// function is to control the view's lifetime.
class AutofillFieldPromoController {
 public:
  virtual ~AutofillFieldPromoController() = default;

  // Displays the `AutofillFieldPromoView` (which is an invisible view), and
  // anchors the IPH onto the view. The IPH may or may not be shown depending on
  // configuration.
  virtual void Show(const gfx::RectF& bounds) = 0;
  // Hides and destroys the view.
  virtual void Hide() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_FIELD_PROMO_CONTROLLER_H_
