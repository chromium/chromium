// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_BNPL_FOOTNOTE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_BNPL_FOOTNOTE_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace autofill {

class AutofillPopupController;

// A view that displays the privacy and legal footnote for Buy Now, Pay Later
// (BNPL) in the autofill popup. It handles dynamic bolding and embedded links.
class PopupBnplFootnoteView : public views::View {
  METADATA_HEADER(PopupBnplFootnoteView, views::View)

 public:
  explicit PopupBnplFootnoteView(
      base::WeakPtr<AutofillPopupController> controller);
  PopupBnplFootnoteView(const PopupBnplFootnoteView&) = delete;
  PopupBnplFootnoteView& operator=(const PopupBnplFootnoteView&) = delete;
  ~PopupBnplFootnoteView() override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_BNPL_FOOTNOTE_VIEW_H_
