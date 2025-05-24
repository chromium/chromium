// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/card_name_fix_flow_controller_impl.h"

#include <utility>

#include "base/check.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/ui/payments/card_name_fix_flow_view.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

CardNameFixFlowControllerImpl::CardNameFixFlowControllerImpl() = default;

CardNameFixFlowControllerImpl::~CardNameFixFlowControllerImpl() {
  MaybeDestroyCardNameFixFlowView(true);
}

void CardNameFixFlowControllerImpl::Show(
    CardNameFixFlowView* card_name_fix_flow_view,
    const std::u16string& inferred_cardholder_name,
    base::OnceCallback<void(const std::u16string&)> name_accepted_callback) {
  DCHECK(!name_accepted_callback.is_null());
  DCHECK(card_name_fix_flow_view);

  MaybeDestroyCardNameFixFlowView(false);
  card_name_fix_flow_view_ = card_name_fix_flow_view;

  name_accepted_callback_ = std::move(name_accepted_callback);

  inferred_cardholder_name_ = inferred_cardholder_name;
  autofill_metrics::LogSaveCardCardholderNamePrefilled(
      !inferred_cardholder_name_.empty());

  card_name_fix_flow_view_->Show();
  AutofillMetrics::LogCardholderNameFixFlowPromptEvent(
      AutofillMetrics::CARDHOLDER_NAME_FIX_FLOW_PROMPT_SHOWN);
  shown_ = true;
  had_user_interaction_ = false;
}

void CardNameFixFlowControllerImpl::OnConfirmNameDialogClosed() {
  MaybeDestroyCardNameFixFlowView(false);
}

void CardNameFixFlowControllerImpl::OnNameAccepted(const std::u16string& name) {
  AutofillMetrics::LogCardholderNameFixFlowPromptEvent(
      AutofillMetrics::CARDHOLDER_NAME_FIX_FLOW_PROMPT_ACCEPTED);
  LogSaveCreditCardPromptResult(
      autofill_metrics::SaveCreditCardPromptResult::kAccepted, true,
      payments::PaymentsAutofillClient::SaveCreditCardOptions()
          .with_should_request_name_from_user(true));
  had_user_interaction_ = true;
  autofill_metrics::LogSaveCardCardholderNameWasEdited(
      inferred_cardholder_name_ != name);
  std::u16string trimmed_name;
  base::TrimWhitespace(name, base::TRIM_ALL, &trimmed_name);
  std::move(name_accepted_callback_).Run(trimmed_name);
}

void CardNameFixFlowControllerImpl::OnDismissed() {
  AutofillMetrics::LogCardholderNameFixFlowPromptEvent(
      AutofillMetrics::CARDHOLDER_NAME_FIX_FLOW_PROMPT_DISMISSED);
  LogSaveCreditCardPromptResult(
      autofill_metrics::SaveCreditCardPromptResult::kDenied, true,
      payments::PaymentsAutofillClient::SaveCreditCardOptions()
          .with_should_request_name_from_user(true));
  had_user_interaction_ = true;
}

int CardNameFixFlowControllerImpl::GetIconId() const {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return IDR_AUTOFILL_GOOGLE_PAY_WITH_DIVIDER;
#else
  return 0;
#endif
}

std::u16string CardNameFixFlowControllerImpl::GetCancelButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_CANCEL);
}

std::u16string CardNameFixFlowControllerImpl::GetInferredCardholderName()
    const {
  return inferred_cardholder_name_;
}

std::u16string CardNameFixFlowControllerImpl::GetInferredNameTooltipText()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_CARD_PROMPT_CARDHOLDER_NAME_TOOLTIP);
}

std::u16string CardNameFixFlowControllerImpl::GetInputLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_CARD_PROMPT_CARDHOLDER_NAME);
}

std::u16string CardNameFixFlowControllerImpl::GetInputPlaceholderText() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_CARD_PROMPT_CARDHOLDER_NAME);
}

std::u16string CardNameFixFlowControllerImpl::GetSaveButtonLabel() const {
#if BUILDFLAG(IS_IOS)
  return l10n_util::GetStringUTF16(IDS_SAVE);
#else
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_FIX_FLOW_PROMPT_SAVE_CARD_LABEL);
#endif
}

std::u16string CardNameFixFlowControllerImpl::GetTitleText() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_CARD_CARDHOLDER_NAME_FIX_FLOW_HEADER);
}

void CardNameFixFlowControllerImpl::MaybeDestroyCardNameFixFlowView(
    bool controller_gone) {
  if (card_name_fix_flow_view_ == nullptr)
    return;
  if (controller_gone)
    card_name_fix_flow_view_->ControllerGone();
  if (shown_ && !had_user_interaction_) {
    AutofillMetrics::LogCardholderNameFixFlowPromptEvent(
        AutofillMetrics::
            CARDHOLDER_NAME_FIX_FLOW_PROMPT_CLOSED_WITHOUT_INTERACTION);
    LogSaveCreditCardPromptResult(
        autofill_metrics::SaveCreditCardPromptResult::kInteractedAndIgnored,
        true,
        payments::PaymentsAutofillClient::SaveCreditCardOptions()
            .with_should_request_name_from_user(true));
  }
  card_name_fix_flow_view_ = nullptr;
}

}  // namespace autofill
