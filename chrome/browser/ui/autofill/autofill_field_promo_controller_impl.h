// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_FIELD_PROMO_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_FIELD_PROMO_CONTROLLER_IMPL_H_

#include "chrome/browser/ui/autofill/autofill_field_promo_controller.h"

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class RectF;
}  // namespace gfx

namespace ui {
class ElementIdentifier;
}  // namespace ui

namespace autofill {

enum class SuggestionHidingReason;
class AutofillFieldPromoControllerImpl : public AutofillFieldPromoController {
 public:
  AutofillFieldPromoControllerImpl(
      content::WebContents* web_contents,
      const base::Feature& feature_promo,
      ui::ElementIdentifier promo_element_identifier);

  AutofillFieldPromoControllerImpl(const AutofillFieldPromoControllerImpl&) =
      delete;
  AutofillFieldPromoControllerImpl& operator=(
      const AutofillFieldPromoControllerImpl&) = delete;
  ~AutofillFieldPromoControllerImpl() override;

  void Show(const gfx::RectF& bounds) override;
  void Hide() override;
  bool IsMaybeShowing() const override;
  const base::Feature& GetFeaturePromo() const override;

#if defined(UNIT_TEST)
  void SetPromoViewForTesting(
      base::WeakPtr<AutofillFieldPromoView> promo_view) {
    promo_view_ = promo_view;
  }
#endif

 private:
  void OnShowPromoResult(user_education::FeaturePromoResult);

  const raw_ptr<content::WebContents> web_contents_;
  const raw_ref<const base::Feature> feature_promo_;
  // The view sets the `element_identifier_` as its property. This is how the
  // IPH knows on which view to attach itself.
  const ui::ElementIdentifier promo_element_identifier_;
  base::WeakPtr<AutofillFieldPromoView> promo_view_;
  // This is a helper which detects events that should hide the promo.
  std::optional<AutofillPopupHideHelper> promo_hide_helper_;
  bool is_maybe_showing_ = false;
  base::WeakPtrFactory<AutofillFieldPromoControllerImpl> weak_ptr_factory_{
      this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_FIELD_PROMO_CONTROLLER_IMPL_H_
