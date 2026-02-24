// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_PAYMENTS_PAYMENTS_FORM_DATA_IMPORTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_PAYMENTS_PAYMENTS_FORM_DATA_IMPORTER_H_

#include <optional>
#include <string>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace autofill {

class AutofillClient;
class CreditCardSaveManager;
class FormDataImporter;
class FormDataImporterTestApi;
class FormStructure;
class Iban;
class IbanSaveManager;
enum class NonInteractivePaymentMethodType;
class PaymentsDataManager;

namespace payments {

// Owned by `FormDataImporter`. Responsible for payments-related form data
// importing functionality, usually on submission. Some examples of such
// functionality includes form extraction and processing, feature enrollment,
// and autofill table updating.
class PaymentsFormDataImporter {
 public:
  struct ExtractCreditCardFromFormResult {
    // The extracted credit card, which may be a candidate for import.
    // If there is no credit card field in the form, the value is the default
    // `CreditCard()`.
    CreditCard card;
    // If there are multiple credit card fields of the same type in the form, we
    // won't know which value to import.
    bool has_duplicate_credit_card_field_type = false;
  };

  // Context for most recently fetched payment method.
  struct FetchedPaymentsDataContext {
    // The instrument id of the card that has been most recently retrieved via
    // Autofill Downstream (card retrieval from server). This can be used to
    // decide whether the card submitted is the same card retrieved. This field
    // is optional and is set when an Autofill credit card Downstream has
    // happened.
    std::optional<int64_t> fetched_card_instrument_id;

    // Whether the last unmasked card (note: it may or may not be the extracted
    // card) is fetched from the local cache (instead of going through a server
    // retrieval process). This field is optional and is set when an Autofill
    // credit card Downstream has happened.
    std::optional<bool> card_was_fetched_from_cache;

    // Whether Save and Fill suggestion was clicked on for the last fetched
    // card. If so, no other payments post-checkout flow should be offered
    // again.
    bool card_submitted_through_save_and_fill = false;
  };

  // Record type of the credit card extracted from the form, if one exists.
  // TODO(crbug.com/40255227): Remove this enum and user CreditCard::RecordType
  // instead.
  enum CreditCardImportType {
    // No card was successfully extracted from the form.
    kNoCard,
    // The extracted card is already stored locally on the device.
    kLocalCard,
    // The extracted card is already known to be a server card (either masked or
    // unmasked).
    kServerCard,
    // The extracted card is not currently stored with the browser.
    kNewCard,
    // The extracted card is already known to be a virtual card.
    kVirtualCard,
    // The extracted card is known to be a duplicate local and server card.
    kDuplicateLocalServerCard,
  };

  explicit PaymentsFormDataImporter(AutofillClient* client);
  PaymentsFormDataImporter(const PaymentsFormDataImporter&) = delete;
  PaymentsFormDataImporter& operator=(const PaymentsFormDataImporter&) = delete;
  virtual ~PaymentsFormDataImporter();

  // Returns the extracted IBAN from the `form` if it is a new IBAN.
  std::optional<Iban> ExtractIban(const FormStructure& form);

  // Cache the last four of the fetched virtual card so we don't offer saving
  // them.
  void CacheFetchedVirtualCard(const std::u16string& last_four);

  FetchedPaymentsDataContext& fetched_payments_data_context() {
    return fetched_payments_data_context_;
  }

  // Tries to initiate the saving of `extracted_iban` if applicable.
  bool ProcessIbanImportCandidate(Iban& extracted_iban);

  // This should only set
  // `payment_method_type_if_non_interactive_authentication_flow_completed_` to
  // a value when there was an autofill with no interactive authentication,
  // otherwise it should set to nullopt.
  void SetPaymentMethodTypeIfNonInteractiveAuthenticationFlowCompleted(
      std::optional<NonInteractivePaymentMethodType>
          payment_method_type_if_non_interactive_authentication_flow_completed);

  // Extracts credit card from the form structure.
  ExtractCreditCardFromFormResult ExtractCreditCardFromForm(
      const FormStructure& form);

  // Returns true if the extracted credit card should be processed, false
  // otherwise.
  bool ShouldProcessExtractedCreditCard();

  CreditCardSaveManager* GetCreditCardSaveManager() {
    return credit_card_save_manager_.get();
  }

 private:
  friend class PaymentsFormDataImporterTestApi;
  // TODO(crbug.com/481379161): Remove `FormDataImporter` and
  //    `FormDataImporterTestApi` as friend classes once the FDI->PaymentsFDI
  //    migration is complete. This is very much not ideal and temporary, but
  //    the alternative is having most functions be public until the last
  //    second, which probably carries slightly higher risk.
  friend class autofill::FormDataImporter;
  friend class autofill::FormDataImporterTestApi;

  // Returns the extracted card if one was found in the form.
  //
  // The returned card is, unless nullopt,
  // - a matching server card, if any match is found, or
  // - the candidate input card, augmented with a matching local card's nickname
  //   if such any match is found.
  // It is nullopt under the following conditions:
  // - if the card number is invalid;
  // - if the card is a known virtual card;
  // - if a card matches but the extracted card has no expiration date.
  //
  // The function has two side-effects:
  // - all matching local cards are updated to include the information from the
  //   extracted card;
  // - `credit_card_import_type_` is set to
  //   - SERVER_CARD if a server card matches;
  //   - LOCAL_CARD if a local and no server card matches;
  //   - NEW_CARD otherwise.
  std::optional<CreditCard> ExtractCreditCard(const FormStructure& form);

  // Tries to initiate the saving of the `extracted_credit_card` if applicable.
  // `submitted_form` is the form from which the card was
  // imported. `is_credit_card_upstream_enabled` indicates if server card
  // storage is enabled. Returns true if a save is initiated.
  bool ProcessExtractedCreditCard(
      const FormStructure& submitted_form,
      const std::optional<CreditCard>& extracted_credit_card,
      bool is_credit_card_upstream_enabled,
      ukm::SourceId ukm_source_id);

  // If the mandatory re-auth opt-in bubble can be shown for a credit card, this
  // function will start the flow and return true. Otherwise, it will return
  // false.
  bool ProceedWithCardMandatoryReauthOptInIfApplicable();

  // Helper function which extracts the IBAN from the form structure.
  Iban ExtractIbanFromForm(const FormStructure& form);

  PaymentsDataManager& payments_data_manager();

  const raw_ref<AutofillClient> client_;

  // Used to store the last four digits of the fetched virtual cards.
  base::flat_set<std::u16string> fetched_virtual_cards_;

  // Struct to record contexts for the last payments data fetch. Should be reset
  // when a new fetch starts.
  FetchedPaymentsDataContext fetched_payments_data_context_;

  // Responsible for managing credit card save flows (local or upload).
  std::unique_ptr<CreditCardSaveManager> credit_card_save_manager_;

  // Responsible for managing IBAN save flows.
  std::unique_ptr<IbanSaveManager> iban_save_manager_;

  // Represents the type of the credit card import candidate from the submitted
  // form. It will be used to determine whether to offer upload save or not.
  // Will be passed to `credit_card_save_manager_` for metrics. If no credit
  // card was found in the form, the type will be `kNoCard`.
  payments::PaymentsFormDataImporter::CreditCardImportType
      credit_card_import_type_ =
          payments::PaymentsFormDataImporter::CreditCardImportType::kNoCard;

  // If the most recent payments autofill flow had a non-interactive
  // authentication,
  // `payment_method_type_if_non_interactive_authentication_flow_completed_`
  // will contain the type of payment method that had the non-interactive
  // authentication, otherwise it will be nullopt. This is for logging purposes
  // to log the type of non interactive payment method type that triggers
  // mandatory reauth.
  std::optional<NonInteractivePaymentMethodType>
      payment_method_type_if_non_interactive_authentication_flow_completed_;
};

}  // namespace payments

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_PAYMENTS_PAYMENTS_FORM_DATA_IMPORTER_H_
