// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_ROW_PREDICTION_IMPROVEMENTS_DETAILS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_ROW_PREDICTION_IMPROVEMENTS_DETAILS_VIEW_H_

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace autofill {

class AutofillPopupController;

// A class for displaying a suggestion that gives details about the prediction
// improvemebts feature. It contains an explanation text and a link that will
// direct the use to a page to know more about it.
class PopupRowPredictionImprovementsDetailsView : public PopupRowView {
  METADATA_HEADER(PopupRowPredictionImprovementsDetailsView, PopupRowView)

 public:
  static constexpr int kLearnMoreStyledLabelViewID = 764;

  PopupRowPredictionImprovementsDetailsView(
      AccessibilitySelectionDelegate& a11y_selection_delegate,
      SelectionDelegate& selection_delegate,
      base::WeakPtr<AutofillPopupController> controller,
      int line_number);

  PopupRowPredictionImprovementsDetailsView(
      const PopupRowPredictionImprovementsDetailsView&) = delete;
  PopupRowPredictionImprovementsDetailsView& operator=(
      const PopupRowPredictionImprovementsDetailsView&) = delete;
  ~PopupRowPredictionImprovementsDetailsView() override;

  // PopupRowView:
  bool HandleKeyPressEvent(const input::NativeWebKeyboardEvent& event) override;

 private:
  base::RepeatingClosure learn_more_callback_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_ROW_PREDICTION_IMPROVEMENTS_DETAILS_VIEW_H_
