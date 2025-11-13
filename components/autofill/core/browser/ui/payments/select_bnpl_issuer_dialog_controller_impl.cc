// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/select_bnpl_issuer_dialog_controller_impl.h"

#include "base/check_deref.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/metrics/payments/bnpl_metrics.h"
#include "components/autofill/core/browser/payments/bnpl_util.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/ui/payments/select_bnpl_issuer_view.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill::payments {

using IssuerId = autofill::BnplIssuer::IssuerId;
using ::autofill::autofill_metrics::LogBnplIssuerSelection;
using ::autofill::autofill_metrics::LogSelectBnplIssuerDialogResult;
using ::autofill::autofill_metrics::SelectBnplIssuerDialogResult;
using l10n_util::GetStringUTF16;
using std::u16string;

SelectBnplIssuerDialogControllerImpl::SelectBnplIssuerDialogControllerImpl(
    PaymentsAutofillClient* client)
    : client_(CHECK_DEREF(client)) {}

SelectBnplIssuerDialogControllerImpl::~SelectBnplIssuerDialogControllerImpl() =
    default;

void SelectBnplIssuerDialogControllerImpl::ShowDialog(
    base::OnceCallback<std::unique_ptr<SelectBnplIssuerView>()>
        create_and_show_dialog_callback,
    std::vector<BnplIssuerContext> issuer_contexts,
    std::string app_locale,
    base::OnceCallback<void(BnplIssuer)> selected_issuer_callback,
    base::OnceClosure cancel_callback) {
  issuer_contexts_ = std::move(issuer_contexts);
  app_locale_ = std::move(app_locale);
  selected_issuer_callback_ = std::move(selected_issuer_callback);
  cancel_callback_ = std::move(cancel_callback);

  dialog_view_ = std::move(create_and_show_dialog_callback).Run();
  autofill_metrics::LogBnplSelectionDialogShown();
}

void SelectBnplIssuerDialogControllerImpl::UpdateDialogWithIssuers(
    std::vector<BnplIssuerContext> issuer_contexts) {
  issuer_contexts_ = std::move(issuer_contexts);
  dialog_view_->UpdateDialogWithIssuers();
}

void SelectBnplIssuerDialogControllerImpl::OnIssuerSelected(BnplIssuer issuer) {
  LogSelectBnplIssuerDialogResult(
      SelectBnplIssuerDialogResult::kIssuerSelected);
  LogBnplIssuerSelection(issuer.issuer_id());

  if (selected_issuer_callback_) {
    std::move(selected_issuer_callback_).Run(std::move(issuer));
  }
}

void SelectBnplIssuerDialogControllerImpl::OnUserCancelled() {
  LogSelectBnplIssuerDialogResult(
      SelectBnplIssuerDialogResult::kCancelButtonClicked);
  Dismiss();
  std::move(cancel_callback_).Run();
}

void SelectBnplIssuerDialogControllerImpl::Dismiss() {
  dialog_view_.reset();
}

const std::vector<BnplIssuerContext>&
SelectBnplIssuerDialogControllerImpl::GetIssuerContexts() const {
  return issuer_contexts_;
}

const std::string& SelectBnplIssuerDialogControllerImpl::GetAppLocale() const {
  return app_locale_;
}

u16string SelectBnplIssuerDialogControllerImpl::GetTitle() const {
  return GetStringUTF16(IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_TITLE);
}

// TODO(crbug.com/430575808): Remove `GetSelectionOptionText` and instead use
// `GetBnplIssuerSelectionOptionText`.
u16string SelectBnplIssuerDialogControllerImpl::GetSelectionOptionText(
    IssuerId issuer_id) const {
  return GetBnplIssuerSelectionOptionText(issuer_id, GetAppLocale(),
                                          GetIssuerContexts());
}

// TODO(crbug.com/430575808): Remove `GetLinkText` and instead use
// `GetBnplUiFooterText`.
// TODO(crbug.com/405187652) Check if we want the selection dialog footer to
// have multiple lines when the text doesn't fit into one line.
TextWithLink SelectBnplIssuerDialogControllerImpl::GetLinkText() const {
#if !BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableAiBasedAmountExtraction)) {
    return GetBnplUiFooterTextForAi(client_->GetPaymentsDataManager());
  } else {
    return GetBnplUiFooterText();
  }
#else
  // `GetBnplUiFooterText` on Android does not return a `TextWithLink`.
  NOTREACHED();
#endif  // !BUILDFLAG(IS_ANDROID)
}

}  // namespace autofill::payments
