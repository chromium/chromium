// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_cell_with_button_view.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/common/input/native_web_keyboard_event.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace autofill {

namespace {

// Overrides `OnMouseEntered` and `OnMouseExited` from
// `views::ButtonController`. Used by `PopupCellWithButtonView` to know when
// the mouse cursor has entered or left the delete button in order to run the
// selection callbacks.
class CellButtonController : public views::ButtonController {
 public:
  CellButtonController(
      views::Button* button,
      CellButtonDelegate* cell_button_delegate,
      std::unique_ptr<views::ButtonControllerDelegate> delegate)
      : views::ButtonController(button, std::move(delegate)),
        cell_button_delegate_(cell_button_delegate) {}

  ~CellButtonController() override = default;

  void OnMouseEntered(const ui::MouseEvent& event) override {
    cell_button_delegate_->OnMouseEnteredCellButton();
    views::ButtonController::OnMouseEntered(event);
  }

  void OnMouseExited(const ui::MouseEvent& event) override {
    cell_button_delegate_->OnMouseExitedCellButton();
    views::ButtonController::OnMouseExited(event);
  }

 private:
  raw_ptr<CellButtonDelegate> cell_button_delegate_ = nullptr;
};

ButtonPlaceholder::ButtonPlaceholder(CellButtonDelegate* cell_button_delegate)
    : cell_button_delegate_(cell_button_delegate) {}

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
      cell_button_delegate_->OnMouseEnteredCellButton();
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

PopupCellWithButtonView::PopupCellWithButtonView(
    base::WeakPtr<AutofillPopupController> controller,
    int line_number)
    : controller_(controller), line_number_(line_number) {}

PopupCellWithButtonView::~PopupCellWithButtonView() = default;

void PopupCellWithButtonView::SetCellButton(
    std::unique_ptr<views::ImageButton> cell_button) {
  if (button_placeholder_) {
    button_ = nullptr;
    RemoveChildView(std::exchange(button_placeholder_, nullptr).get());
  }

  if (!cell_button) {
    return;
  }

  button_placeholder_ = AddChildView(std::make_unique<ButtonPlaceholder>(this));
  button_placeholder_->SetLayoutManager(std::make_unique<views::BoxLayout>());
  button_placeholder_->SetPreferredSize(cell_button->GetPreferredSize());
  button_ = button_placeholder_->AddChildView(std::move(cell_button));
  button_->SetVisible(ShouldCellButtonBeVisible());
  button_->GetViewAccessibility().OverrideIsIgnored(true);
  button_->SetButtonController(std::make_unique<CellButtonController>(
      button_, this,
      std::make_unique<views::Button::DefaultButtonControllerDelegate>(
          button_.get())));
}

void PopupCellWithButtonView::SetCellButtonBehavior(
    CellButtonBehavior cell_button_behavior) {
  cell_button_behavior_ = cell_button_behavior;
  if (button_) {
    button_->SetVisible(ShouldCellButtonBeVisible());
  }
}

void PopupCellWithButtonView::HandleKeyPressEventFocusOnButton() {
  if (!button_) {
    return;
  }

  button_focused_ = true;
  button_->GetViewAccessibility().SetPopupFocusOverride();
  button_->NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
  views::InkDrop::Get(button_->ink_drop_view())->GetInkDrop()->SetHovered(true);
  UpdateSelectedAndRunCallback(false);
}

void PopupCellWithButtonView::HandleKeyPressEventFocusOnContent() {
  button_focused_ = false;
  if (!button_) {
    return;
  }

  UpdateSelectedAndRunCallback(true);
  GetViewAccessibility().SetPopupFocusOverride();
  NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
  views::InkDrop::Get(button_->ink_drop_view())
      ->GetInkDrop()
      ->SetHovered(false);
}

bool PopupCellWithButtonView::HandleKeyPressEvent(
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
      if (button_ && button_focused_) {
        button_->button_controller()->NotifyClick();
        return true;
      }
      break;
    default:
      break;
  }
  return PopupCellView::HandleKeyPressEvent(event);
}

void PopupCellWithButtonView::SetSelected(bool selected) {
  autofill::PopupCellView::SetSelected(selected);
  if (button_) {
    button_->SetVisible(ShouldCellButtonBeVisible());
  }

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
  // mouse navigation. This prevents the case where the delete button is focused
  // but the cursor is moved to the content.
  button_focused_ = false;
}

void PopupCellWithButtonView::OnMouseEnteredCellButton() {
  UpdateSelectedAndRunCallback(/*selected=*/false);
}

void PopupCellWithButtonView::OnMouseExitedCellButton() {
  // We check for IsMouseHovered() because moving too fast outside the button
  // could place the mouse cursor outside the whole cell.
  UpdateSelectedAndRunCallback(/*selected=*/IsMouseHovered());
}

void PopupCellWithButtonView::UpdateSelectedAndRunCallback(bool selected) {
  if (selected_ == selected) {
    return;
  }

  selected_ = selected;
  if (controller_) {
    controller_->SelectSuggestion(
        selected_ ? absl::optional<size_t>(line_number_) : absl::nullopt);
  }
}

bool PopupCellWithButtonView::ShouldCellButtonBeVisible() const {
  switch (cell_button_behavior_) {
    case CellButtonBehavior::kShowOnHoverOrSelect:
      return selected_;
    case CellButtonBehavior::kShowAlways:
      return true;
  }
}

BEGIN_METADATA(PopupCellWithButtonView, autofill::PopupCellView)
END_METADATA

}  // namespace autofill
