// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COMMERCE_DISCOUNTS_COUPON_CODE_LABEL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_COMMERCE_DISCOUNTS_COUPON_CODE_LABEL_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/flex_layout_view.h"

DECLARE_ELEMENT_IDENTIFIER_VALUE(kDiscountsBubbleCopyButtonElementId);

class DiscountsCouponCodeLabelView : public views::FlexLayoutView {
  METADATA_HEADER(DiscountsCouponCodeLabelView, views::FlexLayoutView)

 public:
  DiscountsCouponCodeLabelView(
      const std::u16string& promo_code_text,
      base::RepeatingClosure copy_button_clicked_callback);
  ~DiscountsCouponCodeLabelView() override;

  void UpdateCopyButtonTooltipsAndAccessibleNames(std::u16string tooltip);

  // views::View
  void OnThemeChanged() override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

 private:
  void OnCopyButtonClicked();

  raw_ptr<views::MdTextButton> copy_button_ = nullptr;
  const std::u16string promo_code_text_;
  base::RepeatingClosure copy_button_clicked_callback_;

  base::WeakPtrFactory<DiscountsCouponCodeLabelView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_COMMERCE_DISCOUNTS_COUPON_CODE_LABEL_VIEW_H_
