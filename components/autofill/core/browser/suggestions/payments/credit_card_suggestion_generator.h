// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PAYMENTS_CREDIT_CARD_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PAYMENTS_CREDIT_CARD_SUGGESTION_GENERATOR_H_

#include "base/functional/function_ref.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/form_events/address_form_event_logger.h"
#include "components/autofill/core/browser/metrics/form_events/credit_card_form_event_logger.h"
#include "components/autofill/core/browser/metrics/payments/card_metadata_metrics.h"
#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/save_and_fill_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "components/autofill/core/common/form_data.h"
#include "components/prefs/pref_service.h"
#include "components/sync/service/sync_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

class AutofillClient;
class PaymentsDataManager;
class FormFieldData;

// A `SuggestionGenerator` for `FillingProduct::kCreditCard`.
//
// This class encapsulates logic used exclusively for generating credit card
// suggestions. Free functions, that are also used in TouchToFill feature,
// are still shared in payments_suggestion_generator.h file.
class CreditCardSuggestionGenerator : public SuggestionGenerator {
 public:
  explicit CreditCardSuggestionGenerator(
      AutofillClient* client,
      const std::vector<std::string>& four_digit_combinations_in_dom,
      autofill_metrics::CreditCardFormEventLogger*
          credit_card_form_event_logger,
      autofill_metrics::AddressFormEventLogger* address_form_event_logger);
  ~CreditCardSuggestionGenerator() override;

  void FetchSuggestionData(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client,
      base::OnceCallback<
          void(std::pair<SuggestionDataSource,
                         std::vector<SuggestionGenerator::SuggestionData>>)>
          callback) override;

  void GenerateSuggestions(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
          all_suggestion_data,
      base::OnceCallback<void(ReturnedSuggestions)> callback) override;

  // Like SuggestionGenerator override, but takes a base::FunctionRef instead of
  // a base::OnceCallback. Calls that callback exactly once.
  // TODO(crbug.com/409962888): Clean up after launch.
  void FetchSuggestionData(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client,
      base::FunctionRef<void(std::pair<SuggestionDataSource,
                                       std::vector<SuggestionData>>)> callback);

  // Like SuggestionGenerator override, but takes a base::FunctionRef instead of
  // a base::OnceCallback. Calls that callback exactly once.
  // TODO(crbug.com/409962888): Clean up after launch.
  void GenerateSuggestions(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
          all_suggestion_data,
      base::FunctionRef<void(ReturnedSuggestions)> callback);

  void SetConsiderFromAsSecureForTest(bool value) {
    consider_form_as_secure_for_testing_ = value;
  }

 private:
  bool ShouldShowCreditCardSaveAndFill(bool is_complete_form,
                                       const FormFieldData& trigger_field);

  bool ShouldShowScanCreditCard(const FormData& form,
                                const FormFieldData& trigger_field,
                                const AutofillField* autofill_field);

  bool IsFormNonSecure(const FormData& form) const;

  base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>
  GetVirtualCreditCardsForStandaloneCvcField(const url::Origin&);

  std::vector<CreditCard> FetchCreditCardsForCreditCardOrCvcField(
      const AutofillClient& client,
      const FormFieldData& trigger_field,
      const std::vector<std::string>& four_digit_combinations_in_dom,
      const std::u16string& autofilled_last_four_digits_in_form_for_filtering,
      FieldType trigger_field_type,
      bool should_show_scan_credit_card);

  std::map<std::string, const AutofillOfferData*> GetCardLinkedOffers();

  void FilterCardsToSuggestForCvcFields(
      FieldType trigger_field_type,
      const std::u16string& autofilled_last_four_digits_in_form_for_filtering,
      std::vector<CreditCard>& cards_to_suggest);

  std::vector<CreditCard> FetchVirtualCardsForStandaloneCvcField(
      const FormFieldData& trigger_field);

  PaymentsDataManager* payments_data_manager() const {
    return &client_->GetPersonalDataManager().payments_data_manager();
  }

  payments::SaveAndFillManager* save_and_fill_manager() const {
    return client_->GetPaymentsAutofillClient()->GetSaveAndFillManager();
  }

  PrefService* pref_service() const { return client_->GetPrefs(); }

  syncer::SyncService* sync_service() const {
    return client_->GetSyncService();
  }

  LogManager* log_manager() const { return client_->GetCurrentLogManager(); }

  payments::PaymentsAutofillClient* payments_autofill_client() const {
    return client_->GetPaymentsAutofillClient();
  }

  raw_ptr<AutofillClient> client_;

  const std::vector<std::string> four_digit_combinations_in_dom_;

  // TODO(crbug.com/409962888): Make naming consistent after moving all logic.
  std::optional<bool> consider_form_as_secure_for_testing_;

  base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>
      virtual_card_guid_to_last_four_map_;

  base::WeakPtrFactory<CreditCardSuggestionGenerator> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PAYMENTS_CREDIT_CARD_SUGGESTION_GENERATOR_H_
