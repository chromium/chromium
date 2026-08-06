// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SAVE_PAYMENT_METHOD_AND_VIRTUAL_CARD_ENROLL_CONFIRMATION_UI_PARAMS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SAVE_PAYMENT_METHOD_AND_VIRTUAL_CARD_ENROLL_CONFIRMATION_UI_PARAMS_H_

#include <optional>
#include <string>
#include <tuple>

#include "base/functional/callback.h"
#include "base/types/strong_alias.h"

namespace autofill {

// The UI params for both the Save Card/IBAN and Virtual Card Enrollment
// confirmation dialogs. Since both dialogs have the same basic structure
// (title, description, and success/failure state), they share UI params to
// avoid code duplication.
// TODO(crbug.com/524740910): Rename to a more generic name as more projects
// (such as churned users) are using this.
struct SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams {
  using DescriptionTextLinkStart =
      base::StrongAlias<struct DescriptionTextLinkStartTag, size_t>;
  using DescriptionTextLinkEnd =
      base::StrongAlias<struct DescriptionTextLinkEndTag, size_t>;

  ~SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams();

  SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams(
      const SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams&);
  SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams& operator=(
      const SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams&);

  SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams(
      SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams&&);
  SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams& operator=(
      SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams&&);

  // Static creator methods for the UI params in its various supported states.
  static SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams
  CreateForSaveCardSuccess(bool is_for_save_and_fill);
  static SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams
  CreateForVirtualCardSuccess();
  static SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams
  CreateForChurnedUsersAcceptanceSuccess(base::RepeatingClosure link_callback);
  static SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams
  CreateForSaveCardFailure(bool is_for_save_and_fill);
  static SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams
  CreateForVirtualCardFailure(const std::u16string card_label);
  static SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams
  CreateForSaveIbanSuccess();
  static SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams
  CreateForSaveIbanFailure(bool hit_max_strikes);

  bool is_success;
  bool should_display_wallet_logo;
  std::u16string title_text;
  std::u16string description_text;
  std::u16string failure_ok_button_text;
  std::optional<std::tuple<DescriptionTextLinkStart,
                           DescriptionTextLinkEnd,
                           base::RepeatingClosure>>
      description_text_link_range_and_callback;

 private:
  SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams(
      bool is_success,
      bool should_display_wallet_logo,
      std::u16string title_text,
      std::u16string description_text,
      std::optional<std::tuple<DescriptionTextLinkStart,
                               DescriptionTextLinkEnd,
                               base::RepeatingClosure>>
          description_text_link_range_and_callback = std::nullopt);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SAVE_PAYMENT_METHOD_AND_VIRTUAL_CARD_ENROLL_CONFIRMATION_UI_PARAMS_H_
