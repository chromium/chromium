// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/autofill_error_dialog_controller_impl.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

AutofillErrorDialogControllerImpl::AutofillErrorDialogControllerImpl(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

AutofillErrorDialogControllerImpl::~AutofillErrorDialogControllerImpl() {
  Dismiss();
}

void AutofillErrorDialogControllerImpl::Show(
    const AutofillErrorDialogContext& autofill_error_dialog_context) {
  if (autofill_error_dialog_view_)
    Dismiss();

  DCHECK(autofill_error_dialog_view_ == nullptr);
  error_dialog_context_ = autofill_error_dialog_context;
  autofill_error_dialog_view_ = AutofillErrorDialogView::CreateAndShow(this);

  base::UmaHistogramEnumeration("Autofill.ErrorDialogShown",
                                autofill_error_dialog_context.type);

  // If both |server_returned_title| and |server_returned_description| are
  // populated, then the error dialog was displayed with the server-driven text.
  if (error_dialog_context_.server_returned_title &&
      error_dialog_context_.server_returned_description) {
    base::UmaHistogramEnumeration("Autofill.ErrorDialogShown.WithServerText",
                                  autofill_error_dialog_context.type);
  }
}

void AutofillErrorDialogControllerImpl::OnDismissed() {
  // TODO(crbug.com/1196021): Log the dismiss action along with the type of the
  // error dialog.
  autofill_error_dialog_view_ = nullptr;
}

const std::u16string AutofillErrorDialogControllerImpl::GetTitle() {
  // If the server returned a title to be displayed, we prefer it since this
  // title will be more detailed to the specific error that occurred. We must
  // ensure that both a title and a description were returned from the server
  // before using this title.
  if (error_dialog_context_.server_returned_title &&
      error_dialog_context_.server_returned_description) {
    return base::UTF8ToUTF16(*error_dialog_context_.server_returned_title);
  }

  switch (error_dialog_context_.type) {
    case AutofillErrorDialogType::kVirtualCardTemporaryError:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_TEMPORARY_ERROR_TITLE);
    case AutofillErrorDialogType::kVirtualCardPermanentError:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_PERMANENT_ERROR_TITLE);
    case AutofillErrorDialogType::kVirtualCardNotEligibleError:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_NOT_ELIGIBLE_ERROR_TITLE);
    case AutofillErrorDialogType::
        kMaskedServerCardRiskBasedUnmaskingNetworkError:
    case AutofillErrorDialogType::
        kMaskedServerCardRiskBasedUnmaskingPermanentError:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_MASKED_SERVER_CARD_RISK_BASED_UNMASKING_ERROR_TITLE);
    case AutofillErrorDialogType::kTypeUnknown:
      NOTREACHED();
      return std::u16string();
  }
}

const std::u16string AutofillErrorDialogControllerImpl::GetDescription() {
  // If the server returned a description to be displayed, we prefer it since
  // this description will be more detailed to the specific error that occurred.
  // We must ensure that both a title and a description were returned from the
  // server before using this description.
  if (error_dialog_context_.server_returned_title &&
      error_dialog_context_.server_returned_description) {
    return base::UTF8ToUTF16(
        *error_dialog_context_.server_returned_description);
  }

  switch (error_dialog_context_.type) {
    case AutofillErrorDialogType::kVirtualCardTemporaryError:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_TEMPORARY_ERROR_DESCRIPTION);
    case AutofillErrorDialogType::kVirtualCardPermanentError:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_PERMANENT_ERROR_DESCRIPTION);
    case AutofillErrorDialogType::kVirtualCardNotEligibleError:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_NOT_ELIGIBLE_ERROR_DESCRIPTION);
    case AutofillErrorDialogType::
        kMaskedServerCardRiskBasedUnmaskingNetworkError:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_PROMPT_ERROR_NETWORK);
    case AutofillErrorDialogType::
        kMaskedServerCardRiskBasedUnmaskingPermanentError:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_PROMPT_ERROR_PERMANENT);
    case AutofillErrorDialogType::kTypeUnknown:
      NOTREACHED();
      return std::u16string();
  }
}

const std::u16string AutofillErrorDialogControllerImpl::GetButtonLabel() {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_ERROR_DIALOG_NEGATIVE_BUTTON_LABEL);
}

content::WebContents* AutofillErrorDialogControllerImpl::GetWebContents() {
  return web_contents_;
}

void AutofillErrorDialogControllerImpl::Dismiss() {
  if (autofill_error_dialog_view_)
    autofill_error_dialog_view_->Dismiss();
}

}  // namespace autofill
