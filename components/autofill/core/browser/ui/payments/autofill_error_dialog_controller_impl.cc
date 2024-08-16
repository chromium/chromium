// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/autofill_error_dialog_controller_impl.h"

#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "components/autofill/core/browser/ui/payments/autofill_error_dialog_view.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

AutofillErrorDialogControllerImpl::AutofillErrorDialogControllerImpl(
    AutofillErrorDialogContext error_dialog_context)
    : error_dialog_context_(std::move(error_dialog_context)) {}

AutofillErrorDialogControllerImpl::~AutofillErrorDialogControllerImpl() {
  DismissIfApplicable();
}

void AutofillErrorDialogControllerImpl::Show(
    base::OnceCallback<base::WeakPtr<AutofillErrorDialogView>()>
        view_creation_callback) {
  CHECK(!autofill_error_dialog_view_);
  autofill_error_dialog_view_ = std::move(view_creation_callback).Run();
  CHECK(autofill_error_dialog_view_);

  base::UmaHistogramEnumeration("Autofill.ErrorDialogShown",
                                error_dialog_context_.type);

  // If both |server_returned_title| and |server_returned_description| are
  // populated, then the error dialog was displayed with the server-driven text.
  if (error_dialog_context_.server_returned_title &&
      error_dialog_context_.server_returned_description) {
    base::UmaHistogramEnumeration("Autofill.ErrorDialogShown.WithServerText",
                                  error_dialog_context_.type);
  }
}

base::WeakPtr<AutofillErrorDialogController>
AutofillErrorDialogControllerImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void AutofillErrorDialogControllerImpl::OnDismissed() {
  // TODO(crbug.com/40176273): Log the dismiss action along with the type of the
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
    case AutofillErrorDialogType::kMaskedServerIbanUnmaskingTemporaryError:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_IBAN_UNMASK_ERROR_DIALOG_TITLE);
    case AutofillErrorDialogType::kCreditCardUploadError:
#if BUILDFLAG(IS_IOS)
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_FAILURE_TITLE_TEXT);
#else
      NOTREACHED();
#endif  // BUILDFLAG(IS_IOS)
    case AutofillErrorDialogType::kVirtualCardEnrollmentTemporaryError:
#if BUILDFLAG(IS_IOS)
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_TEMPORARY_ERROR_TITLE);
#else
      NOTREACHED();
#endif  // BUILDFLAG(IS_IOS)
    case AutofillErrorDialogType::kTypeUnknown:
      NOTREACHED();
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
    case AutofillErrorDialogType::kMaskedServerIbanUnmaskingTemporaryError:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_IBAN_UNMASK_ERROR_DIALOG_MESSAGE);
    case AutofillErrorDialogType::kCreditCardUploadError:
#if BUILDFLAG(IS_IOS)
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_FAILURE_DESCRIPTION_TEXT);
#else
      NOTREACHED();
#endif  // BUILDFLAG(IS_IOS)
    case AutofillErrorDialogType::kVirtualCardEnrollmentTemporaryError:
#if BUILDFLAG(IS_IOS)
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_TEMPORARY_ERROR_DESCRIPTION);
#else
      NOTREACHED();
#endif  // BUILDFLAG(IS_IOS)
    case AutofillErrorDialogType::kTypeUnknown:
      NOTREACHED();
  }
}

const std::u16string AutofillErrorDialogControllerImpl::GetButtonLabel() {
  if (error_dialog_context_.type ==
          AutofillErrorDialogType::kCreditCardUploadError ||
      error_dialog_context_.type ==
          AutofillErrorDialogType::kVirtualCardEnrollmentTemporaryError) {
#if BUILDFLAG(IS_IOS)
    return l10n_util::GetStringUTF16(IDS_OK);
#else  // BUILDFLAG(IS_IOS)
    // Not reachable on non-iOS platforms.
    NOTREACHED();
#endif
  }

  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_ERROR_DIALOG_NEGATIVE_BUTTON_LABEL);
}

void AutofillErrorDialogControllerImpl::DismissIfApplicable() {
  if (autofill_error_dialog_view_) {
    autofill_error_dialog_view_->Dismiss();
  }
}

}  // namespace autofill
