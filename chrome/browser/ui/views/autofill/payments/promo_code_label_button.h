// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_PROMO_CODE_LABEL_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_PROMO_CODE_LABEL_BUTTON_H_

#include <string>

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/label_button.h"

namespace autofill {

// LabelButton that displays a merchant promo code in an emphasized format with
// a copy button icon.
class PromoCodeLabelButton : public views::LabelButton {
  METADATA_HEADER(PromoCodeLabelButton, views::LabelButton)

 public:
  PromoCodeLabelButton(PressedCallback callback, const std::u16string& text);

  PromoCodeLabelButton(const PromoCodeLabelButton&) = delete;
  PromoCodeLabelButton& operator=(const PromoCodeLabelButton&) = delete;

  ~PromoCodeLabelButton() override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_PROMO_CODE_LABEL_BUTTON_H_
