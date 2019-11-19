// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller_impl.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_view.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/grit/components_scaled_resources.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

CardUnmaskPromptControllerImpl::CardUnmaskPromptControllerImpl(
    PrefService* pref_service,
    bool is_off_the_record)
    : pref_service_(pref_service), is_off_the_record_(is_off_the_record) {}

CardUnmaskPromptControllerImpl::~CardUnmaskPromptControllerImpl() {
  if (card_unmask_view_)
    card_unmask_view_->ControllerGone();
}

void CardUnmaskPromptControllerImpl::ShowPrompt(
    CardUnmaskPromptViewFactory card_unmask_view_factory,
    const CreditCard& card,
    AutofillClient::UnmaskCardReason reason,
    base::WeakPtr<CardUnmaskDelegate> delegate) {
  if (card_unmask_view_)
    card_unmask_view_->ControllerGone();

  new_card_link_clicked_ = false;
  shown_timestamp_ = AutofillClock::Now();
  pending_details_ = CardUnmaskDelegate::UserProvidedUnmaskDetails();
  card_ = card;
  reason_ = reason;
  delegate_ = delegate;
  card_unmask_view_ = std::move(card_unmask_view_factory).Run();
  card_unmask_view_->Show();
  unmasking_result_ = AutofillClient::NONE;
  unmasking_number_of_attempts_ = 0;
  unmasking_initial_should_store_pan_ = GetStoreLocallyStartState();
  AutofillMetrics::LogUnmaskPromptEvent(AutofillMetrics::UNMASK_PROMPT_SHOWN);
}

void CardUnmaskPromptControllerImpl::OnVerificationResult(
    AutofillClient::PaymentsRpcResult result) {
  if (!card_unmask_view_)
    return;

  base::string16 error_message;
  switch (result) {
    case AutofillClient::SUCCESS:
      break;

    case AutofillClient::TRY_AGAIN_FAILURE: {
      error_message = l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_PROMPT_ERROR_TRY_AGAIN_CVC);
      break;
    }

    case AutofillClient::PERMANENT_FAILURE: {
      error_message = l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_PROMPT_ERROR_PERMANENT);
      break;
    }

    case AutofillClient::NETWORK_ERROR: {
      error_message = l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_PROMPT_ERROR_NETWORK);
      break;
    }

    case AutofillClient::NONE:
      NOTREACHED();
      return;
  }

  unmasking_result_ = result;
  AutofillMetrics::LogRealPanResult(result);
  AutofillMetrics::LogUnmaskingDuration(
      AutofillClock::Now() - verify_timestamp_, result);
  card_unmask_view_->GotVerificationResult(error_message, AllowsRetry(result));
}

void CardUnmaskPromptControllerImpl::OnUnmaskDialogClosed() {
  card_unmask_view_ = nullptr;
  LogOnCloseEvents();
  unmasking_result_ = AutofillClient::NONE;
  if (delegate_)
    delegate_->OnUnmaskPromptClosed();
}

void CardUnmaskPromptControllerImpl::OnUnmaskPromptAccepted(
    const base::string16& cvc,
    const base::string16& exp_month,
    const base::string16& exp_year,
    bool should_store_pan,
    bool enable_fido_auth) {
  verify_timestamp_ = AutofillClock::Now();
  unmasking_number_of_attempts_++;
  unmasking_result_ = AutofillClient::NONE;
  card_unmask_view_->DisableAndWaitForVerification();

  DCHECK(InputCvcIsValid(cvc));
  base::TrimWhitespace(cvc, base::TRIM_ALL, &pending_details_.cvc);
  if (ShouldRequestExpirationDate()) {
    DCHECK(InputExpirationIsValid(exp_month, exp_year));
    pending_details_.exp_month = exp_month;
    pending_details_.exp_year = exp_year;
  }
  if (CanStoreLocally()) {
    pending_details_.should_store_pan = should_store_pan;
    // Remember the last choice the user made (on this device).
    pref_service_->SetBoolean(prefs::kAutofillWalletImportStorageCheckboxState,
                              should_store_pan);
  } else {
    DCHECK(!should_store_pan);
    pending_details_.should_store_pan = false;
  }

  // The FIDO authentication checkbox is only shown when the local storage
  // checkbox is not shown and the flag is turned on. If it is shown, then
  // remember the last choice the user made on this device.
  if (base::FeatureList::IsEnabled(
          features::kAutofillCreditCardAuthentication) &&
      !CanStoreLocally()) {
    pref_service_->SetBoolean(
        prefs::kAutofillCreditCardFidoAuthOfferCheckboxState, enable_fido_auth);
  }

  // There is a chance the delegate has disappeared (i.e. tab closed) before the
  // unmask response came in. Avoid a crash.
  if (delegate_)
    delegate_->OnUnmaskPromptAccepted(pending_details_);
}

void CardUnmaskPromptControllerImpl::NewCardLinkClicked() {
  new_card_link_clicked_ = true;
}

base::string16 CardUnmaskPromptControllerImpl::GetWindowTitle() const {
#if defined(OS_IOS)
  // The iOS UI has less room for the title so it shows a shorter string.
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_CARD_UNMASK_PROMPT_TITLE);
#else
  return l10n_util::GetStringFUTF16(
      ShouldRequestExpirationDate()
          ? IDS_AUTOFILL_CARD_UNMASK_PROMPT_EXPIRED_TITLE
          : IDS_AUTOFILL_CARD_UNMASK_PROMPT_TITLE,
      card_.NetworkOrBankNameAndLastFourDigits());
#endif
}

base::string16 CardUnmaskPromptControllerImpl::GetInstructionsMessage() const {
// The prompt for server cards should reference Google Payments, whereas the
// prompt for local cards should not.
#if defined(OS_IOS)
  int ids;
  if (reason_ == AutofillClient::UNMASK_FOR_AUTOFILL &&
      ShouldRequestExpirationDate()) {
    ids = card_.record_type() == autofill::CreditCard::LOCAL_CARD
              ? IDS_AUTOFILL_CARD_UNMASK_PROMPT_INSTRUCTIONS_EXPIRED_LOCAL_CARD
              : IDS_AUTOFILL_CARD_UNMASK_PROMPT_INSTRUCTIONS_EXPIRED;
  } else {
    ids = card_.record_type() == autofill::CreditCard::LOCAL_CARD
              ? IDS_AUTOFILL_CARD_UNMASK_PROMPT_INSTRUCTIONS_LOCAL_CARD
              : IDS_AUTOFILL_CARD_UNMASK_PROMPT_INSTRUCTIONS;
  }
  // The iOS UI shows the card details in the instructions text since they
  // don't fit in the title.
  return l10n_util::GetStringFUTF16(ids,
                                    card_.NetworkOrBankNameAndLastFourDigits());
#else
  return l10n_util::GetStringUTF16(
      card_.record_type() == autofill::CreditCard::LOCAL_CARD
          ? IDS_AUTOFILL_CARD_UNMASK_PROMPT_INSTRUCTIONS_LOCAL_CARD
          : IDS_AUTOFILL_CARD_UNMASK_PROMPT_INSTRUCTIONS);
#endif
}

base::string16 CardUnmaskPromptControllerImpl::GetOkButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_CARD_UNMASK_CONFIRM_BUTTON);
}

int CardUnmaskPromptControllerImpl::GetCvcImageRid() const {
  return card_.network() == kAmericanExpressCard ? IDR_CREDIT_CARD_CVC_HINT_AMEX
                                                 : IDR_CREDIT_CARD_CVC_HINT;
}

bool CardUnmaskPromptControllerImpl::ShouldRequestExpirationDate() const {
  return card_.ShouldUpdateExpiration(AutofillClock::Now()) ||
         new_card_link_clicked_;
}

bool CardUnmaskPromptControllerImpl::CanStoreLocally() const {
  if (base::FeatureList::IsEnabled(
          features::kAutofillNoLocalSaveOnUnmaskSuccess)) {
    return false;
  }
  // Never offer to save for incognito.
  if (is_off_the_record_)
    return false;
  if (reason_ == AutofillClient::UNMASK_FOR_PAYMENT_REQUEST)
    return false;
  if (card_.record_type() == CreditCard::LOCAL_CARD)
    return false;

  return OfferStoreUnmaskedCards(is_off_the_record_);
}

bool CardUnmaskPromptControllerImpl::GetStoreLocallyStartState() const {
  return pref_service_->GetBoolean(
      prefs::kAutofillWalletImportStorageCheckboxState);
}

bool CardUnmaskPromptControllerImpl::GetWebauthnOfferStartState() const {
  return pref_service_->GetBoolean(
      prefs::kAutofillCreditCardFidoAuthOfferCheckboxState);
}

bool CardUnmaskPromptControllerImpl::InputCvcIsValid(
    const base::string16& input_text) const {
  base::string16 trimmed_text;
  base::TrimWhitespace(input_text, base::TRIM_ALL, &trimmed_text);
  return IsValidCreditCardSecurityCode(trimmed_text, card_.network());
}

bool CardUnmaskPromptControllerImpl::InputExpirationIsValid(
    const base::string16& month,
    const base::string16& year) const {
  if ((month.size() != 2U && month.size() != 1U) ||
      (year.size() != 4U && year.size() != 2U)) {
    return false;
  }

  int month_value = 0, year_value = 0;
  if (!base::StringToInt(month, &month_value) ||
      !base::StringToInt(year, &year_value)) {
    return false;
  }

  // Convert 2 digit year to 4 digit year.
  if (year_value < 100) {
    base::Time::Exploded now;
    AutofillClock::Now().LocalExplode(&now);
    year_value += (now.year / 100) * 100;
  }

  return IsValidCreditCardExpirationDate(year_value, month_value,
                                         AutofillClock::Now());
}

int CardUnmaskPromptControllerImpl::GetExpectedCvcLength() const {
  return GetCvcLengthForCardType(card_.network());
}

base::TimeDelta CardUnmaskPromptControllerImpl::GetSuccessMessageDuration()
    const {
  return base::TimeDelta::FromMilliseconds(
      card_.record_type() == CreditCard::LOCAL_CARD ||
              reason_ == AutofillClient::UNMASK_FOR_PAYMENT_REQUEST
          ? 0
          : 500);
}

AutofillClient::PaymentsRpcResult
CardUnmaskPromptControllerImpl::GetVerificationResult() const {
  return unmasking_result_;
}

bool CardUnmaskPromptControllerImpl::AllowsRetry(
    AutofillClient::PaymentsRpcResult result) {
  if (result == AutofillClient::NETWORK_ERROR ||
      result == AutofillClient::PERMANENT_FAILURE) {
    return false;
  }
  return true;
}

void CardUnmaskPromptControllerImpl::LogOnCloseEvents() {
  AutofillMetrics::UnmaskPromptEvent close_reason_event = GetCloseReasonEvent();
  AutofillMetrics::LogUnmaskPromptEvent(close_reason_event);
  AutofillMetrics::LogUnmaskPromptEventDuration(
      AutofillClock::Now() - shown_timestamp_, close_reason_event);

  if (close_reason_event == AutofillMetrics::UNMASK_PROMPT_CLOSED_NO_ATTEMPTS)
    return;

  if (close_reason_event ==
      AutofillMetrics::UNMASK_PROMPT_CLOSED_ABANDON_UNMASKING) {
    AutofillMetrics::LogTimeBeforeAbandonUnmasking(AutofillClock::Now() -
                                                   verify_timestamp_);
  }

  bool final_should_store_pan = pending_details_.should_store_pan;
  if (unmasking_result_ == AutofillClient::SUCCESS && final_should_store_pan) {
    AutofillMetrics::LogUnmaskPromptEvent(
        AutofillMetrics::UNMASK_PROMPT_SAVED_CARD_LOCALLY);
  }

  if (CanStoreLocally()) {
    // Tracking changes in local save preference.
    AutofillMetrics::UnmaskPromptEvent event;
    if (unmasking_initial_should_store_pan_ && final_should_store_pan) {
      event = AutofillMetrics::UNMASK_PROMPT_LOCAL_SAVE_DID_NOT_OPT_OUT;
    } else if (!unmasking_initial_should_store_pan_ &&
               !final_should_store_pan) {
      event = AutofillMetrics::UNMASK_PROMPT_LOCAL_SAVE_DID_NOT_OPT_IN;
    } else if (unmasking_initial_should_store_pan_ && !final_should_store_pan) {
      event = AutofillMetrics::UNMASK_PROMPT_LOCAL_SAVE_DID_OPT_OUT;
    } else {
      event = AutofillMetrics::UNMASK_PROMPT_LOCAL_SAVE_DID_OPT_IN;
    }
    AutofillMetrics::LogUnmaskPromptEvent(event);
  }
}

AutofillMetrics::UnmaskPromptEvent
CardUnmaskPromptControllerImpl::GetCloseReasonEvent() {
  if (unmasking_number_of_attempts_ == 0)
    return AutofillMetrics::UNMASK_PROMPT_CLOSED_NO_ATTEMPTS;

  // If NONE and we have a pending request, we have a pending GetRealPan
  // request.
  if (unmasking_result_ == AutofillClient::NONE)
    return AutofillMetrics::UNMASK_PROMPT_CLOSED_ABANDON_UNMASKING;

  if (unmasking_result_ == AutofillClient::SUCCESS) {
    return unmasking_number_of_attempts_ == 1
               ? AutofillMetrics::UNMASK_PROMPT_UNMASKED_CARD_FIRST_ATTEMPT
               : AutofillMetrics::
                     UNMASK_PROMPT_UNMASKED_CARD_AFTER_FAILED_ATTEMPTS;
  }
  return AllowsRetry(unmasking_result_)
             ? AutofillMetrics::
                   UNMASK_PROMPT_CLOSED_FAILED_TO_UNMASK_RETRIABLE_FAILURE
             : AutofillMetrics::
                   UNMASK_PROMPT_CLOSED_FAILED_TO_UNMASK_NON_RETRIABLE_FAILURE;
}

}  // namespace autofill
