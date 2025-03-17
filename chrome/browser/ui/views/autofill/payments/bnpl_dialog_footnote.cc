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

  AddChildView(views::Builder<views::StyledLabel>()
                   .SetText(footnote_text)
                   .AddStyleRange(text_link_info.offset, style_info)
                   .Build());
}

BnplDialogFootnote::~BnplDialogFootnote() = default;

BEGIN_METADATA(BnplDialogFootnote)
END_METADATA

}  // namespace autofill::payments
