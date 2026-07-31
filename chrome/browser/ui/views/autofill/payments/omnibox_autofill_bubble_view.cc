// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/omnibox_autofill_bubble_view.h"

#include <string>

#include "chrome/browser/ui/autofill/payments/omnibox_autofill_bubble_controller.h"
#include "chrome/browser/ui/views/autofill/payments/omnibox_autofill_suggestion_view.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_content_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_factory_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/display/screen.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace autofill {

OmniboxAutofillBubbleView::OmniboxAutofillBubbleView(
    views::BubbleAnchor anchor_view,
    content::WebContents* web_contents,
    OmniboxAutofillBubbleController* controller)
    : AutofillLocationBarBubble(anchor_view, web_contents),
      controller_(controller->GetWeakPtr()) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetShowCloseButton(true);

  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  GetViewAccessibility().SetRole(ax::mojom::Role::kDialog);
  GetViewAccessibility().SetName(GetWindowTitle());
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

  // Set `scroll_view_` after the title view is created that way
  // `GetMaxScrollViewHeight()` can take it into account.
  if (scroll_view_) {
    scroll_view_->ClipHeightTo(0, GetMaxScrollViewHeight());
  }
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

  auto suggestions_container = std::make_unique<views::BoxLayoutView>();
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

    auto suggestion_button = std::make_unique<OmniboxAutofillSuggestion>(
        std::move(content_view), suggestion.main_text.value,
        base::BindRepeating(&OmniboxAutofillBubbleView::OnSuggestionAccepted,
                            base::Unretained(this), suggestion, row_index),
        base::BindRepeating(&OmniboxAutofillBubbleView::OnSuggestionSelected,
                            base::Unretained(this), suggestion),
        base::BindRepeating(&OmniboxAutofillBubbleView::OnSuggestionDeselected,
                            weak_ptr_factory_.GetWeakPtr()));
    suggestions_container->AddChildView(std::move(suggestion_button));
    row_index++;
  }

  // Dynamically size bubble width: default to preferred bubble width plus an
  // adjustment as minimum to prevent the "Choose payment method" title from
  // wrapping (and giving proper space between the title and GPay logo, if
  // shown). Expand to fit the content's preferred width if suggestions require
  // more space (e.g. card benefits are displayed).
  const int width_adjustment = 30;
  int min_bubble_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                             views::DISTANCE_BUBBLE_PREFERRED_WIDTH) +
                         width_adjustment;
  int content_preferred_width =
      suggestions_container->GetPreferredSize().width() + margins().width();
  set_fixed_width(std::max(min_bubble_width, content_preferred_width));

  auto scroll_view = std::make_unique<views::ScrollView>();
  scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  scroll_view->SetContents(std::move(suggestions_container));
  scroll_view_ = AddChildView(std::move(scroll_view));
}

int OmniboxAutofillBubbleView::GetMaxBubbleHeight() const {
  // Get the omnibox chip position.
  gfx::Rect anchor_rect = GetAnchorRect();

  // Get the usable monitor display area excluding OS taskbars/docks.
  gfx::Rect work_area = display::Screen::Get()
                            ->GetDisplayNearestPoint(anchor_rect.CenterPoint())
                            .work_area();

  // Calculate available space between `anchor_rect` and `work_area`.
  int available_height = work_area.bottom() - anchor_rect.bottom();

  // Cap available height by the active tab's web contents area height. This
  // prevents the bubble popup from stretching past the browser window's
  // vertical bounds when Chrome is in windowed mode.
  if (web_contents()) {
    available_height = std::min(available_height,
                                web_contents()->GetContainerBounds().height());
  }

  return available_height;
}

int OmniboxAutofillBubbleView::GetMaxScrollViewHeight() const {
  // Calculate non-scrollable overhead dynamically from `Views` components.
  int overhead = margins().height();
  if (const views::BubbleFrameView* frame = GetBubbleFrameView()) {
    overhead += frame->GetInsets().height();
    if (const views::View* title_view = frame->title()) {
      overhead += title_view->GetPreferredSize().height();
    }
  }

  return GetMaxBubbleHeight() - overhead;
}

void OmniboxAutofillBubbleView::OnSuggestionAccepted(
    const Suggestion& suggestion,
    size_t row_index) {
  if (controller_) {
    controller_->OnSuggestionAccepted(suggestion, row_index);
    CloseBubble();
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
