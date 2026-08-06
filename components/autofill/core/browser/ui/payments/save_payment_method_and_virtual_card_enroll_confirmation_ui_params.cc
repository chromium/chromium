// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/save_payment_method_and_virtual_card_enroll_confirmation_ui_params.h"

#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams::
    ~SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams() = default;

SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams::
    SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams(
        const SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams&) = default;
SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams&
SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams::operator=(
    const SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams&) = default;

SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams::
    SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams(
        SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams&&) = default;
SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams&
SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams::operator=(
    SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams&&) = default;

SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams::
    SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams(
        bool is_success,
        bool should_display_wallet_logo,
        std::u16string title_text,
        std::u16string description_text,
        std::optional<std::tuple<DescriptionTextLinkStart,
                                 DescriptionTextLinkEnd,
                                 base::RepeatingClosure>>
            description_text_link_range_and_callback)
    : is_success(is_success),
      should_display_wallet_logo(should_display_wallet_logo),
      title_text(std::move(title_text)),
      description_text(std::move(description_text)),
      failure_ok_button_text(
          is_success
              ? std::u16string()
              : l10n_util::GetStringUTF16(
                    IDS_AUTOFILL_SAVE_CARD_AND_VIRTUAL_CARD_ENROLL_CONFIRMATION_BUTTON_TEXT)),
      description_text_link_range_and_callback(
          std::move(description_text_link_range_and_callback)) {}

// static
SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams
SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams::
    CreateForSaveCardSuccess(bool is_for_save_and_fill) {
  return SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams(
      /*is_success=*/true,
      /*should_display_wallet_logo=*/
      !base::FeatureList::IsEnabled(features::kAutofillEnableWalletBranding) ||
          !is_for_save_and_fill,
      /*title_text=*/
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_SUCCESS_TITLE_TEXT),
      /*description_text=*/
      l10n_util::GetStringUTF16(
          base::FeatureList::IsEnabled(features::kAutofillEnableWalletBranding)
              ? (base::FeatureList::IsEnabled(
                     features::kAutofillEnableWalletBrandingV2)
                     ? IDS_AUTOFILL_SAVE_CARD_TO_WALLET_CONFIRMATION_SUCCESS_DESCRIPTION_TEXT_V2
                     : IDS_AUTOFILL_SAVE_CARD_TO_WALLET_CONFIRMATION_SUCCESS_DESCRIPTION_TEXT)
              : IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_SUCCESS_DESCRIPTION_TEXT));
}

// static
SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams
SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams::
    CreateForVirtualCardSuccess() {
  return SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams(
      /*is_success=*/true,
      /*should_display_wallet_logo=*/
      !base::FeatureList::IsEnabled(features::kAutofillEnableWalletBranding),
      /*title_text=*/
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_ENROLL_CONFIRMATION_SUCCESS_TITLE_TEXT),
      /*description_text=*/
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_ENROLL_CONFIRMATION_SUCCESS_DESCRIPTION_TEXT));
}

// static
SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams
SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams::
    CreateForChurnedUsersAcceptanceSuccess(
        base::RepeatingClosure link_callback) {
  std::u16string description = l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_CHURNED_USERS_CONFIRMATION_BUBBLE_DESCRIPTION,
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CHURNED_USERS_CONFIRMATION_BUBBLE_LINK_TEXT));
  size_t link_start = description.find(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_CHURNED_USERS_CONFIRMATION_BUBBLE_LINK_TEXT));
  CHECK(link_start != std::string::npos);
  size_t link_end =
      link_start + l10n_util::GetStringUTF16(
                       IDS_AUTOFILL_CHURNED_USERS_CONFIRMATION_BUBBLE_LINK_TEXT)
                       .length();
  return SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams(
      /*is_success=*/true,
      /*should_display_wallet_logo=*/
      false,
      /*title_text=*/
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CHURNED_USERS_CONFIRMATION_BUBBLE_TITLE),
      /*description_text=*/
      std::move(description),
      /*description_text_link_range_and_callback=*/
      std::make_tuple(DescriptionTextLinkStart(link_start),
                      DescriptionTextLinkEnd(link_end),
                      std::move(link_callback)));
}

// static
SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams
SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams::
    CreateForSaveCardFailure(bool is_for_save_and_fill) {
  return SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams(
      /*is_success=*/false,
      /*should_display_wallet_logo=*/
      !base::FeatureList::IsEnabled(features::kAutofillEnableWalletBranding) ||
          !is_for_save_and_fill,
      /*title_text=*/
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_FAILURE_TITLE_TEXT),
      /*description_text=*/
      l10n_util::GetStringUTF16(
          base::FeatureList::IsEnabled(features::kAutofillEnableWalletBranding)
              ? IDS_AUTOFILL_SAVE_CARD_TO_WALLET_CONFIRMATION_FAILURE_DESCRIPTION_TEXT
              : IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_FAILURE_DESCRIPTION_TEXT));
}

// static
SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams
SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams::
    CreateForVirtualCardFailure(const std::u16string card_label) {
  return SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams(
      /*is_success=*/false,
      /*should_display_wallet_logo=*/true,
      /*title_text=*/
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_ENROLL_CONFIRMATION_FAILURE_TITLE_TEXT),
      /*description_text=*/
      l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_ENROLL_CONFIRMATION_FAILURE_DESCRIPTION_TEXT,
          card_label));
}

// static
SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams
SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams::
    CreateForSaveIbanSuccess() {
  return SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams(
      /*is_success=*/true,
      /*should_display_wallet_logo=*/true,
      /*title_text=*/
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_IBAN_CONFIRMATION_SUCCESS_TITLE_TEXT),
      /*description_text=*/
      l10n_util::GetStringUTF16(
          base::FeatureList::IsEnabled(features::kAutofillEnableWalletBranding)
              ? IDS_AUTOFILL_SAVE_IBAN_TO_WALLET_CONFIRMATION_SUCCESS_DESCRIPTION_TEXT
              : IDS_AUTOFILL_SAVE_IBAN_CONFIRMATION_SUCCESS_DESCRIPTION_TEXT));
}

// static
SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams
SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams::
    CreateForSaveIbanFailure(bool hit_max_strikes) {
  return SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams(
      /*is_success=*/false,
      /*should_display_wallet_logo=*/true,
      /*title_text=*/
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_IBAN_CONFIRMATION_FAILURE_TITLE_TEXT),
      /*description_text=*/
      hit_max_strikes
          ? l10n_util::GetStringUTF16(
                base::FeatureList::IsEnabled(
                    features::kAutofillEnableWalletBranding)
                    ? IDS_AUTOFILL_SAVE_IBAN_TO_WALLET_CONFIRMATION_FAILURE_HIT_MAX_STRIKE_DESCRIPTION_TEXT
                    : IDS_AUTOFILL_SAVE_IBAN_CONFIRMATION_FAILURE_HIT_MAX_STRIKE_DESCRIPTION_TEXT)
          : l10n_util::GetStringUTF16(
                base::FeatureList::IsEnabled(
                    features::kAutofillEnableWalletBranding)
                    ? IDS_AUTOFILL_SAVE_IBAN_TO_WALLET_CONFIRMATION_FAILURE_DESCRIPTION_TEXT
                    : IDS_AUTOFILL_SAVE_IBAN_CONFIRMATION_FAILURE_DESCRIPTION_TEXT));
}

}  // namespace autofill
