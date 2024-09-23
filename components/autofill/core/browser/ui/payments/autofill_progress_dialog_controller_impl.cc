// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/autofill_progress_dialog_controller_impl.h"

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autofill_progress_dialog_type.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

AutofillProgressDialogControllerImpl::AutofillProgressDialogControllerImpl(
    AutofillProgressDialogType autofill_progress_dialog_type,
    base::OnceClosure cancel_callback)
    : autofill_progress_dialog_type_(autofill_progress_dialog_type),
      cancel_callback_(std::move(cancel_callback)) {}

AutofillProgressDialogControllerImpl::~AutofillProgressDialogControllerImpl() {
  // This if-statement is entered in the case where the tab is closed. When the
  // tab is closed, the controller is destroyed before the view is destroyed, so
  // we need to invalidate the pointer to `this` in
  // `autofill_progress_dialog_view_` and trigger `OnDismissed()` manually.
  if (autofill_progress_dialog_view_) {
    autofill_progress_dialog_view_->InvalidateControllerForCallbacks();
    OnDismissed(/*is_canceled_by_user=*/true);
    autofill_progress_dialog_view_ = nullptr;
  }
}

void AutofillProgressDialogControllerImpl::ShowDialog(
    base::OnceCallback<base::WeakPtr<AutofillProgressDialogView>()>
        create_and_show_view_callback) {
  if (autofill_progress_dialog_view_) {
    return;
  }

  autofill_progress_dialog_view_ =
      std::move(create_and_show_view_callback).Run();

  if (autofill_progress_dialog_view_) {
    AutofillMetrics::LogProgressDialogShown(autofill_progress_dialog_type_);
  }
}

void AutofillProgressDialogControllerImpl::DismissDialog(
    bool show_confirmation_before_closing,
    base::OnceClosure no_interactive_authentication_callback) {
  if (!autofill_progress_dialog_view_) {
    return;
  }

  no_interactive_authentication_callback_ =
      std::move(no_interactive_authentication_callback);

  autofill_progress_dialog_view_->Dismiss(show_confirmation_before_closing,
                                          /*is_canceled_by_user=*/false);
}

void AutofillProgressDialogControllerImpl::OnDismissed(
    bool is_canceled_by_user) {
  // Dialog is being dismissed so set the pointer to nullptr.
  autofill_progress_dialog_view_ = nullptr;
  if (is_canceled_by_user) {
    std::move(cancel_callback_).Run();
  } else {
    if (no_interactive_authentication_callback_) {
      std::move(no_interactive_authentication_callback_).Run();
    }
  }

  AutofillMetrics::LogProgressDialogResultMetric(
      is_canceled_by_user, autofill_progress_dialog_type_);
  cancel_callback_.Reset();
}

std::u16string AutofillProgressDialogControllerImpl::GetLoadingTitle() const {
  switch (autofill_progress_dialog_type_) {
    case AutofillProgressDialogType::kVirtualCardUnmaskProgressDialog:
    case AutofillProgressDialogType::kServerCardUnmaskProgressDialog:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_PROGRESS_DIALOG_TITLE);
    case AutofillProgressDialogType::kServerIbanUnmaskProgressDialog:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_IBAN_UNMASK_PROGRESS_DIALOG_TITLE);
    case AutofillProgressDialogType::k3dsFetchVcnProgressDialog:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_LOADING_AND_CONSENT_DIALOG_TITLE_VCN_3DS);
    case AutofillProgressDialogType::kUnspecified:
      NOTREACHED();
  }
}

std::u16string AutofillProgressDialogControllerImpl::GetConfirmationTitle()
    const {
  switch (autofill_progress_dialog_type_) {
    case AutofillProgressDialogType::kVirtualCardUnmaskProgressDialog:
    case AutofillProgressDialogType::kServerCardUnmaskProgressDialog:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_CONFIRMATION_DIALOG_TITLE);
    case AutofillProgressDialogType::k3dsFetchVcnProgressDialog:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_LOADING_AND_CONSENT_DIALOG_TITLE_VCN_3DS);
    case AutofillProgressDialogType::kServerIbanUnmaskProgressDialog:
    case AutofillProgressDialogType::kUnspecified:
      NOTREACHED();
  }
}

std::u16string AutofillProgressDialogControllerImpl::GetCancelButtonLabel()
    const {
  switch (autofill_progress_dialog_type_) {
    case AutofillProgressDialogType::kVirtualCardUnmaskProgressDialog:
    case AutofillProgressDialogType::kServerCardUnmaskProgressDialog:
    // TODO(crbug.com/296651406): Rename
    // IDS_AUTOFILL_CARD_UNMASK_CANCEL_BUTTON_LABEL to
    // IDS_AUTOFILL_PAYMENT_METHOD_UNMASK_CANCEL_BUTTON_LABEL
    case AutofillProgressDialogType::kServerIbanUnmaskProgressDialog:
    case autofill::AutofillProgressDialogType::k3dsFetchVcnProgressDialog:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_CANCEL_BUTTON_LABEL);
    case AutofillProgressDialogType::kUnspecified:
      NOTREACHED();
  }
}

std::u16string AutofillProgressDialogControllerImpl::GetLoadingMessage() const {
  switch (autofill_progress_dialog_type_) {
    case AutofillProgressDialogType::kVirtualCardUnmaskProgressDialog:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_PROGRESS_BAR_MESSAGE);
    case AutofillProgressDialogType::kServerCardUnmaskProgressDialog:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_MASKED_SERVER_CARD_RISK_BASED_UNMASK_PROGRESS_BAR_MESSAGE);
    case AutofillProgressDialogType::kServerIbanUnmaskProgressDialog:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_IBAN_UNMASK_PROGRESS_BAR_MESSAGE);
    case AutofillProgressDialogType::k3dsFetchVcnProgressDialog:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_3DS_FETCH_VCN_PROGRESS_DIALOG_LOADING_MESSAGE);
    case AutofillProgressDialogType::kUnspecified:
      NOTREACHED();
  }
}

std::u16string AutofillProgressDialogControllerImpl::GetConfirmationMessage()
    const {
  switch (autofill_progress_dialog_type_) {
    case AutofillProgressDialogType::kVirtualCardUnmaskProgressDialog:
    case AutofillProgressDialogType::kServerCardUnmaskProgressDialog:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_CONFIRMATION_MESSAGE);
    case AutofillProgressDialogType::k3dsFetchVcnProgressDialog:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_3DS_FETCH_VCN_PROGRESS_DIALOG_CONFIRMATION_MESSAGE);
    case AutofillProgressDialogType::kServerIbanUnmaskProgressDialog:
    case AutofillProgressDialogType::kUnspecified:
      NOTREACHED();
  }
}

base::WeakPtr<AutofillProgressDialogController>
AutofillProgressDialogControllerImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

#if BUILDFLAG(IS_IOS)
base::WeakPtr<AutofillProgressDialogControllerImpl>
AutofillProgressDialogControllerImpl::GetImplWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
#endif

}  // namespace autofill
