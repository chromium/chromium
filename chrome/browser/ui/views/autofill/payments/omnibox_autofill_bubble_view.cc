// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/omnibox_autofill_bubble_view.h"

#include <string>

#include "chrome/browser/ui/autofill/payments/omnibox_autofill_bubble_controller.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_content_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_factory_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"

namespace autofill {

namespace {

class SuggestionButton : public views::Button {
  METADATA_HEADER(SuggestionButton, views::Button)
 public:
  SuggestionButton(std::unique_ptr<PopupRowContentView> content_view,
                   const std::u16string& accessible_name,
                   PressedCallback pressed_callback,
                   base::RepeatingClosure selected_callback,
                   base::RepeatingClosure deselection_callback)
      : views::Button(std::move(pressed_callback)),
        content_view_(AddChildView(std::move(content_view))),
        selected_callback_(std::move(selected_callback)),
        deselection_callback_(std::move(deselection_callback)) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    SetFocusBehavior(FocusBehavior::ALWAYS);
    SetRequestFocusOnPress(true);
    SetAccessibleName(accessible_name);
    SetNotifyEnterExitOnChild(true);
  }

  // views::View:
  void OnFocus() override {
    views::Button::OnFocus();
    UpdateSelectionState(true);
  }
  void OnBlur() override {
    views::Button::OnBlur();
    UpdateSelectionState(false);
  }

 protected:
  // views::Button:
  void StateChanged(ButtonState old_state) override {
    views::Button::StateChanged(old_state);
    bool selected = GetState() == STATE_HOVERED ||
                    GetState() == STATE_PRESSED || HasFocus();
    UpdateSelectionState(selected);
  }

 private:
  void UpdateSelectionState(bool selected) {
    if (selected_ != selected) {
      selected_ = selected;
      content_view_->UpdateStyle(selected);
      if (selected_) {
        RequestFocus();
        if (selected_callback_) {
          selected_callback_.Run();
        }
      } else {
        if (deselection_callback_) {
          deselection_callback_.Run();
        }
      }
    }
  }

  raw_ptr<PopupRowContentView> content_view_;
  base::RepeatingClosure selected_callback_;
  base::RepeatingClosure deselection_callback_;
  bool selected_ = false;
};

BEGIN_METADATA(SuggestionButton)
END_METADATA

}  // namespace

OmniboxAutofillBubbleView::OmniboxAutofillBubbleView(
    views::BubbleAnchor anchor_view,
    content::WebContents* web_contents,
    OmniboxAutofillBubbleController* controller)
    : AutofillLocationBarBubble(anchor_view, web_contents),
      controller_(controller->GetWeakPtr()) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetShowCloseButton(true);
  // Adjust the width to prevent the "Choose payment method" title from wrapping
  // and to keep long text, like card benefits, fully visible.
  const int width_adjustment = 50;
  set_fixed_width(ChromeLayoutProvider::Get()->GetDistanceMetric(
                      views::DISTANCE_BUBBLE_PREFERRED_WIDTH) +
                  width_adjustment);
}

OmniboxAutofillBubbleView::~OmniboxAutofillBubbleView() = default;

void OmniboxAutofillBubbleView::Show(DisplayReason reason) {
  ShowForReason(reason);
  if (controller_) {
    controller_->OnSuggestionsShown();
  }
}

void OmniboxAutofillBubbleView::Hide() {
  CloseBubble();
  WindowClosing();
}

std::u16string OmniboxAutofillBubbleView::GetWindowTitle() const {
  return controller_ ? controller_->GetWindowTitle() : std::u16string();
}

void OmniboxAutofillBubbleView::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed(
        GetPaymentsUiClosedReasonFromWidget(GetWidget()));
    controller_ = nullptr;
  }
}

void OmniboxAutofillBubbleView::AddedToWidget() {
  if (controller_ && controller_->ShouldShowGooglePayLogo()) {
    GetBubbleFrameView()->SetTitleView(
        std::make_unique<TitleWithIconAfterLabelView>(
            GetWindowTitle(), TitleWithIconAfterLabelView::Icon::GOOGLE_PAY));
  } else {
    auto title_view = std::make_unique<views::Label>(
        GetWindowTitle(), views::style::CONTEXT_DIALOG_TITLE);
    title_view->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
    title_view->SetMultiLine(true);
    GetBubbleFrameView()->SetTitleView(std::move(title_view));
  }
}

views::View* OmniboxAutofillBubbleView::GetInitiallyFocusedView() {
  return initially_focused_view_;
}

void OmniboxAutofillBubbleView::Init() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  if (!controller_) {
    return;
  }

  const std::vector<Suggestion>& suggestions = controller_->GetSuggestions();
  if (suggestions.empty()) {
    return;
  }

  auto* suggestions_container =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  suggestions_container->SetOrientation(
      views::BoxLayout::Orientation::kVertical);
  suggestions_container->SetBetweenChildSpacing(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL));

  size_t row_index = 0;
  for (const auto& suggestion : suggestions) {
    std::unique_ptr<PopupRowContentView> content_view;
    if (suggestion.type == SuggestionType::kVirtualCreditCardEntry) {
      content_view = CreateAlternativePaymentMethodPopupRowContentView(
          suggestion, /*show_new_badge=*/std::nullopt,
          FillingProduct::kCreditCard, /*filter_match=*/std::nullopt);
    } else {
      content_view = CreatePopupRowContentView(suggestion,
                                               /*show_new_badge=*/std::nullopt,
                                               FillingProduct::kCreditCard,
                                               /*filter_match=*/std::nullopt);
    }

    auto suggestion_button = std::make_unique<SuggestionButton>(
        std::move(content_view), suggestion.main_text.value,
        base::BindRepeating(&OmniboxAutofillBubbleView::OnSuggestionAccepted,
                            base::Unretained(this), suggestion, row_index),
        base::BindRepeating(&OmniboxAutofillBubbleView::OnSuggestionSelected,
                            base::Unretained(this), suggestion),
        base::BindRepeating(&OmniboxAutofillBubbleView::OnSuggestionDeselected,
                            weak_ptr_factory_.GetWeakPtr()));
    // Ensures the first suggestion is initially focused.
    if (!initially_focused_view_) {
      initially_focused_view_ = suggestion_button.get();
    }
    suggestions_container->AddChildView(std::move(suggestion_button));
    row_index++;
  }
}

void OmniboxAutofillBubbleView::OnSuggestionAccepted(
    const Suggestion& suggestion,
    size_t row_index) {
  if (controller_) {
    controller_->OnSuggestionAccepted(suggestion, row_index);
  }
}

void OmniboxAutofillBubbleView::OnSuggestionSelected(
    const Suggestion& suggestion) {
  if (controller_) {
    controller_->OnSuggestionSelected(suggestion);
  }
}

void OmniboxAutofillBubbleView::OnSuggestionDeselected() {
  if (controller_) {
    controller_->OnSuggestionDeselected();
  }
}

BEGIN_METADATA(OmniboxAutofillBubbleView)
END_METADATA

}  // namespace autofill
