// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_row_with_button_view.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/rtl.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_content_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/strings/grit/components_strings.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
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
// `views::ButtonController`. Used by `PopupRowWithButtonView` to know when
// the mouse cursor has entered or left the delete button in order to run the
// selection callbacks.
class ButtonController : public views::ButtonController {
 public:
  ButtonController(views::Button* button,
                   ButtonDelegate* button_delegate,
                   std::unique_ptr<views::ButtonControllerDelegate> delegate)
      : views::ButtonController(button, std::move(delegate)),
        button_delegate_(button_delegate) {}

  ~ButtonController() override = default;

  void OnMouseEntered(const ui::MouseEvent& event) override {
    button_delegate_->OnMouseEnteredButton();
    views::ButtonController::OnMouseEntered(event);
  }

  void OnMouseExited(const ui::MouseEvent& event) override {
    button_delegate_->OnMouseExitedButton();
    views::ButtonController::OnMouseExited(event);
  }

 private:
  raw_ptr<ButtonDelegate> button_delegate_ = nullptr;
};

// Used to determine when both the placeholder and the button are painted
// and have dimensions. This is important to solve the issue where deleting an
// entry leads to another entry being rendered right under the cursor.
class ButtonPlaceholder : public views::View, public views::ViewObserver {
  METADATA_HEADER(ButtonPlaceholder, views::View)

 public:
  explicit ButtonPlaceholder(ButtonDelegate* button_delegate);
  ~ButtonPlaceholder() override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;

 private:
  // Scoped observation for OnViewBoundsChanged.
  base::ScopedObservation<views::View, views::ViewObserver>
      view_bounds_changed_observer_{this};
  const raw_ptr<ButtonDelegate> button_delegate_ = nullptr;
  bool first_paint_happened_ = false;
};

ButtonPlaceholder::ButtonPlaceholder(ButtonDelegate* button_delegate)
    : button_delegate_(button_delegate) {}

ButtonPlaceholder::~ButtonPlaceholder() = default;

void ButtonPlaceholder::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);
  if (first_paint_happened_) {
    return;
  }

  // The button placeholder should have only the button as child.
  CHECK(children().size() == 1);
  // The placeholder also always have a parent, the row view.
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
  // simply call `SynthesizeMouseMoveEvent()` because it calls the row view's
  // `OnMouseEntered` method after we highlight the button, leading the an
  // incorrect state.
  View* delete_button = children()[0];
  if (observed_view == delete_button && observed_view->GetVisible()) {
    GetWidget()->SynthesizeMouseMoveEvent();
    if (observed_view->IsMouseHovered()) {
      button_delegate_->OnMouseEnteredButton();
      view_bounds_changed_observer_.Reset();
    }
  }
}

BEGIN_METADATA(ButtonPlaceholder)
END_METADATA

}  // namespace

PopupRowWithButtonView::PopupRowWithButtonView(
    AccessibilitySelectionDelegate& a11y_selection_delegate,
    SelectionDelegate& selection_delegate,
    base::WeakPtr<AutofillPopupController> controller,
    int line_number,
    std::unique_ptr<PopupRowContentView> content_view,
    std::unique_ptr<views::ImageButton> button,
    ButtonVisibility button_visibility,
    ButtonSelectBehavior button_select_behavior)
    : PopupRowView(a11y_selection_delegate,
                   selection_delegate,
                   controller,
                   line_number,
                   std::move(content_view)),
      button_visibility_(button_visibility),
      button_select_behavior_(button_select_behavior) {
  auto* content_layout =
      static_cast<views::BoxLayout*>(GetContentView().GetLayoutManager());
  // A spacer between the other children of the content view and the button.
  // We do not use between child spacing since because the content view may
  // have multiple children that we do not wish to affect.
  const int spacer_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RELATED_LABEL_HORIZONTAL_LIST);
  content_layout->SetFlexForView(
      GetContentView().AddChildView(
          views::Builder<views::View>()
              .SetPreferredSize(gfx::Size(spacer_width, 1))
              .Build()),
      0,
      /*use_min_size=*/true);

  CHECK(button);
  button_placeholder_ =
      GetContentView().AddChildView(std::make_unique<ButtonPlaceholder>(this));
  button_placeholder_->SetLayoutManager(std::make_unique<views::BoxLayout>());
  button_placeholder_->SetPreferredSize(button->GetPreferredSize());
  button_ = button_placeholder_->AddChildView(std::move(button));
  button_->SetVisible(ShouldButtonBeVisible());
  button_->GetViewAccessibility().SetIsIgnored(true);
  button_->SetButtonController(std::make_unique<ButtonController>(
      button_, this,
      std::make_unique<views::Button::DefaultButtonControllerDelegate>(
          button_.get())));
  content_layout->SetFlexForView(button_placeholder_, 0);
}

PopupRowWithButtonView::~PopupRowWithButtonView() = default;

views::View* PopupRowWithButtonView::GetButtonContainer() {
  return button_placeholder_;
}

void PopupRowWithButtonView::HandleKeyPressEventFocusOnButton() {
  button_->GetViewAccessibility().SetPopupFocusOverride();
  button_->NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
  views::InkDrop::Get(button_->ink_drop_view())->GetInkDrop()->SetHovered(true);
  UpdateFocusedPartAndSelectedSuggestion(RowWithButtonPart::kButton);
}

void PopupRowWithButtonView::HandleKeyPressEventFocusOnContent() {
  UpdateFocusedPartAndSelectedSuggestion(RowWithButtonPart::kContent);
  GetContentView().GetViewAccessibility().SetPopupFocusOverride();
  NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
  views::InkDrop::Get(button_->ink_drop_view())
      ->GetInkDrop()
      ->SetHovered(false);
}

bool PopupRowWithButtonView::HandleKeyPressEvent(
    const input::NativeWebKeyboardEvent& event) {
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
      if (focused_part_ == RowWithButtonPart::kButton) {
        button_->button_controller()->NotifyClick();
        return true;
      }
      break;
    default:
      break;
  }

  return PopupRowView::HandleKeyPressEvent(event);
}

void PopupRowWithButtonView::SetSelectedCell(std::optional<CellType> cell) {
  autofill::PopupRowView::SetSelectedCell(cell);

  button_->SetVisible(ShouldButtonBeVisible());

  // We also update `focused_part_`: it could be set to `kContent` or unfocused
  // from this method, as the method is used by `PopupViewViews`.
  focused_part_ = (cell == CellType::kContent)
                      ? std::optional(RowWithButtonPart::kContent)
                      : std::nullopt;
}

void PopupRowWithButtonView::OnMouseEnteredButton() {
  UpdateFocusedPartAndSelectedSuggestion(RowWithButtonPart::kButton);
}

void PopupRowWithButtonView::OnMouseExitedButton() {
  // We check for IsMouseHovered() because moving too fast outside the button
  // could place the mouse cursor outside the whole cell.
  UpdateFocusedPartAndSelectedSuggestion(
      GetContentView().IsMouseHovered()
          ? std::optional(RowWithButtonPart::kContent)
          : std::nullopt);
}

void PopupRowWithButtonView::UpdateFocusedPartAndSelectedSuggestion(
    std::optional<RowWithButtonPart> part) {
  if (focused_part_ == part) {
    return;
  }

  if (!controller()) {
    return;
  }

  focused_part_ = part;
  if (focused_part_ == RowWithButtonPart::kContent) {
    controller()->SelectSuggestion(line_number());
  } else if (button_select_behavior_ ==
             ButtonSelectBehavior::kUnselectSuggestion) {
    controller()->UnselectSuggestion();
  }
}

bool PopupRowWithButtonView::ShouldButtonBeVisible() const {
  switch (button_visibility_) {
    case ButtonVisibility::kShowOnHoverOrSelect:
      return GetSelectedCell() == CellType::kContent;
    case ButtonVisibility::kShowAlways:
      return true;
  }
}

BEGIN_METADATA(PopupRowWithButtonView)
END_METADATA

}  // namespace autofill
