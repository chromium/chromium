// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_BNPL_DIALOG_FOOTNOTE_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_BNPL_DIALOG_FOOTNOTE_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/metadata/view_factory.h"

namespace autofill {
struct TextLinkInfo;
}

namespace autofill::payments {

// Footnote view banner across the bottom of the BNPL Issuer selection dialog.
class BnplDialogFootnote : public views::BoxLayoutView {
  METADATA_HEADER(BnplDialogFootnote, views::BoxLayoutView)
 public:
  BnplDialogFootnote(const std::u16string& footnote_text,
                     const TextLinkInfo& text_link_info);
  BnplDialogFootnote(const BnplDialogFootnote&) = delete;
  BnplDialogFootnote& operator=(const BnplDialogFootnote&) = delete;
  ~BnplDialogFootnote() override;
};

BEGIN_VIEW_BUILDER(, BnplDialogFootnote, views::BoxLayoutView)
END_VIEW_BUILDER

}  // namespace autofill::payments

DEFINE_VIEW_BUILDER(, autofill::payments::BnplDialogFootnote)

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_BNPL_DIALOG_FOOTNOTE_H_
