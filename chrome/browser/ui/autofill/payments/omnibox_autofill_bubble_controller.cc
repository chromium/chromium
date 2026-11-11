// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/omnibox_autofill_bubble_controller.h"

#include <algorithm>
#include <utility>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/autofill/payments/omnibox_autofill_page_action_controller.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/ui/payments/payments_ui_closed_reasons.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace autofill {

namespace {

SuggestionHidingReason ToSuggestionHidingReason(PaymentsUiClosedReason reason) {
  switch (reason) {
    case PaymentsUiClosedReason::kAccepted:
      return SuggestionHidingReason::kAcceptSuggestion;
    case PaymentsUiClosedReason::kCancelled:
    case PaymentsUiClosedReason::kClosed:
      return SuggestionHidingReason::kUserAborted;
    case PaymentsUiClosedReason::kLostFocus:
      return SuggestionHidingReason::kFocusChanged;
    case PaymentsUiClosedReason::kNotInteracted:
    case PaymentsUiClosedReason::kUnknown:
      return SuggestionHidingReason::kUserAborted;
  }
}

}  // namespace

DEFINE_USER_DATA(OmniboxAutofillBubbleController);

OmniboxAutofillBubbleController::OmniboxAutofillBubbleController(
    tabs::TabInterface* tab_interface)
    : AutofillBubbleControllerBase(
          tab_interface,
          PageActionConfig{.action_id = kActionAutofillPayment,
                           .should_sync_bubble_showing_state = true}),
      scoped_unowned_user_data_(
          CHECK_DEREF(tab_interface).GetUnownedUserDataHost(),
          *this),
      will_discard_contents_subscription_(
          tab_interface->RegisterWillDiscardContents(base::BindRepeating(
              &OmniboxAutofillBubbleController::WillDiscardContents,
              base::Unretained(this)))),
      payments_data_manager_(PersonalDataManagerFactory::GetForBrowserContext(
                                 web_contents()->GetBrowserContext())
                                 ->payments_data_manager()) {}

OmniboxAutofillBubbleController::~OmniboxAutofillBubbleController() = default;

void OmniboxAutofillBubbleController::Initialize(
    std::vector<Suggestion> suggestions,
    base::RepeatingCallback<void(base::span<const Suggestion>)>
        on_suggestions_shown,
    base::RepeatingCallback<void(SuggestionHidingReason)> on_suggestions_hidden,
    base::RepeatingCallback<void(const Suggestion&)> did_select_suggestion,
    base::RepeatingClosure did_deselect_suggestion,
    base::RepeatingCallback<
        void(const Suggestion&,
             const AutofillSuggestionDelegate::SuggestionMetadata&)>
        did_accept_suggestion) {
  suggestions_ = std::move(suggestions);
  on_suggestions_shown_callback_ = std::move(on_suggestions_shown);
  on_suggestions_hidden_callback_ = std::move(on_suggestions_hidden);
  did_select_suggestion_callback_ = std::move(did_select_suggestion);
  did_deselect_suggestion_callback_ = std::move(did_deselect_suggestion);
  did_accept_suggestion_callback_ = std::move(did_accept_suggestion);

  // If any cards are server cards, ensure the GPay logo is shown. Determined
  // here at initialization time because GUIDs may update/refresh during the
  // autofill flow.
  // TODO(crbug.com/527195586): The underlying fix should be to use instrument
  // IDs rather than GUIDs.
  should_show_google_pay_logo_ =
      std::ranges::any_of(suggestions_, [this](const Suggestion& suggestion) {
        const CreditCard* const credit_card =
            payments_data_manager_->GetCreditCardByGUID(
                suggestion.GetPayload<Suggestion::Guid>().value());
        return credit_card && payments_data_manager_->IsServerCard(credit_card);
      });
}

// static
OmniboxAutofillBubbleController* OmniboxAutofillBubbleController::From(
    tabs::TabInterface& tab_interface) {
  return Get(tab_interface.GetUnownedUserDataHost());
}

BubbleType OmniboxAutofillBubbleController::GetBubbleType() const {
  return BubbleType::kOmniboxAutofill;
}

base::WeakPtr<BubbleControllerBase>
OmniboxAutofillBubbleController::GetBubbleControllerBaseWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void OmniboxAutofillBubbleController::DoShowBubble() {
  BrowserWindowInterface* browser =
      GetTabInterface()->GetBrowserWindowInterface();
  if (!browser) {
    return;
  }
  BrowserWindow* browser_window = BrowserWindow::FromBrowser(browser);
  if (!browser_window) {
    return;
  }
  if (AutofillBubbleBase* bubble_view =
          browser_window->GetAutofillBubbleHandler()->ShowOmniboxAutofillBubble(
              web_contents(), this)) {
    SetBubbleView(*bubble_view);
  }
}

AutofillBubbleBase* OmniboxAutofillBubbleController::GetBubbleView() const {
  return bubble_view();
}

std::u16string OmniboxAutofillBubbleController::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_OMNIBOX_BUBBLE_TITLE);
}

const std::vector<Suggestion>& OmniboxAutofillBubbleController::GetSuggestions()
    const {
  return suggestions_;
}

bool OmniboxAutofillBubbleController::ShouldShowGooglePayLogo() const {
  return should_show_google_pay_logo_;
}

void OmniboxAutofillBubbleController::OnSuggestionsShown() {
  if (on_suggestions_shown_callback_) {
    on_suggestions_shown_callback_.Run(suggestions_);
  }
}

void OmniboxAutofillBubbleController::OnSuggestionDeselected() {
  if (did_deselect_suggestion_callback_) {
    did_deselect_suggestion_callback_.Run();
  }
}

void OmniboxAutofillBubbleController::OnBubbleClosed(
    PaymentsUiClosedReason reason) {
  if (on_suggestions_hidden_callback_) {
    on_suggestions_hidden_callback_.Run(ToSuggestionHidingReason(reason));
  }

  // When the bubble is closed without accepting a suggestion, collapse the
  // expanded omnibox chip down to icon-only mode.
  if (reason != PaymentsUiClosedReason::kAccepted) {
    if (OmniboxAutofillPageActionController* page_action_controller =
            OmniboxAutofillPageActionController::From(*GetTabInterface())) {
      page_action_controller->ShowCollapsedChip();
    }
  }

  ResetBubbleViewAndInformBubbleManager();
}

void OmniboxAutofillBubbleController::OnSuggestionSelected(
    const Suggestion& suggestion) {
  if (did_select_suggestion_callback_) {
    did_select_suggestion_callback_.Run(suggestion);
  }
}

void OmniboxAutofillBubbleController::OnSuggestionAccepted(
    const Suggestion& suggestion,
    size_t row) {
  if (did_accept_suggestion_callback_) {
    AutofillSuggestionDelegate::SuggestionMetadata metadata{
        .multi_index = {row}};
    did_accept_suggestion_callback_.Run(suggestion, metadata);
  }
}

base::WeakPtr<OmniboxAutofillBubbleController>
OmniboxAutofillBubbleController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void OmniboxAutofillBubbleController::WillDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  HideBubble(/*initiated_by_bubble_manager=*/false);
  suggestions_.clear();
  on_suggestions_shown_callback_.Reset();
  on_suggestions_hidden_callback_.Reset();
  did_select_suggestion_callback_.Reset();
  did_deselect_suggestion_callback_.Reset();
  did_accept_suggestion_callback_.Reset();
  should_show_google_pay_logo_ = false;
  if (OmniboxAutofillPageActionController* const page_action_controller =
          OmniboxAutofillPageActionController::From(*GetTabInterface())) {
    page_action_controller->HideChip();
  }
}

}  // namespace autofill
