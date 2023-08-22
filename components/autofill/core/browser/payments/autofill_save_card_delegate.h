// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_SAVE_CARD_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_SAVE_CARD_DELEGATE_H_

#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace autofill {

class AutofillSaveCardInfoBarDelegateMobileTest;

// Delegate class providing callbacks for UIs presenting save card offers.
class AutofillSaveCardDelegate {
 public:
  AutofillSaveCardDelegate(
      absl::variant<AutofillClient::LocalSaveCardPromptCallback,
                    AutofillClient::UploadSaveCardPromptCallback> callback,
      AutofillClient::SaveCreditCardOptions options);

  ~AutofillSaveCardDelegate();

  bool is_for_upload() const {
    return absl::holds_alternative<
        AutofillClient::UploadSaveCardPromptCallback>(callback_);
  }

  void OnUiShown();
  void OnUiAccepted();
  void OnUiUpdatedAndAccepted(
      AutofillClient::UserProvidedCardDetails user_provided_details);
  void OnUiCanceled();
  void OnUiIgnored();

 private:
  friend class AutofillSaveCardInfoBarDelegateMobileTest;

  // Runs the appropriate local or upload save callback with the given
  // |user_decision|, using the |user_provided_details|. If
  // |user_provided_details| is empty then the current Card values will be used.
  // The cardholder name and expiration date portions of
  // |user_provided_details| are handled separately, so if either of them are
  // empty the current card values will be used.
  void RunSaveCardPromptCallback(
      AutofillClient::SaveCardOfferUserDecision user_decision,
      AutofillClient::UserProvidedCardDetails user_provided_details);

  void LogUserAction(AutofillMetrics::InfoBarMetric user_action);

  // If the cardholder name is missing, request the name from the user before
  // saving the card. If the expiration date is missing, request the missing
  // data from the user before saving the card.
  AutofillClient::SaveCreditCardOptions options_;

  // Did the user ever explicitly accept or dismiss this UI?
  bool had_user_interaction_;

  // The callback to run once the user makes a decision with respect to the
  // credit card offer-to-save prompt.
  absl::variant<AutofillClient::LocalSaveCardPromptCallback,
                AutofillClient::UploadSaveCardPromptCallback>
      callback_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_SAVE_CARD_DELEGATE_H_
