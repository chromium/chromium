// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/card_expiration_date_fix_flow_controller_impl.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/ui/payments/card_expiration_date_fix_flow_view.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

CardExpirationDateFixFlowControllerImpl::
    CardExpirationDateFixFlowControllerImpl() {}

CardExpirationDateFixFlowControllerImpl::
    ~CardExpirationDateFixFlowControllerImpl() {
  if (card_expiration_date_fix_flow_view_)
    card_expiration_date_fix_flow_view_->ControllerGone();

  if (shown_ && !had_user_interaction_) {
    AutofillMetrics::LogExpirationDateFixFlowPromptEvent(
        AutofillMetrics::ExpirationDateFixFlowPromptEvent::
            EXPIRATION_DATE_FIX_FLOW_PROMPT_CLOSED_WITHOUT_INTERACTION);
  }
}

void CardExpirationDateFixFlowControllerImpl::Show(
    CardExpirationDateFixFlowView* card_expiration_date_fix_flow_view,
    const CreditCard& card,
    base::OnceCallback<void(const base::string16&, const base::string16&)>
        callback) {
  DCHECK(!callback.is_null());
  DCHECK(card_expiration_date_fix_flow_view);

  card_label_ = card.NetworkAndLastFourDigits();

  if (card_expiration_date_fix_flow_view_)
    card_expiration_date_fix_flow_view_->ControllerGone();
  card_expiration_date_fix_flow_view_ = card_expiration_date_fix_flow_view;

  upload_save_card_callback_ = std::move(callback);

  card_expiration_date_fix_flow_view_->Show();
  AutofillMetrics::LogExpirationDateFixFlowPromptShown();
  shown_ = true;
}

void CardExpirationDateFixFlowControllerImpl::OnAccepted(
    const base::string16& month,
    const base::string16& year) {
  AutofillMetrics::LogExpirationDateFixFlowPromptEvent(
      AutofillMetrics::ExpirationDateFixFlowPromptEvent::
          EXPIRATION_DATE_FIX_FLOW_PROMPT_ACCEPTED);
  had_user_interaction_ = true;
  std::move(upload_save_card_callback_).Run(month, year);
}

void CardExpirationDateFixFlowControllerImpl::OnDismissed() {
  AutofillMetrics::LogExpirationDateFixFlowPromptEvent(
      AutofillMetrics::ExpirationDateFixFlowPromptEvent::
          EXPIRATION_DATE_FIX_FLOW_PROMPT_DISMISSED);
  had_user_interaction_ = true;
}

void CardExpirationDateFixFlowControllerImpl::OnDialogClosed() {
  card_expiration_date_fix_flow_view_ = nullptr;
}

int CardExpirationDateFixFlowControllerImpl::GetIconId() const {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return IDR_AUTOFILL_GOOGLE_PAY_WITH_DIVIDER;
#else
  return 0;
#endif
}

base::string16 CardExpirationDateFixFlowControllerImpl::GetTitleText() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_CARD_UPDATE_EXPIRATION_DATE_TITLE);
}

base::string16 CardExpirationDateFixFlowControllerImpl::GetSaveButtonLabel()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_FIX_FLOW_PROMPT_SAVE_CARD_LABEL);
}

base::string16 CardExpirationDateFixFlowControllerImpl::GetCardLabel() const {
  return card_label_;
}

base::string16 CardExpirationDateFixFlowControllerImpl::GetCancelButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_CANCEL);
}

base::string16 CardExpirationDateFixFlowControllerImpl::GetInputLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_CARD_UPDATE_EXPIRATION_DATE_TOOLTIP);
}

base::string16 CardExpirationDateFixFlowControllerImpl::GetDateSeparator()
    const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_EXPIRATION_DATE_SEPARATOR);
}

base::string16 CardExpirationDateFixFlowControllerImpl::GetInvalidDateError()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_CARD_UPDATE_EXPIRATION_DATE_ERROR_TRY_AGAIN);
}

}  // namespace autofill
