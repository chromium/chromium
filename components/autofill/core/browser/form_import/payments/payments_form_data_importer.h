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

namespace autofill {

class AutofillClient;
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

 private:
  friend class PaymentsFormDataImporterTestApi;
  // TODO(crbug.com/481379161): Remove `FormDataImporter` and
  //    `FormDataImporterTestApi` as friend classes once the FDI->PaymentsFDI
  //    migration is complete. This is very much not ideal and temporary, but
  //    the alternative is having most functions be public until the last
  //    second, which probably carries slightly higher risk.
  friend class autofill::FormDataImporter;
  friend class autofill::FormDataImporterTestApi;

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

  // Responsible for managing IBAN save flows.
  std::unique_ptr<IbanSaveManager> iban_save_manager_;

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
