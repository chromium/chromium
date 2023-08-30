// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_PROMO_CODE_LABEL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_PROMO_CODE_LABEL_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/flex_layout_view.h"

namespace autofill {
class PromoCodeLabelView : public views::FlexLayoutView {
 public:
  PromoCodeLabelView(
      gfx::Size& preferred_size,
      const std::u16string& promo_code_text,
      views::Button::PressedCallback copy_button_pressed_callback);
  ~PromoCodeLabelView() override;

  void UpdateCopyButtonTooltipsAndAccessibleNames(std::u16string& tooltip);

  // views::View
  void OnThemeChanged() override;

  raw_ptr<views::LabelButton> GetCopyButtonForTesting();
  const std::u16string& GetPromoCodeLabelTextForTesting() const;

 private:
  raw_ptr<views::MdTextButton> copy_button_ = nullptr;
  raw_ptr<views::Label> promo_code_label_ = nullptr;
};
}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_PROMO_CODE_LABEL_VIEW_H_
