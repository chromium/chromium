// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/bnpl_tos_controller_impl.h"

#include "base/check_deref.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/metrics/payments/bnpl_metrics.h"
#include "components/autofill/core/browser/ui/payments/bnpl_tos_view.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using l10n_util::GetStringFUTF16;
using l10n_util::GetStringUTF16;
using std::u16string;

namespace autofill {
using autofill_metrics::BnplTosDialogResult;
using autofill_metrics::LogBnplTosDialogShown;

namespace {
// LINT.IfChange
constexpr std::string_view kWalletLinkText = "wallet.google.com";
constexpr std::string_view kWalletUrlString = "https://wallet.google.com/";
// LINT.ThenChange(//chrome/browser/touch_to_fill/autofill/android/internal/java/src/org/chromium/chrome/browser/touch_to_fill/payments/TouchToFillPaymentMethodMediator.java)
}  // namespace

BnplTosModel::BnplTosModel() = default;

BnplTosModel::BnplTosModel(const BnplTosModel& other) = default;

BnplTosModel::BnplTosModel(BnplTosModel&& other) = default;

BnplTosModel& BnplTosModel::operator=(const BnplTosModel& other) = default;

BnplTosModel& BnplTosModel::operator=(BnplTosModel&& other) = default;

BnplTosModel::~BnplTosModel() = default;

bool BnplTosModel::operator==(const BnplTosModel&) const = default;

BnplTosControllerImpl::BnplTosControllerImpl(AutofillClient* client)
    : client_(CHECK_DEREF(client)) {}

BnplTosControllerImpl::~BnplTosControllerImpl() = default;

void BnplTosControllerImpl::OnUserAccepted() {
  std::move(accept_callback_).Run();
  LogBnplTosDialogResult(BnplTosDialogResult::kAcceptButtonClicked,
                         GetIssuerId());
}

void BnplTosControllerImpl::OnUserCancelled() {
  Dismiss();
  std::move(cancel_callback_).Run();
  LogBnplTosDialogResult(BnplTosDialogResult::kCancelButtonClicked,
                         GetIssuerId());
}

u16string BnplTosControllerImpl::GetOkButtonLabel() const {
  return GetStringUTF16(IDS_AUTOFILL_BNPL_TOS_OK_BUTTON_LABEL);
}

u16string BnplTosControllerImpl::GetCancelButtonLabel() const {
  return GetStringUTF16(IDS_AUTOFILL_BNPL_TOS_CANCEL_BUTTON_LABEL);
}

u16string BnplTosControllerImpl::GetTitle() const {
  if (model_.issuer.payment_instrument() &&
      model_.issuer.payment_instrument()->action_required().contains(
          autofill::PaymentInstrument::ActionRequired::kAcceptTos)) {
    return GetStringFUTF16(IDS_AUTOFILL_BNPL_TOS_LINKED_TITLE,
                           model_.issuer.GetDisplayName());
  }

  return GetStringFUTF16(IDS_AUTOFILL_BNPL_TOS_UNLINKED_TITLE,
                         model_.issuer.GetDisplayName());
}

u16string BnplTosControllerImpl::GetReviewText() const {
  return GetStringFUTF16(IDS_AUTOFILL_BNPL_TOS_REVIEW_TEXT,
                         model_.issuer.GetDisplayName());
}

u16string BnplTosControllerImpl::GetApproveText() const {
  return GetStringFUTF16(IDS_AUTOFILL_BNPL_TOS_APPROVE_TEXT,
                         model_.issuer.GetDisplayName());
}

TextWithLink BnplTosControllerImpl::GetLinkText() const {
  TextWithLink text_with_link;
  std::vector<size_t> offsets;
  text_with_link.text = GetStringFUTF16(
      IDS_AUTOFILL_BNPL_TOS_LINK_TEXT, model_.issuer.GetDisplayName(),
      base::UTF8ToUTF16(kWalletLinkText), &offsets);

  // The link is the second replacement string making it the second offset.
  text_with_link.offset =
      gfx::Range(offsets[1], offsets[1] + kWalletLinkText.length());

  text_with_link.url = GURL(kWalletUrlString);

  return text_with_link;
}

const LegalMessageLines& BnplTosControllerImpl::GetLegalMessageLines() const {
  return model_.legal_message_lines;
}

AccountInfo BnplTosControllerImpl::GetAccountInfo() const {
  signin::IdentityManager* identity_manager = client_->GetIdentityManager();
  if (!identity_manager) {
    return AccountInfo();
  }

  return identity_manager->FindExtendedAccountInfo(
      client_->GetPersonalDataManager()
          .payments_data_manager()
          .GetAccountInfoForPaymentsServer());
}

BnplIssuer::IssuerId BnplTosControllerImpl::GetIssuerId() const {
  return model_.issuer.issuer_id();
}

base::WeakPtr<BnplTosController> BnplTosControllerImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void BnplTosControllerImpl::Show(
    base::OnceCallback<std::unique_ptr<BnplTosView>()>
        create_and_show_view_callback,
    BnplTosModel model,
    base::OnceClosure accept_callback,
    base::OnceClosure cancel_callback) {
  // If the view already exists, don't create and show a new view.
  if (view_) {
    return;
  }

  model_ = std::move(model);
  accept_callback_ = std::move(accept_callback);
  cancel_callback_ = std::move(cancel_callback);

  view_ = std::move(create_and_show_view_callback).Run();
  LogBnplTosDialogShown(GetIssuerId());
}

void BnplTosControllerImpl::Dismiss() {
  view_.reset();
}

}  // namespace autofill
