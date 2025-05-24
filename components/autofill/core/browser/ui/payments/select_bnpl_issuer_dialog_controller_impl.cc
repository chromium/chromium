// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/select_bnpl_issuer_dialog_controller_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/metrics/payments/bnpl_metrics.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/ui/payments/select_bnpl_issuer_view.h"
#include "components/payments/core/currency_formatter.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill::payments {

using IssuerId = autofill::BnplIssuer::IssuerId;
using ::autofill::autofill_metrics::LogBnplIssuerSelection;
using ::autofill::autofill_metrics::LogSelectBnplIssuerDialogResult;
using ::autofill::autofill_metrics::SelectBnplIssuerDialogResult;
using l10n_util::GetStringFUTF16;
using l10n_util::GetStringUTF16;
using std::u16string;

SelectBnplIssuerDialogControllerImpl::SelectBnplIssuerDialogControllerImpl() =
    default;

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

u16string SelectBnplIssuerDialogControllerImpl::GetSelectionOptionText(
    IssuerId issuer_id) const {
  // TODO(crbug.com/403361321): Add a util function that returns all supported
  // locales.
  CHECK_EQ(GetAppLocale(), "en-US");
  ::payments::CurrencyFormatter formatter = ::payments::CurrencyFormatter(
      /*currency_code=*/"USD",
      /*locale_name=*/GetAppLocale());
  formatter.SetMaxFractionalDigits(/*max_fractional_digits=*/2);

  BnplIssuerContext issuer_context;
  for (const BnplIssuerContext& current_issuer_context : GetIssuerContexts()) {
    if (current_issuer_context.issuer.issuer_id() == issuer_id) {
      issuer_context = current_issuer_context;
      break;
    }
  }

  // Get the price range in USD as it's the only supported currency for now.
  base::optional_ref<const BnplIssuer::EligiblePriceRange>
      eligible_price_range =
          issuer_context.issuer.GetEligiblePriceRangeForCurrency("USD");
  CHECK(eligible_price_range);

  switch (issuer_context.eligibility) {
    case BnplIssuerEligibilityForPage::kUndefined:
      NOTREACHED();
    case BnplIssuerEligibilityForPage::kIsEligible:
      switch (issuer_id) {
        case IssuerId::kBnplAffirm:
        case IssuerId::kBnplAfterpay:
          return GetStringUTF16(
              IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_PAYMENT_OPTION_AFFIRM_AND_AFTERPAY);
        case IssuerId::kBnplZip:
          return GetStringUTF16(
              IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_PAYMENT_OPTION_ZIP);
      }
      NOTREACHED();
    case BnplIssuerEligibilityForPage::kNotEligibleIssuerDoesNotSupportMerchant:
      return GetStringUTF16(
          IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_PAYMENT_OPTION_NOT_SUPPORTED_BY_MERCHANT);
    case BnplIssuerEligibilityForPage::kNotEligibleCheckoutAmountTooLow:
      // Divide displayed price by `1'000'000.0` to convert from micros and
      // retain decimals.
      return GetStringFUTF16(
          IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_PAYMENT_OPTION_CHECKOUT_AMOUNT_TOO_LOW,
          formatter.Format(base::NumberToString(
              eligible_price_range->price_lower_bound / 1'000'000.0)));
    case BnplIssuerEligibilityForPage::kNotEligibleCheckoutAmountTooHigh:
      // Divide displayed price by `1'000'000.0` to convert from micros and
      // retain decimals.
      return GetStringFUTF16(
          IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_PAYMENT_OPTION_CHECKOUT_AMOUNT_TOO_HIGH,
          formatter.Format(base::NumberToString(
              eligible_price_range->price_upper_bound / 1'000'000.0)));
  }
  NOTREACHED();
}

// TODO(crbug.com/405187652) Check if we want the selection dialog footer to
// have multiple lines when the text doesn't fit into one line.
TextWithLink SelectBnplIssuerDialogControllerImpl::GetLinkText() const {
  TextWithLink text_with_link;
  std::u16string payments_settings_link_text = GetStringUTF16(
      IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_FOOTNOTE_HIDE_OPTION_PAYMENT_SETTINGS_LINK_TEXT);
  size_t offset = 0;
  text_with_link.text = GetStringFUTF16(
      IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_FOOTNOTE_HIDE_OPTION,
      payments_settings_link_text, &offset);

  text_with_link.offset =
      gfx::Range(offset, offset + payments_settings_link_text.length());

  return text_with_link;
}

}  // namespace autofill::payments
