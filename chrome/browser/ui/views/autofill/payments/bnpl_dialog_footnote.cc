// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/bnpl_dialog_footnote.h"

#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace autofill::payments {

BnplDialogFootnote::BnplDialogFootnote(const std::u16string& footnote_text,
                                       const TextLinkInfo& text_link_info) {
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  SetInsideBorderInsets(ChromeLayoutProvider::Get()->GetInsetsMetric(
      views::INSETS_DIALOG_FOOTNOTE));

  views::StyledLabel::RangeStyleInfo style_info =
      views::StyledLabel::RangeStyleInfo::CreateForLink(
          text_link_info.callback);

  views::StyledLabel* label =
      AddChildView(std::make_unique<views::StyledLabel>());
  label->SetText(footnote_text);
  label->AddStyleRange(text_link_info.offset, style_info);
  if (!text_link_info.bold_range.is_empty()) {
    views::StyledLabel::RangeStyleInfo bold_style;
    bold_style.text_style = views::style::STYLE_EMPHASIZED;
    label->AddStyleRange(text_link_info.bold_range, bold_style);
  }
}

BnplDialogFootnote::~BnplDialogFootnote() = default;

BEGIN_METADATA(BnplDialogFootnote)
END_METADATA

}  // namespace autofill::payments
