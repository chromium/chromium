// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_field_promo_controller_impl.h"

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_field_promo_view.h"
#include "components/autofill/core/browser/ui/popup_hiding_reasons.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/rect_f.h"

namespace autofill {

AutofillFieldPromoControllerImpl::AutofillFieldPromoControllerImpl(
    content::WebContents* web_contents,
    ui::ElementIdentifier promo_element_identifier)
    : web_contents_(web_contents),
      promo_element_identifier_(std::move(promo_element_identifier)) {}

AutofillFieldPromoControllerImpl::~AutofillFieldPromoControllerImpl() {
  Hide();
}

void AutofillFieldPromoControllerImpl::Show(const gfx::RectF& bounds) {
  Hide();

  AutofillPopupHideHelper::HidingParams hiding_params = {
      .hide_on_text_field_change = false};

  AutofillPopupHideHelper::HidingCallback hiding_callback =
      base::BindRepeating([](AutofillFieldPromoControllerImpl& controller,
                             PopupHidingReason) { controller.Hide(); },
                          std::ref(*this));

  AutofillPopupHideHelper::PictureInPictureDetectionCallback
      pip_detection_callback = base::BindRepeating(
          [](base::WeakPtr<AutofillFieldPromoView> view) {
            return view && view->OverlapsWithPictureInPictureWindow();
          },
          promo_view_);

  // The hide helper is destroyed on hide, so it cannot outlive the popup
  // controller.
  promo_hide_helper_ = AutofillPopupHideHelper::CreateAutofillPopupHideHelper(
      web_contents_, std::move(hiding_params), std::move(hiding_callback),
      std::move(pip_detection_callback));

  // If the hide helper is null, then no frame has focus.
  if (!promo_hide_helper_) {
    Hide();
    return;
  }

  promo_view_ = AutofillFieldPromoView::CreateAndShow(
      web_contents_, bounds, promo_element_identifier_);
}

void AutofillFieldPromoControllerImpl::Hide() {
  promo_hide_helper_.reset();
  if (promo_view_) {
    promo_view_->Close();
    promo_view_ = nullptr;
  }
}

}  // namespace autofill
