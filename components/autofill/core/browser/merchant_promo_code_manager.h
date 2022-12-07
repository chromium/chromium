// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_MERCHANT_PROMO_CODE_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_MERCHANT_PROMO_CODE_MANAGER_H_

#include "components/autofill/core/browser/autofill_subject.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/single_field_form_filler.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/webdata/common/web_data_service_consumer.h"

namespace autofill {

class AutofillClient;
class AutofillOfferData;
class PersonalDataManager;
struct SuggestionsContext;

// Per-profile Merchant Promo Code Manager. This class handles promo code
// related functionality such as retrieving promo code offer data, managing
// promo code suggestions, filling promo code fields, and handling form
// submission data when there is a merchant promo code field present.
class MerchantPromoCodeManager : public SingleFieldFormFiller,
                                 public KeyedService,
                                 public AutofillSubject {
 public:
  MerchantPromoCodeManager();

  MerchantPromoCodeManager(const MerchantPromoCodeManager&) = delete;
  MerchantPromoCodeManager& operator=(const MerchantPromoCodeManager&) = delete;

  ~MerchantPromoCodeManager() override;

  // SingleFieldFormFiller overrides:
  [[nodiscard]] bool OnGetSingleFieldSuggestions(
      AutoselectFirstSuggestion autoselect_first_suggestion,
      const FormFieldData& field,
      const AutofillClient& client,
      base::WeakPtr<SuggestionsHandler> handler,
      const SuggestionsContext& context) override;
  void OnWillSubmitFormWithFields(const std::vector<FormFieldData>& fields,
                                  bool is_autocomplete_enabled) override;
  void CancelPendingQueries(const SuggestionsHandler* handler) override;
  void OnRemoveCurrentSingleFieldSuggestion(const std::u16string& field_name,
                                            const std::u16string& value,
                                            int frontend_id) override;
  void OnSingleFieldSuggestionSelected(const std::u16string& value,
                                       int frontend_id) override;

  // Initializes the instance with the given parameters. |personal_data_manager|
  // is a profile-scope data manager used to retrieve promo code offers from the
  // local autofill table. |is_off_the_record| indicates whether the user is
  // currently operating in an off-the-record context (i.e. incognito).
  void Init(PersonalDataManager* personal_data_manager, bool is_off_the_record);

  // Returns a weak pointer to the current MerchantPromoCodeManager
  // instance.
  base::WeakPtr<MerchantPromoCodeManager> GetWeakPtr();

 private:
  friend class MerchantPromoCodeManagerTest;
  FRIEND_TEST_ALL_PREFIXES(MerchantPromoCodeManagerTest,
                           DoesNotShowPromoCodeOffersForOffTheRecord);
  FRIEND_TEST_ALL_PREFIXES(
      MerchantPromoCodeManagerTest,
      DoesNotShowPromoCodeOffersIfPersonalDataManagerDoesNotExist);

  // Records metrics related to the offers suggestions popup.
  class UMARecorder {
   public:
    UMARecorder() = default;

    UMARecorder(const UMARecorder&) = delete;
    UMARecorder& operator=(const UMARecorder&) = delete;

    ~UMARecorder() = default;

    void OnOffersSuggestionsShown(
        const FieldGlobalId& field_global_id,
        const std::vector<const AutofillOfferData*>& offers);
    void OnOfferSuggestionSelected(int frontend_id);

   private:
    // The global id of the field that most recently had suggestions shown.
    FieldGlobalId most_recent_suggestions_shown_field_global_id_;

    // The global id of the field that most recently had a suggestion selected.
    FieldGlobalId most_recent_suggestion_selected_field_global_id_;
  };

  // Sends suggestions for `promo_code_offers` to the `query_handler`'s handler
  // for display in the associated Autofill popup. If suggestions were
  // displayed, this function also logs metrics for promo code suggestions
  // shown. `field_global_id` is used for this metrics logging, as it checks
  // whether the field where promo code suggestions are being shown has just had
  // suggestions shown. This ensures we to log to the correct histogram, as we
  // have separate histograms for unique shows and repetitive shows.
  void SendPromoCodeSuggestions(
      const std::vector<const AutofillOfferData*>& promo_code_offers,
      const FieldGlobalId& field_global_id,
      const QueryHandler& query_handler);

  raw_ptr<PersonalDataManager> personal_data_manager_ = nullptr;

  bool is_off_the_record_ = false;

  UMARecorder uma_recorder_;

  base::WeakPtrFactory<MerchantPromoCodeManager> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_MERCHANT_PROMO_CODE_MANAGER_H_
