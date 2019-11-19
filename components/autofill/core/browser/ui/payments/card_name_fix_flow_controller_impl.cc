// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/card_name_fix_flow_controller_impl.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/ui/payments/card_name_fix_flow_view.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

CardNameFixFlowControllerImpl::CardNameFixFlowControllerImpl() {}

CardNameFixFlowControllerImpl::~CardNameFixFlowControllerImpl() {
  if (card_name_fix_flow_view_)
    card_name_fix_flow_view_->ControllerGone();

  if (shown_ && !had_user_interaction_) {
    AutofillMetrics::LogCardholderNameFixFlowPromptEvent(
        AutofillMetrics::
            CARDHOLDER_NAME_FIX_FLOW_PROMPT_CLOSED_WITHOUT_INTERACTION);
  }
}

void CardNameFixFlowControllerImpl::Show(
    CardNameFixFlowView* card_name_fix_flow_view,
    const base::string16& inferred_cardholder_name,
    base::OnceCallback<void(const base::string16&)> name_accepted_callback) {
  DCHECK(!name_accepted_callback.is_null());
  DCHECK(card_name_fix_flow_view);

  if (card_name_fix_flow_view_)
    card_name_fix_flow_view_->ControllerGone();
  card_name_fix_flow_view_ = card_name_fix_flow_view;

  name_accepted_callback_ = std::move(name_accepted_callback);

  inferred_cardholder_name_ = inferred_cardholder_name;
  AutofillMetrics::LogSaveCardCardholderNamePrefilled(
      !inferred_cardholder_name_.empty());

  card_name_fix_flow_view_->Show();
  AutofillMetrics::LogCardholderNameFixFlowPromptEvent(
      AutofillMetrics::CARDHOLDER_NAME_FIX_FLOW_PROMPT_SHOWN);
  shown_ = true;
}

void CardNameFixFlowControllerImpl::OnConfirmNameDialogClosed() {
  card_name_fix_flow_view_ = nullptr;
}

void CardNameFixFlowControllerImpl::OnNameAccepted(const base::string16& name) {
  AutofillMetrics::LogCardholderNameFixFlowPromptEvent(
      AutofillMetrics::CARDHOLDER_NAME_FIX_FLOW_PROMPT_ACCEPTED);
  had_user_interaction_ = true;
  AutofillMetrics::LogSaveCardCardholderNameWasEdited(
      inferred_cardholder_name_ != name);
  base::string16 trimmed_name;
  base::TrimWhitespace(name, base::TRIM_ALL, &trimmed_name);
  std::move(name_accepted_callback_).Run(trimmed_name);
}

void CardNameFixFlowControllerImpl::OnDismissed() {
  AutofillMetrics::LogCardholderNameFixFlowPromptEvent(
      AutofillMetrics::CARDHOLDER_NAME_FIX_FLOW_PROMPT_DISMISSED);
  had_user_interaction_ = true;
}

int CardNameFixFlowControllerImpl::GetIconId() const {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return IDR_AUTOFILL_GOOGLE_PAY_WITH_DIVIDER;
#else
  return 0;
#endif
}

base::string16 CardNameFixFlowControllerImpl::GetCancelButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_CANCEL);
}

base::string16 CardNameFixFlowControllerImpl::GetInferredCardholderName()
    const {
  return inferred_cardholder_name_;
}

base::string16 CardNameFixFlowControllerImpl::GetInferredNameTooltipText()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_CARD_PROMPT_CARDHOLDER_NAME_TOOLTIP);
}

base::string16 CardNameFixFlowControllerImpl::GetInputLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_CARD_PROMPT_CARDHOLDER_NAME);
}

base::string16 CardNameFixFlowControllerImpl::GetInputPlaceholderText() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_CARD_PROMPT_CARDHOLDER_NAME);
}

base::string16 CardNameFixFlowControllerImpl::GetSaveButtonLabel() const {
#if defined(OS_IOS)
  return l10n_util::GetStringUTF16(IDS_SAVE);
#else
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_FIX_FLOW_PROMPT_SAVE_CARD_LABEL);
#endif
}

base::string16 CardNameFixFlowControllerImpl::GetTitleText() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_CARD_CARDHOLDER_NAME_FIX_FLOW_HEADER);
}

}  // namespace autofill
