// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/touch_to_fill_delegate_impl.h"

#include "components/autofill/core/browser/autofill_browser_util.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_util.h"

namespace autofill {

TouchToFillDelegateImpl::TouchToFillDelegateImpl(
    BrowserAutofillManager* manager)
    : manager_(manager) {
  DCHECK(manager);
}

TouchToFillDelegateImpl::~TouchToFillDelegateImpl() {
  // Invalidate pointers to avoid post hide callbacks.
  weak_ptr_factory_.InvalidateWeakPtrs();
  HideTouchToFill();
}

bool TouchToFillDelegateImpl::TryToShowTouchToFill(const FormData& form,
                                                   const FormFieldData& field) {
  // TODO(crbug.com/1386143): store only FormGlobalId and FieldGlobalId instead
  // to avoid that FormData and FormFieldData may become obsolete during the
  // bottomsheet being open.
  query_form_ = form;
  query_field_ = field;
  // Trigger only for a credit card field/form.
  // TODO(crbug.com/1247698): Clarify field/form requirements.
  if (manager_->GetPopupType(form, field) != PopupType::kCreditCards)
    return false;
  // Trigger only on supported platforms.
  if (!manager_->client()->IsTouchToFillCreditCardSupported())
    return false;
  // Trigger only if the client and the form are not insecure.
  if (IsFormOrClientNonSecure(manager_->client(), form))
    return false;
  // Trigger only if not shown before.
  if (ttf_credit_card_state_ != TouchToFillState::kShouldShow)
    return false;
  // Trigger only on focusable empty field.
  if (!field.is_focusable || !SanitizedFieldIsEmpty(field.value))
    return false;
  // Trigger only if there is at least 1 complete valid credit card on file.
  // Complete = contains number, expiration date and name on card.
  // Valid = unexpired with valid number format.
  PersonalDataManager* pdm = manager_->client()->GetPersonalDataManager();
  DCHECK(pdm);
  std::vector<CreditCard*> cards_to_suggest = pdm->GetCreditCardsToSuggest(
      manager_->client()->AreServerCardsSupported());

  base::EraseIf(cards_to_suggest,
                base::not_fn(&CreditCard::IsCompleteValidCard));
  if (cards_to_suggest.empty())
    return false;
  // Trigger only if the UI is available.
  if (!manager_->driver()->CanShowAutofillUi())
    return false;
  // Finally try showing the surface.
  if (!manager_->client()->ShowTouchToFillCreditCard(
          GetWeakPtr(), std::move(cards_to_suggest))) {
    return false;
  }

  ttf_credit_card_state_ = TouchToFillState::kIsShowing;
  manager_->client()->HideAutofillPopup(
      PopupHidingReason::kOverlappingWithTouchToFillSurface);
  return true;
}

bool TouchToFillDelegateImpl::IsShowingTouchToFill() {
  return ttf_credit_card_state_ == TouchToFillState::kIsShowing;
}

// TODO(crbug.com/1348538): Create a central point for TTF hiding decision.
void TouchToFillDelegateImpl::HideTouchToFill() {
  if (IsShowingTouchToFill()) {
    manager_->client()->HideTouchToFillCreditCard();
    ttf_credit_card_state_ = TouchToFillState::kWasShown;
  }
}

void TouchToFillDelegateImpl::Reset() {
  HideTouchToFill();
  ttf_credit_card_state_ = TouchToFillState::kShouldShow;
}

AutofillDriver* TouchToFillDelegateImpl::GetDriver() {
  return manager_->driver();
}

bool TouchToFillDelegateImpl::ShouldShowScanCreditCard() {
  if (!manager_->client()->HasCreditCardScanFeature())
    return false;

  return !IsFormOrClientNonSecure(manager_->client(), query_form_);
}

void TouchToFillDelegateImpl::ScanCreditCard() {
  manager_->client()->ScanCreditCard(base::BindOnce(
      &TouchToFillDelegateImpl::OnCreditCardScanned, GetWeakPtr()));
}

void TouchToFillDelegateImpl::OnCreditCardScanned(const CreditCard& card) {
  HideTouchToFill();
  manager_->FillCreditCardFormImpl(query_form_, query_field_, card,
                                   std::u16string());
}

void TouchToFillDelegateImpl::ShowCreditCardSettings() {
  HideTouchToFill();
  manager_->client()->ShowAutofillSettings(/*show_credit_card_settings=*/true);
}

void TouchToFillDelegateImpl::SuggestionSelected(std::string unique_id) {
  HideTouchToFill();
  PersonalDataManager* pdm = manager_->client()->GetPersonalDataManager();
  DCHECK(pdm);
  CreditCard* card = pdm->GetCreditCardByGUID(unique_id);
  manager_->FillOrPreviewCreditCardForm(mojom::RendererFormDataAction::kFill,
                                        query_form_, query_field_, card);
}

base::WeakPtr<TouchToFillDelegateImpl> TouchToFillDelegateImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace autofill
