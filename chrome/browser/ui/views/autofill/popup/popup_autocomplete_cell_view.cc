// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_autocomplete_cell_view.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/popup_cell_utils.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"

namespace autofill {

namespace {

constexpr int kCloseIconSize = 20;

}

PopupAutocompleteCellView::PopupAutocompleteCellView(
    base::WeakPtr<AutofillPopupController> controller,
    int line_number)
    : controller_(controller), line_number_(line_number) {
  CHECK(controller_);
  const Suggestion& kSuggestion = controller->GetSuggestionAt(line_number_);

  // Add the label views.
  std::unique_ptr<views::Label> main_text_label =
      popup_cell_utils::CreateMainTextLabel(
          kSuggestion.main_text, views::style::TextStyle::STYLE_PRIMARY);
  popup_cell_utils::FormatLabel(*main_text_label, kSuggestion.main_text,
                                controller);
  popup_cell_utils::AddSuggestionContentToView(
      kSuggestion, std::move(main_text_label),
      popup_cell_utils::CreateMinorTextLabel(kSuggestion.minor_text),
      /*description_label=*/nullptr,
      popup_cell_utils::CreateAndTrackSubtextViews(*this, controller,
                                                   line_number_),
      *this);

  // Prepare the callbacks to the controller.
  popup_cell_utils::AddCallbacksToContentView(controller, line_number_, *this);
  CreateDeleteButton();
}

PopupAutocompleteCellView::~PopupAutocompleteCellView() = default;

void PopupAutocompleteCellView::HandleKeyPressEventFocusOnButton() {
  CHECK(button_);

  button_focused_ = true;
  button_->GetViewAccessibility().SetPopupFocusOverride();
  button_->NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
  views::InkDrop::Get(button_->ink_drop_view())->GetInkDrop()->SetHovered(true);
  UpdateSelectedAndRunCallback(false);
}

void PopupAutocompleteCellView::HandleKeyPressEventFocusOnContent() {
  button_focused_ = false;
  GetViewAccessibility().SetPopupFocusOverride();
  NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
  views::InkDrop::Get(button_->ink_drop_view())
      ->GetInkDrop()
      ->SetHovered(false);
  UpdateSelectedAndRunCallback(true);
}

bool PopupAutocompleteCellView::HandleKeyPressEvent(
    const content::NativeWebKeyboardEvent& event) {
  switch (event.windows_key_code) {
    // When pressing left arrow key (LTR):
    // 1. Set button as not focused.
    // 2. Focus on the content
    // 3. Make sure to remove hovered style from the delete button.
    // 4. Update selected state.
    case ui::VKEY_LEFT:
      if (base::i18n::IsRTL()) {
        HandleKeyPressEventFocusOnButton();
      } else {
        HandleKeyPressEventFocusOnContent();
      }
      return true;
    // When pressing arrow arrow key (LTR):
    // 1. Set button as focused.
    // 2. Focus on the delete button.
    // 3. Make sure to add hovered style from the delete button.
    // 4. Update selected state.
    case ui::VKEY_RIGHT:
      if (base::i18n::IsRTL()) {
        HandleKeyPressEventFocusOnContent();
      } else {
        HandleKeyPressEventFocusOnButton();
      }
      return true;
    case ui::VKEY_RETURN:
      CHECK(button_);
      if (button_focused_) {
        DeleteAutocomplete();
        return true;
      }
      return false;
    default:
      return false;
  }
}

void PopupAutocompleteCellView::SetSelected(bool selected) {
  CHECK(button_);
  if ((selected || IsMouseHovered())) {
    button_->SetVisible(true);
  } else {
    button_->SetVisible(false);
  }
  autofill::PopupCellView::SetSelected(selected);

  // There are cases where SetSelect is called with the same value as before
  // but we still want to refresh the style. For example in the case where there
  // is an arrow navigation from the delete button to the next cell. In this
  // case we go directly from selected = false (buttons was selected) to again
  // selected = false, which does not lead to a style refresh. Another case is
  // when the cursor moves directly from the delete button to outside of the
  // cell (if moving quickly from top to bottom). This can lead the the style
  // never being refreshed. That is because the cell goes from not selected
  // (hovering the delete button) to again not selected (outside the cell
  // itself) without an intermediate state to update the style. Therefore we
  // always refresh the style as a sanity check.
  if (selected_ == selected) {
    RefreshStyle();
  }

  // We also always reset `button_focused_` when selected is updated due to
  // mouse navigation.
  // This prevents the case where the delete button is focused but the cursor is
  // moved to the content.
  button_focused_ = false;
}

views::ImageButton* PopupAutocompleteCellView::GetDeleteButton() {
  return button_;
}

void PopupAutocompleteCellView::CreateDeleteButton() {
  std::unique_ptr<views::ImageButton> button =
      views::CreateVectorImageButtonWithNativeTheme(
          base::BindRepeating(&PopupAutocompleteCellView::DeleteAutocomplete,
                              base::Unretained(this)),
          views::kIcCloseIcon, kCloseIconSize);
  InstallCircleHighlightPathGenerator(button.get());
  button->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_DELETE_AUTOCOMPLETE_SUGGESTION_TOOLTIP));
  button->SetAccessibleRole(ax::mojom::Role::kMenuItem);
  button->SetAccessibleName(l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_DELETE_AUTOCOMPLETE_SUGGESTION_A11Y_HINT,
      popup_cell_utils::GetVoiceOverStringFromSuggestion(
          controller_->GetSuggestionAt(line_number_))));
  button->SetVisible(false);

  // Make child view grow to fill the space and align the button to the right.
  CHECK(GetLayoutManager());
  views::BoxLayout* layout = static_cast<views::BoxLayout*>(GetLayoutManager());
  for (views::View* child : children()) {
    layout->SetFlexForView(child, 1);
  }

  views::View* button_placeholder =
      AddChildView(std::make_unique<views::View>());
  button_placeholder->SetLayoutManager(std::make_unique<views::BoxLayout>());
  button_placeholder->SetPreferredSize(button->GetPreferredSize());
  button_ = button_placeholder->AddChildView(std::move(button));
  layout->SetFlexForView(button_placeholder, 0);

  button_state_change_subscription_ = button_->AddStateChangedCallback(
      base::BindRepeating(&PopupAutocompleteCellView::OnButtonPropertyChanged,
                          base::Unretained(this)));
}

void PopupAutocompleteCellView::DeleteAutocomplete() {
  if (controller_ && controller_->RemoveSuggestion(line_number_)) {
    AutofillMetrics::OnAutocompleteSuggestionDeleted(
        AutofillMetrics::AutocompleteSingleEntryRemovalMethod::
            kDeleteButtonClicked);
  }
}

void PopupAutocompleteCellView::OnButtonPropertyChanged() {
  CHECK(button_);
  bool selected = !button_->IsMouseHovered() && IsMouseHovered();

  UpdateSelectedAndRunCallback(selected);
}

void PopupAutocompleteCellView::UpdateSelectedAndRunCallback(bool selected) {
  if (selected_ == selected) {
    return;
  }

  selected_ = selected;
  if (base::RepeatingClosure callback =
          selected_ ? on_selected_callback_ : on_unselected_callback_) {
    callback.Run();
  }
}

}  // namespace autofill
