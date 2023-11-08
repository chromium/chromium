// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/views/autofill/popup/popup_row_factory_utils.h"

#include "chrome/browser/ui/views/autofill/popup/popup_cell_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_cell_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_with_button_view.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"

namespace autofill {

namespace {
// The size of a close or delete icon.
constexpr int kCloseIconSize = 16;

// Returns a wrapper around `closure` that posts it to the default message
// queue instead of executing it directly. This is to avoid that the callback's
// caller can suicide by (unwittingly) deleting itself or its parent.
base::RepeatingClosure CreateExecuteSoonWrapper(base::RepeatingClosure task) {
  return base::BindRepeating(
      [](base::RepeatingClosure delayed_task) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, std::move(delayed_task));
      },
      std::move(task));
}
}  // namespace

std::unique_ptr<PopupRowWithButtonView> CreateAutocompleteRowWithDeleteButton(
    base::WeakPtr<AutofillPopupController> controller,
    PopupRowView::AccessibilitySelectionDelegate& a11y_selection_delegate,
    PopupRowView::SelectionDelegate& selection_delegate,
    int line_number) {
  auto view = std::make_unique<PopupCellView>();

  const Suggestion& kSuggestion = controller->GetSuggestionAt(line_number);
  std::unique_ptr<views::Label> main_text_label =
      popup_cell_utils::CreateMainTextLabel(
          kSuggestion.main_text,
          GetMainTextStyleForPopupItemId(kSuggestion.popup_item_id));
  popup_cell_utils::FormatLabel(*main_text_label, kSuggestion.main_text,
                                controller);
  popup_cell_utils::AddSuggestionContentToView(
      kSuggestion, std::move(main_text_label),
      popup_cell_utils::CreateMinorTextLabel(kSuggestion.minor_text),
      /*description_label=*/nullptr,
      popup_cell_utils::CreateAndTrackSubtextViews(*view, controller,
                                                   line_number),
      *view);

  // Setup a layout of the delete button for Autocomplete entries.
  views::BoxLayout* layout =
      static_cast<views::BoxLayout*>(view->GetLayoutManager());
  for (views::View* child : view->children()) {
    layout->SetFlexForView(child, 1);
  }

  // The closure that actually attempts to delete an entry and record metrics
  // for it.
  base::RepeatingClosure deletion_action = base::BindRepeating(
      [](base::WeakPtr<AutofillPopupController> controller, int line_number) {
        if (controller && controller->RemoveSuggestion(line_number)) {
          AutofillMetrics::OnAutocompleteSuggestionDeleted(
              AutofillMetrics::AutocompleteSingleEntryRemovalMethod::
                  kDeleteButtonClicked);
        }
      },
      controller, line_number);
  std::unique_ptr<views::ImageButton> button =
      views::CreateVectorImageButtonWithNativeTheme(
          CreateExecuteSoonWrapper(std::move(deletion_action)),
          views::kIcCloseIcon, kCloseIconSize);

  // We are making sure that the vertical distance from the delete button edges
  // to the cell border is the same as the horizontal distance.
  // 1. Take the current horizontal distance.
  int horizontal_margin = layout->inside_border_insets().right();
  // 2. Take the height of the cell.
  int cell_height = layout->minimum_cross_axis_size();
  // 3. The diameter needs to be the height - 2 * the desired margin.
  int radius = (cell_height - horizontal_margin * 2) / 2;
  views::InstallFixedSizeCircleHighlightPathGenerator(button.get(), radius);
  button->SetPreferredSize(gfx::Size(radius * 2, radius * 2));
  button->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_DELETE_AUTOCOMPLETE_SUGGESTION_TOOLTIP));
  button->SetAccessibleRole(ax::mojom::Role::kMenuItem);
  button->SetAccessibleName(l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_DELETE_AUTOCOMPLETE_SUGGESTION_A11Y_HINT,
      popup_cell_utils::GetVoiceOverStringFromSuggestion(
          controller->GetSuggestionAt(line_number))));
  button->SetVisible(false);

  return std::make_unique<PopupRowWithButtonView>(
      a11y_selection_delegate, selection_delegate, controller, line_number,
      std::move(view), std::move(button),
      PopupRowWithButtonView::ButtonBehavior::kShowOnHoverOrSelect);
}

}  // namespace autofill
