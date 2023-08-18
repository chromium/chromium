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
#include "ui/gfx/canvas.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace autofill {

namespace {

constexpr int kCloseIconSize = 16;

// Overrides `OnMouseEntered` and `OnMouseExited` from
// `views::ButtonController`. Used by `PopupAutocompleteCellView` to know when
// the mouse cursor has entered or left the delete button in order to run the
// selection callbacks.
class DeleteButtonController : public views::ButtonController {
 public:
  DeleteButtonController(
      views::Button* button,
      DeleteButtonDelegate* delete_button_owner,
      std::unique_ptr<views::ButtonControllerDelegate> delegate)
      : views::ButtonController(button, std::move(delegate)),
        delete_button_owner_(delete_button_owner) {}

  void OnMouseEntered(const ui::MouseEvent& event) override {
    delete_button_owner_->OnMouseEnteredDeleteButton();
    views::ButtonController::OnMouseEntered(event);
  }

  void OnMouseExited(const ui::MouseEvent& event) override {
    delete_button_owner_->OnMouseExitedDeleteButton();
    views::ButtonController::OnMouseExited(event);
  }

 private:
  raw_ptr<DeleteButtonDelegate> delete_button_owner_ = nullptr;
};

ButtonPlaceholder::ButtonPlaceholder(DeleteButtonDelegate* delete_button_owner)
    : delete_button_owner_(delete_button_owner) {}
ButtonPlaceholder::~ButtonPlaceholder() = default;

void ButtonPlaceholder::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);
  if (first_paint_happened_) {
    return;
  }

  // The button placeholder should have only the button as child.
  CHECK(children().size() == 1);
  // The placeholder also always have a parent, the cell.
  CHECK(parent());

  // If the row that owns the placeholder is rendered right under the cursor, we
  // make the button visible.
  View* delete_button = children()[0];
  if (parent()->IsMouseHovered()) {
    view_bounds_changed_observer_.Observe(delete_button);
    delete_button->SetVisible(true);
  }
  first_paint_happened_ = true;
}

void ButtonPlaceholder::OnViewBoundsChanged(View* observed_view) {
  CHECK(children().size() == 1);

  // After making the button visible, we will be notified about its bounds
  // changing. We emit a `SynthesizeMouseMoveEvent()` to first select the cell
  // and then conditionally select the button if hovering over it. We cannot
  // simply call `SynthesizeMouseMoveEvent()` because it calls the cell
  // `OnMouseEntered` method after we highlight the button, leading the an
  // incorrect state.
  View* delete_button = children()[0];
  if (observed_view == delete_button && observed_view->GetVisible()) {
    GetWidget()->SynthesizeMouseMoveEvent();
    if (observed_view->IsMouseHovered()) {
      delete_button_owner_->OnMouseEnteredDeleteButton();
      view_bounds_changed_observer_.Reset();
    }
  }
}

int ButtonPlaceholder::GetHeightForWidth(int width) const {
  // The parent for this view (the cell) and the placeholder button uses a
  // `BoxLayout` for its `LayoutManager`. Internally `BoxLayout` uses
  // `GetHeightForWidth` on each children to define their height when the
  // orientation is not `kVertical`. Finally these children uses
  // `BoxLayout::GetPreferredSizeForChildWidth` to tell their parent their
  // height, however they only return a non 0 value if they have visible
  // children. This is not the case here because the button is at first no
  // visible. Therefore we override GetHeightForWidth to return the preferred
  // height regardless of children being visible or not.
  return GetPreferredSize().height();
}
}  // namespace

PopupAutocompleteCellView::PopupAutocompleteCellView(
    base::WeakPtr<AutofillPopupController> controller,
    int line_number)
    : PopupCellView(
          controller->ShouldIgnoreMouseObservedOutsideItemBoundsCheck()),
      controller_(controller),
      line_number_(line_number) {
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
  button_->SetAccessibleName(l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_DELETE_AUTOCOMPLETE_SUGGESTION_A11Y_HINT,
      popup_cell_utils::GetVoiceOverStringFromSuggestion(
          controller_->GetSuggestionAt(line_number_))));
  button_->GetViewAccessibility().SetPopupFocusOverride();
  button_->NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
  views::InkDrop::Get(button_->ink_drop_view())->GetInkDrop()->SetHovered(true);
  UpdateSelectedAndRunCallback(false);
}

void PopupAutocompleteCellView::HandleKeyPressEventFocusOnContent() {
  button_focused_ = false;

  // TODO(crbug.com/1417187): Find out the root cause for the necessity of this
  // workaround. Without explicitly removing the accessible name for the button,
  // the screen reader is announcing both the content and the delete button (on
  // MAC). For example, if the content is "jondoe@gmail.com", the screen reader
  // announces "delete jon". This does not happen if the button has no
  // accessible name.
  button_->SetAccessibleName(u"");
  UpdateSelectedAndRunCallback(true);
  GetViewAccessibility().SetPopupFocusOverride();
  NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
  views::InkDrop::Get(button_->ink_drop_view())
      ->GetInkDrop()
      ->SetHovered(false);
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

  // TODO(crbug.com/1417187): Find out the root cause for the necessity of this
  // workaround. Without explicitly removing the accessible name for the button
  // the screen reader is announcing both the content and the delete button (on
  // MAC). For example, if the content is "jondoe@gmail.com", the screen reader
  // announces "delete jon". This does not happen if the button has no
  // accessible name.
  button_->SetVisible(selected);
  button_->SetAccessibleName(u"");

  autofill::PopupCellView::SetSelected(selected);

  // There are cases where SetSelect is called with the same value as before
  // but we still want to refresh the style. For example in the case where there
  // is an arrow navigation from the delete button to the next cell. In this
  // case we go directly from selected = false (button was selected) to again
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

  CHECK(GetLayoutManager());
  views::BoxLayout* layout = static_cast<views::BoxLayout*>(GetLayoutManager());
  // We are making sure that the vertical distance from the delete button edges
  // to the cell border is the same as the horizontal distance.
  // 1. Take the current horizontal distance.
  int horizontal_margin = layout->inside_border_insets().right();
  // 2. Take the height of the cell.
  int cell_height = layout->minimum_cross_axis_size();
  // 3. The diameter needs to be the height - 2 * the desired margin.
  int radius = (cell_height - horizontal_margin * 2) / 2;
  InstallFixedSizeCircleHighlightPathGenerator(button.get(), radius);
  button->SetPreferredSize(gfx::Size(radius * 2, radius * 2));
  button->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_DELETE_AUTOCOMPLETE_SUGGESTION_TOOLTIP));
  button->SetAccessibleRole(ax::mojom::Role::kMenuItem);
  button->SetVisible(false);

  // Make child view grow to fill the space and align the button to the right.
  for (views::View* child : children()) {
    layout->SetFlexForView(child, 1);
  }

  button_placeholder_ = AddChildView(std::make_unique<ButtonPlaceholder>(this));
  button_placeholder_->SetLayoutManager(std::make_unique<views::BoxLayout>());
  button_placeholder_->SetPreferredSize(button->GetPreferredSize());

  button_ = button_placeholder_->AddChildView(std::move(button));
  layout->SetFlexForView(button_placeholder_, 0);

  button_->SetButtonController(std::make_unique<DeleteButtonController>(
      button_, this,
      std::make_unique<views::Button::DefaultButtonControllerDelegate>(
          button_.get())));
}

void PopupAutocompleteCellView::DeleteAutocomplete() {
  if (controller_ && controller_->RemoveSuggestion(line_number_)) {
    AutofillMetrics::OnAutocompleteSuggestionDeleted(
        AutofillMetrics::AutocompleteSingleEntryRemovalMethod::
            kDeleteButtonClicked);
  }
}

void PopupAutocompleteCellView::OnMouseEnteredDeleteButton() {
  UpdateSelectedAndRunCallback(/*selected=*/false);
}

void PopupAutocompleteCellView::OnMouseExitedDeleteButton() {
  // We check for IsMouseHovered() because moving too fast outside the button
  // could place the mouse cursor outside the whole cell.
  UpdateSelectedAndRunCallback(/*selected=*/IsMouseHovered());
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
