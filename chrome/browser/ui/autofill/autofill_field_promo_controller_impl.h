// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_FIELD_PROMO_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_FIELD_PROMO_CONTROLLER_IMPL_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_field_promo_controller.h"
#include "chrome/browser/ui/autofill/autofill_field_promo_view.h"
#include "chrome/browser/ui/autofill/autofill_popup_hide_helper.h"
#include "ui/base/interaction/element_identifier.h"

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class RectF;
}  // namespace gfx

namespace autofill {

enum class PopupHidingReason;

class AutofillFieldPromoControllerImpl : public AutofillFieldPromoController {
 public:
  AutofillFieldPromoControllerImpl(
      content::WebContents* web_contents,
      ui::ElementIdentifier promo_element_identifier);

  AutofillFieldPromoControllerImpl(const AutofillFieldPromoControllerImpl&) =
      delete;
  AutofillFieldPromoControllerImpl& operator=(
      const AutofillFieldPromoControllerImpl&) = delete;
  ~AutofillFieldPromoControllerImpl() override;

  void Show(const gfx::RectF& bounds) override;
  void Hide() override;

#if defined(UNIT_TEST)
  void SetPromoViewForTesting(
      base::WeakPtr<AutofillFieldPromoView> promo_view) {
    promo_view_ = promo_view;
  }
#endif

 private:
  const raw_ptr<content::WebContents> web_contents_;
  // The view sets the `element_identifier_` as its property. This is how the
  // IPH knows on which view to attach itself.
  const ui::ElementIdentifier promo_element_identifier_;
  base::WeakPtr<AutofillFieldPromoView> promo_view_;
  // This is a helper which detects events that should hide the promo.
  std::unique_ptr<AutofillPopupHideHelper> promo_hide_helper_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_FIELD_PROMO_CONTROLLER_IMPL_H_
