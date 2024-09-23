// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_row_prediction_improvements_details_view.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_content_view.h"
#include "components/autofill/core/browser/ui/suggestion_button_action.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace autofill {

namespace {

// Creates the styled label view/ContentsView that is included inside the
// content view. Used to give users details about improved predictions.
std::unique_ptr<PopupRowContentView> CreateContentsView(
    base::RepeatingClosure learn_more_callback) {
  auto details_container = std::make_unique<PopupRowContentView>();
  // TODO(crbug.com/357026771): Possibly move `touchable_menu_height` inside the
  // `ChromeLayoutProvider`.
  details_container->SetMinimumCrossAxisSize(
      views::MenuConfig::instance().touchable_menu_height);

  std::vector<size_t> replacement_offsets;
  views::StyledLabel::RangeStyleInfo style_info =
      views::StyledLabel::RangeStyleInfo::CreateForLink(
          std::move(learn_more_callback));
  style_info.text_style = views::style::TextStyle::STYLE_LINK_5;
  const std::u16string manage_prediction_improvements_text_link =
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_MANAGE_PREDICTION_IMPROVEMENTS);
  const std::u16string formatted_text = l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_DETAILS_SUGGESTION_MAIN_TEXT,
      /*replacements=*/{manage_prediction_improvements_text_link},
      &replacement_offsets);
  details_container->AddChildView(
      views::Builder<views::StyledLabel>()
          .SetText(formatted_text)
          .SetDefaultTextStyle(views::style::STYLE_BODY_5)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .AddStyleRange(
              gfx::Range(replacement_offsets[0],
                         replacement_offsets[0] +
                             manage_prediction_improvements_text_link.length()),
              style_info)
          // This is used in tests only.
          .SetID(PopupRowPredictionImprovementsDetailsView::
                     kLearnMoreStyledLabelViewID)
          .Build());
  return details_container;
}

}  // namespace

PopupRowPredictionImprovementsDetailsView::
    PopupRowPredictionImprovementsDetailsView(
        AccessibilitySelectionDelegate& a11y_selection_delegate,
        SelectionDelegate& selection_delegate,
        base::WeakPtr<AutofillPopupController> controller,
        int line_number)
    : PopupRowView(
          a11y_selection_delegate,
          selection_delegate,
          controller,
          line_number,
          /*content_view=*/
          CreateContentsView(base::BindRepeating(
              &AutofillPopupController::PerformButtonActionForSuggestion,
              controller,
              line_number,
              PredictionImprovementsButtonActions::kLearnMoreClicked))),
      learn_more_callback_(base::BindRepeating(
          &AutofillPopupController::PerformButtonActionForSuggestion,
          controller,
          line_number,
          PredictionImprovementsButtonActions::kLearnMoreClicked)) {}

PopupRowPredictionImprovementsDetailsView::
    ~PopupRowPredictionImprovementsDetailsView() = default;

bool PopupRowPredictionImprovementsDetailsView::HandleKeyPressEvent(
    const input::NativeWebKeyboardEvent& event) {
  switch (event.windows_key_code) {
    case ui::VKEY_RETURN:
      // The link exists inside a suggestion's text. Since navigating to it via
      // keyboard adds code complexity, we simplify it by reacting to ENTER
      // keystrokes on the whole content cell. This is especially important for
      // a11y users who tend to use cursor navigation less often.
      // TODO(crbug.com/361434879): Make sure that screen readers announce how
      // one can open the link.
      learn_more_callback_.Run();
      return true;
    default:
      break;
  }

  return PopupRowView::HandleKeyPressEvent(event);
}

BEGIN_METADATA(PopupRowPredictionImprovementsDetailsView)
END_METADATA

}  // namespace autofill
