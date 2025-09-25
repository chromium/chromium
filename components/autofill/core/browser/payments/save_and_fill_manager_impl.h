// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_SAVE_AND_FILL_MANAGER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_SAVE_AND_FILL_MANAGER_IMPL_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/metrics/payments/save_and_fill_metrics.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/payments/save_and_fill_manager.h"
#include "components/autofill/core/browser/strike_databases/payments/save_and_fill_strike_database.h"

namespace autofill::payments {

class PaymentsAutofillClient;

// Owned by PaymentsAutofillClient. There is one instance of this class per Web
// Contents. This class manages the flow for the Save and Fill dialog.
class SaveAndFillManagerImpl : public SaveAndFillManager {
 public:
  // A struct containing a few logging related info. This is not reset when flow
  // ends as metrics loggings are once per page load.
  struct LoggingContext {
    // Set to true after the first time the Save and Fill suggestion not being
    // shown is logged. Ensures that logging occurs only once per page load.
    bool has_logged_save_and_fill_suggestion_not_shown_reason = false;

    // Set when the Save and Fill dialog was accepted by the user. Records
    // whether the attempt was for upload / local save and whether the attempt
    // succeeded.
    std::optional<bool> last_attempt_succeeded;
    std::optional<bool> last_attempt_was_for_upload;

    // Whether credit card form filling / submission event has been logged.
    // Ensures that logging occurs only once per page load.
    bool has_logged_form_filled = false;
    bool has_logged_form_submitted = false;
  };

  explicit SaveAndFillManagerImpl(AutofillClient* autofill_client);
  SaveAndFillManagerImpl(const SaveAndFillManagerImpl& other) = delete;
  SaveAndFillManagerImpl& operator=(const SaveAndFillManagerImpl& other) =
      delete;
  ~SaveAndFillManagerImpl() override;

  // SaveAndFillManager:
  void OnDidAcceptCreditCardSaveAndFillSuggestion(
      FillCardCallback fill_card_callback) override;
  void OnSuggestionOffered() override;
  void MaybeAddStrikeForSaveAndFill() override;
  bool ShouldBlockFeature() override;
  void MaybeLogSaveAndFillSuggestionNotShownReason(
      autofill_metrics::SaveAndFillSuggestionNotShownReason reason) override;
  void LogCreditCardFormFilled() override;
  void LogCreditCardFormSubmitted() override;

  // Called when the user makes a decision on the local Save and Fill dialog.
  // The `user_provided_card_save_and_fill_details` holds the  data entered by
  // the user in the Save and Fill dialog when the `user_decision` is
  // `kAccepted`.
  void OnUserDidDecideOnLocalSave(
      CardSaveAndFillDialogUserDecision user_decision,
      const UserProvidedCardSaveAndFillDetails&
          user_provided_card_save_and_fill_details);

  void SetCreditCardUploadEnabledOverrideForTesting(
      bool credit_card_upload_enabled_override);

 private:
  FRIEND_TEST_ALL_PREFIXES(SaveAndFillManagerImplTest,
                           OnUserDidDecideOnUploadSave_Accepted);
  FRIEND_TEST_ALL_PREFIXES(SaveAndFillManagerImplTest,
                           ResetOnFlowEnds_ServerSave);
  FRIEND_TEST_ALL_PREFIXES(SaveAndFillManagerImplTest,
                           ResetOnFlowEnds_LocalSave);
  FRIEND_TEST_ALL_PREFIXES(SaveAndFillManagerImplTest,
                           LogFunnelMetrics_ServerSave);

  // Begins the process to show the local Save and Fill dialog.
  void OfferLocalSaveAndFill();

  // Populates a new credit card object with user provided card details from the
  // Save and Fill dialog. This is called after the user provides credit card
  // information and accepts the dialog.
  void PopulateCreditCardInfo(CreditCard& card,
                              const UserProvidedCardSaveAndFillDetails&
                                  user_provided_card_save_and_fill_details);

  // Whether all prerequisites for credit card uploading are met.
  bool IsCreditCardUploadEnabled() const;

  // Callback invoked when the response to fetch upload details is returned.
  void OnDidGetDetailsForCreateCard(
      base::TimeTicks request_sent_timestamp,
      PaymentsAutofillClient::PaymentsRpcResult result,
      const std::u16string& context_token,
      std::unique_ptr<base::Value::Dict> legal_message,
      std::vector<std::pair<int, int>> supported_card_bin_ranges);

  // If server upload is enabled, populate info to the `upload_details_` for
  // server communication.
  void PopulateInitialUploadDetails();

  // Begins the process to show the upload Save and Fill dialog.
  void OfferUploadSaveAndFill(
      const LegalMessageLines& parsed_legal_message_lines);

  // The callback that is invoked after the user makes a decision on the
  // upload Save and Fill dialog.
  void OnUserDidDecideOnUploadSave(
      CardSaveAndFillDialogUserDecision user_decision,
      const UserProvidedCardSaveAndFillDetails&
          user_provided_card_save_and_fill_details);

  // Callback invoked when risk data is fetched.
  void OnDidLoadRiskData(const std::string& risk_data);

  // Helper function to send CreateCard request to the server with the
  // `upload_details_`.
  void SendCreateCardRequest();

  // Callback invoked when the CreateCard response is received.
  void OnDidCreateCard(base::TimeTicks request_sent_timestamp,
                       PaymentsAutofillClient::PaymentsRpcResult result,
                       const std::string& instrument_id);

  // Function invoked when the flow ends. This resets the state variables in
  // this class.
  void Reset();

  // Returns the SaveAndFillStrikeDatabase for `autofill_client_`.
  SaveAndFillStrikeDatabase* GetSaveAndFillStrikeDatabase();

  PaymentsAutofillClient* payments_autofill_client() const {
    return autofill_client_->GetPaymentsAutofillClient();
  }

  // Reference to the AutofillClient. `autofill_client_` outlives `this`.
  const raw_ref<AutofillClient> autofill_client_;

  // Struct that contains necessary information for uploading the card to
  // server.
  payments::UploadCardRequestDetails upload_details_;

  FillCardCallback fill_card_callback_;

  // The supported card BIN ranges for card upload.
  std::vector<std::pair<int, int>> supported_card_bin_ranges_;

  // Boolean value indicates whether the upload Save and Fill dialog has been
  // accepted.
  bool upload_save_and_fill_dialog_accepted_ = false;
  // True if the Save and Fill suggestion was offered to the user.
  bool save_and_fill_suggestion_offered_ = false;
  // True if the user accepted the Save and Fill suggestion.
  bool save_and_fill_suggestion_selected_ = false;
  // True if any strike has been logged for form submission in this session.
  // Note this variable is not reset once set to true, meaning that a
  // SaveAndFillManagerImpl instance only adds at most one strike for cases
  // where a Save-and-Fill suggestion is shown but not accepted.
  bool has_logged_strikes_for_form_submission_ = false;

  LoggingContext logging_context_;

  // StrikeDatabase used to check whether to show the Save and Fill suggestion.
  std::unique_ptr<SaveAndFillStrikeDatabase> save_and_fill_strike_database_;

  base::WeakPtrFactory<SaveAndFillManagerImpl> weak_ptr_factory_{this};
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_SAVE_AND_FILL_MANAGER_IMPL_H_
