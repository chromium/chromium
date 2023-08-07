// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_BROWSER_AUTOFILL_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_BROWSER_AUTOFILL_MANAGER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_ablation_study.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/field_filler.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_autofill_history.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/metrics/fallback_autocomplete_unrecognized_metrics.h"
#include "components/autofill/core/browser/metrics/form_events/address_form_event_logger.h"
#include "components/autofill/core/browser/metrics/form_events/credit_card_form_event_logger.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/payments/card_unmask_delegate.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments/full_card_request.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/single_field_form_fill_router.h"
#include "components/autofill/core/browser/sync_utils.h"
#include "components/autofill/core/browser/ui/fast_checkout_delegate.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/popup_types.h"
#include "components/autofill/core/browser/ui/touch_to_fill_delegate.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/signatures.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace gfx {
class RectF;
}

namespace autofill {

class AutofillField;
class AutofillClient;
class AutofillSuggestionGenerator;
class BrowserAutofillManagerTestDelegate;
class AutofillProfile;
class AutofillType;
class CreditCard;

struct FormData;
struct FormFieldData;
struct SuggestionsContext;

// Use <Phone><WebOTP><OTC> as the bit pattern to identify the metrics state.
enum class PhoneCollectionMetricState {
  kNone = 0,    // Site did not collect phone, not use OTC, not use WebOTP
  kOTC = 1,     // Site used OTC only
  kWebOTP = 2,  // Site used WebOTP only
  kWebOTPPlusOTC = 3,  // Site used WebOTP and OTC
  kPhone = 4,          // Site collected phone, not used neither WebOTP nor OTC
  kPhonePlusOTC = 5,   // Site collected phone number and used OTC
  kPhonePlusWebOTP = 6,         // Site collected phone number and used WebOTP
  kPhonePlusWebOTPPlusOTC = 7,  // Site collected phone number and used both
  kMaxValue = kPhonePlusWebOTPPlusOTC,
};

namespace phone_collection_metric {
constexpr uint32_t kOTCUsed = 1 << 0;
constexpr uint32_t kWebOTPUsed = 1 << 1;
constexpr uint32_t kPhoneCollected = 1 << 2;
}  // namespace phone_collection_metric

// Enum for the value patterns metric. Don't renumerate existing value. They are
// used for metrics.
enum class ValuePatternsMetric {
  kNoPatternFound = 0,
  kUpiVpa = 1,  // UPI virtual payment address.
  kIban = 2,    // International Bank Account Number.
  kMaxValue = kIban,
};

// Manages saving and restoring the user's personal information entered into web
// forms. One per frame; owned by the AutofillDriver.
class BrowserAutofillManager : public AutofillManager,
                               public SingleFieldFormFiller::SuggestionsHandler,
                               public CreditCardAccessManager::Accessor {
 public:
  BrowserAutofillManager(AutofillDriver* driver,
                         AutofillClient* client,
                         const std::string& app_locale);

  BrowserAutofillManager(const BrowserAutofillManager&) = delete;
  BrowserAutofillManager& operator=(const BrowserAutofillManager&) = delete;

  ~BrowserAutofillManager() override;

  void ShowAutofillSettings(PopupType popup_type);

  // Whether the |field| should show an entry to scan a credit card.
  virtual bool ShouldShowScanCreditCard(const FormData& form,
                                        const FormFieldData& field);

  // Returns the type of the popup being shown.
  virtual PopupType GetPopupType(const FormData& form,
                                 const FormFieldData& field);

  // Handlers for the "Show Cards From Account" row. This row should be shown to
  // users who have cards in their account and can use Sync Transport. Clicking
  // the row records the user's consent to see these cards on this device, and
  // refreshes the popup.
  virtual bool ShouldShowCardsFromAccountOption(const FormData& form,
                                                const FormFieldData& field);
  virtual void OnUserAcceptedCardsFromAccountOption();
  virtual void RefetchCardsAndUpdatePopup(const FormData& form,
                                          const FormFieldData& field_data);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // Returns the list of credit cards that have associated cloud token data.
  virtual void FetchVirtualCardCandidates();

  // Callback invoked when an actual card is selected. |selected_card_id| will
  // be used to identify the card. The selected card's cloud token data will be
  // fetched from the server.
  // TODO(crbug.com/1020740): Passes card server id for now. In the future when
  // one actual credit card can have multiple virtual cards, passes instrument
  // token instead. Design TBD.
  virtual void OnVirtualCardCandidateSelected(
      const std::string& selected_card_id);
#endif

  // Called from our external delegate so they cannot be private.
  // TODO(crbug.com/1330108): Clean up the API.
  virtual void FillOrPreviewForm(
      mojom::AutofillActionPersistence action_persistence,
      const FormData& form,
      const FormFieldData& field,
      Suggestion::BackendId backend_id,
      const AutofillTriggerSource trigger_source);
  void FillCreditCardFormImpl(const FormData& form,
                              const FormFieldData& field,
                              const CreditCard& credit_card,
                              const std::u16string& cvc,
                              AutofillTriggerSource trigger_source) override;
  // Reverts the last autofill operation on `form` that affected
  // `trigger_field`, virtual for testing. `renderer_action` denotes whether
  // this is an actual filling or a preview operation on the renderer side.
  virtual void UndoAutofill(mojom::AutofillActionPersistence action_persistence,
                            FormData form,
                            const FormFieldData& trigger_field);
  // Virtual for testing
  virtual void DidShowSuggestions(bool has_autofill_suggestions,
                                  const FormData& form,
                                  const FormFieldData& field);

  // Fills or previews the credit card form.
  // Assumes the form and field are valid.
  // Asks for authentication via CVC before filling with server card data.
  // TODO(crbug.com/1330108): Clean up the API.
  virtual void FillOrPreviewCreditCardForm(
      mojom::AutofillActionPersistence action_persistence,
      const FormData& form,
      const FormFieldData& field,
      const CreditCard* credit_card,
      const AutofillTriggerSource trigger_source);

  // TODO(crbug.com/1330108): Clean up the API.
  void FillProfileFormImpl(const FormData& form,
                           const FormFieldData& field,
                           const AutofillProfile& profile,
                           AutofillTriggerSource trigger_source) override;

  // Fetches the related virtual card information given the related actual card
  // |guid| and fills the information into the form.
  // TODO(crbug.com/1330108): Clean up the API.
  virtual void FillOrPreviewVirtualCardInformation(
      mojom::AutofillActionPersistence action_persistence,
      const std::string& guid,
      const FormData& form,
      const FormFieldData& field,
      const AutofillTriggerSource trigger_source);

  // Returns true if the value/identifier is deletable. Fills out
  // |title| and |body| with relevant user-facing text.
  bool GetDeletionConfirmationText(const std::u16string& value,
                                   PopupItemId popup_item_id,
                                   Suggestion::BackendId backend_id,
                                   std::u16string* title,
                                   std::u16string* body);

  // Remove the credit card or Autofill profile that matches |backend_id|
  // from the database. Returns true if deletion is allowed.
  bool RemoveAutofillProfileOrCreditCard(Suggestion::BackendId backend_id);

  // Remove the specified suggestion from single field filling. `popup_item_id`
  // is the PopupItemId of the suggestion.
  void RemoveCurrentSingleFieldSuggestion(const std::u16string& name,
                                          const std::u16string& value,
                                          PopupItemId popup_item_id);

  // Invoked when the user selected |value| in a suggestions list from single
  // field filling. `popup_item_id` is the PopupItemId of the suggestion.
  void OnSingleFieldSuggestionSelected(const std::u16string& value,
                                       PopupItemId popup_item_id,
                                       const FormData& form,
                                       const FormFieldData& field);

  // Invoked when the user selects the "Hide Suggestions" item in the
  // Autocomplete drop-down.
  virtual void OnUserHideSuggestions(const FormData& form,
                                     const FormFieldData& field);

  const std::string& app_locale() const { return app_locale_; }

  // Only for testing.
  void SetTestDelegate(BrowserAutofillManagerTestDelegate* delegate);

  // Will send an upload based on the |form_structure| data and the local
  // Autofill profile data. |observed_submission| is specified if the upload
  // follows an observed submission event. Returns false if the upload couldn't
  // start.
  virtual bool MaybeStartVoteUploadProcess(
      std::unique_ptr<FormStructure> form_structure,
      bool observed_submission);

  // Update the pending form with |form|, possibly processing the current
  // pending form for upload.
  void UpdatePendingForm(const FormData& form);

  // Upload the current pending form.
  void ProcessPendingFormForUpload();

  // Invoked when the popup view can't be created. Main usage is to collect
  // metrics.
  void DidSuppressPopup(const FormData& form, const FormFieldData& field);

  // Invoked when the "suggestions" popup is hidden.
  void DidHidePopup();

  // AutofillManager:
  base::WeakPtr<AutofillManager> GetWeakPtr() override;
  CreditCardAccessManager* GetCreditCardAccessManager() override;
  bool ShouldClearPreviewedForm() override;
  void OnFocusNoLongerOnFormImpl(bool had_interacted_form) override;
  void OnFocusOnFormFieldImpl(const FormData& form,
                              const FormFieldData& field,
                              const gfx::RectF& bounding_box) override;
  void OnDidFillAutofillFormDataImpl(const FormData& form,
                                     const base::TimeTicks timestamp) override;
  void OnDidPreviewAutofillFormDataImpl() override;
  void OnDidEndTextFieldEditingImpl() override;
  void OnHidePopupImpl() override;
  void OnSelectOrSelectMenuFieldOptionsDidChangeImpl(
      const FormData& form) override;
  void OnJavaScriptChangedAutofilledValueImpl(
      const FormData& form,
      const FormFieldData& field,
      const std::u16string& old_value) override;
  void PropagateAutofillPredictionsDeprecated(
      const std::vector<FormStructure*>& forms) override;
  void Reset() override;
  void OnContextMenuShownInField(const FormGlobalId& form_global_id,
                                 const FieldGlobalId& field_global_id) override;
  // SingleFieldFormFiller::SuggestionsHandler:
  void OnSuggestionsReturned(
      FieldGlobalId field_id,
      AutofillSuggestionTriggerSource trigger_source,
      const std::vector<Suggestion>& suggestions) override;

  // Retrieves the four digit combinations from the DOM of the current web page
  // and stores them in `four_digit_combinations_in_dom_`. This is used to check
  // for the virtual card last four when checking for standalone CVC field.
  void FetchPotentialCardLastFourDigitsCombinationFromDOM();

  // Returns true if either Profile or CreditCard Autofill is enabled.
  virtual bool IsAutofillEnabled() const;

  // Returns true if the value of the AutofillProfileEnabled pref is true and
  // the client supports Autofill.
  virtual bool IsAutofillProfileEnabled() const;

  // Returns true if the value of the AutofillCreditCardEnabled pref is true and
  // the client supports Autofill.
  virtual bool IsAutofillCreditCardEnabled() const;

  // Shared code to determine if |form| should be uploaded to the Autofill
  // server. It verifies that uploading is allowed and |form| meets conditions
  // to be uploadable. Exposed for testing.
  bool ShouldUploadForm(const FormStructure& form);

  // Returns the last form the autofill manager considered in this frame.
  virtual const FormData& last_query_form() const;

  // Exposed to ContentAutofillDriver to help with recording WebOTP metrics.
  bool has_parsed_forms() const { return has_parsed_forms_; }
  bool has_observed_phone_number_field() const {
    return has_observed_phone_number_field_;
  }
  bool has_observed_one_time_code_field() const {
    return has_observed_one_time_code_field_;
  }

  // Reports whether a document collects phone numbers, uses one time code, uses
  // WebOTP. There are cases that the reporting is not expected:
  //   1. some unit tests do not set necessary members,
  //   |browser_autofill_manager_|
  //   2. there is no form and WebOTP is not used
  void ReportAutofillWebOTPMetrics(bool used_web_otp) override;

  // Handles the logic for when the user selects to see promo code offer
  // details. It opens a new tab and navigates to the offer details page, and
  // then logs that the promo code suggestions footer was selected.
  void OnSeePromoCodeOfferDetailsSelected(const GURL& offer_details_url,
                                          const std::u16string& value,
                                          PopupItemId popup_item_id,
                                          const FormData& form,
                                          const FormFieldData& field);

  // Set Fast Checkout run ID on the corresponding form event logger.
  virtual void SetFastCheckoutRunId(FieldTypeGroup field_type_group,
                                    int64_t run_id);

  TouchToFillDelegate* touch_to_fill_delegate() {
    return touch_to_fill_delegate_.get();
  }

  void set_touch_to_fill_delegate(
      std::unique_ptr<TouchToFillDelegate> touch_to_fill_delegate) {
    touch_to_fill_delegate_ = std::move(touch_to_fill_delegate);
  }

  FastCheckoutDelegate* fast_checkout_delegate() {
    return fast_checkout_delegate_.get();
  }

  void set_fast_checkout_delegate(
      std::unique_ptr<FastCheckoutDelegate> fast_checkout_delegate) {
    fast_checkout_delegate_ = std::move(fast_checkout_delegate);
  }

  void set_test_addresses(
      std::vector<autofill::AutofillProfile> test_addresses) {
    test_addresses_ = test_addresses;
  }

  // Returns the field corresponding to |form| and |field| that can be
  // autofilled. Returns NULL if the field cannot be autofilled.
  [[nodiscard]] AutofillField* GetAutofillField(const FormData& form,
                                                const FormFieldData& field);

  autofill_metrics::AutocompleteUnrecognizedFallbackEventLogger&
  GetAutocompleteUnrecognizedFallbackEventLogger() {
    return *autocomplete_unrecognized_fallback_logger_;
  }

 protected:
  // Stores a `callback` for `form_signature`, possibly overriding an older
  // callback for `form_signature` or triggering a pending callback in case too
  // many callbacks are stored to create space.
  virtual void StoreUploadVotesAndLogQualityCallback(
      FormSignature form_signature,
      base::OnceClosure callback);

  // Triggers and wipes all pending QualityAndVotesUploadCallbacks.
  void FlushPendingLogQualityAndVotesUploadCallbacks();

  // Removes a callback for the given `form_signature` without calling it.
  void WipeLogQualityAndVotesUploadCallback(FormSignature form_signature);

  // Logs quality metrics for the |submitted_form| and uploads votes for the
  // field types to the crowdsourcing server, if appropriate.
  // |observed_submission| indicates whether the upload is a result of an
  // observed submission event.
  virtual void UploadVotesAndLogQuality(
      std::unique_ptr<FormStructure> submitted_form,
      base::TimeTicks interaction_time,
      base::TimeTicks submission_time,
      bool observed_submission);

  // Returns the card image for `credit_card`. If the `credit_card` has a card
  // art image linked, prefer it. Otherwise fall back to the network icon.
  virtual const gfx::Image& GetCardImage(const CreditCard& credit_card);

  // AutofillManager:
  void OnFormSubmittedImpl(const FormData& form,
                           bool known_success,
                           mojom::SubmissionSource source) override;
  void OnTextFieldDidChangeImpl(const FormData& form,
                                const FormFieldData& field,
                                const gfx::RectF& bounding_box,
                                const base::TimeTicks timestamp) override;
  void OnTextFieldDidScrollImpl(const FormData& form,
                                const FormFieldData& field,
                                const gfx::RectF& bounding_box) override {}
  void OnAskForValuesToFillImpl(
      const FormData& form,
      const FormFieldData& field,
      const gfx::RectF& transformed_box,
      AutofillSuggestionTriggerSource trigger_source) override;
  void OnSelectControlDidChangeImpl(const FormData& form,
                                    const FormFieldData& field,
                                    const gfx::RectF& bounding_box) override;
  bool ShouldParseForms() override;
  void OnBeforeProcessParsedForms() override;
  void OnFormProcessed(const FormData& form,
                       const FormStructure& form_structure) override;
  void OnAfterProcessParsedForms(const DenseSet<FormType>& form_types) override;

 private:
  friend class BrowserAutofillManagerTestApi;

  // Keeps track of the filling context for a form, used to make refill
  // attempts.
  struct FillingContext {
    // |profile_or_credit_card| contains either AutofillProfile or CreditCard
    // and must be non-null.
    // If |profile_or_credit_card| contains a CreditCard, |optional_cvc| may be
    // non-null.
    FillingContext(const AutofillField& field,
                   absl::variant<const AutofillProfile*, const CreditCard*>
                       profile_or_credit_card,
                   const std::u16string* optional_cvc);
    ~FillingContext();

    // Whether a refill attempt was made.
    bool attempted_refill = false;
    // The profile or credit card that was used for the initial fill.
    // The std::string associated with the credit card is the CVC, which may be
    // empty.
    absl::variant<AutofillProfile, std::pair<CreditCard, std::u16string>>
        profile_or_credit_card_with_cvc;
    // Possible identifiers of the field that was focused when the form was
    // initially filled. A refill shall be triggered from the same field.
    const FieldGlobalId filled_field_id;
    const FieldSignature filled_field_signature;
    // The security origin from which the field was filled.
    url::Origin filled_origin;
    // The time at which the initial fill occurred.
    const base::TimeTicks original_fill_time;
    // The timer used to trigger a refill.
    base::OneShotTimer on_refill_timer;
    // The field type groups that were initially filled.
    std::set<FieldTypeGroup> type_groups_originally_filled;
    // If populated, this map determines which values will be filled into a
    // field (it does not matter whether the field already contains a value).
    std::map<FieldGlobalId, std::u16string> forced_fill_values;
  };

  // Given a `form` (and corresponding `form_structure`) to fill, return a list
  // of skip statuses for the fields.
  // `optional_credit_card` is the credit card to be filled or nullopt if we're
  // filling an AutofillProfile.
  // `type_group_originally_filled` denotes, in case of a refill, what groups
  // where filled in the initial filling.
  // It is assumed here that `form` and `form_structure` have the same
  // number of fields, and this would be the size of the returned list.
  // TODO(crbug/1331312): Keep only one of 'form' and 'form_structure'.
  // TODO(crbug/1275649): Add the case removed in crrev.com/c/4675831 when the
  // experiment resumes.
  std::vector<SkipStatus> GetSkipStatuses(
      const FormData& form,
      const FormStructure& form_structure,
      const FormFieldData& trigger_field,
      const Section& filling_section,
      const absl::optional<CreditCard>& optional_credit_card,
      const std::set<FieldTypeGroup>& type_groups_originally_filled,
      bool skip_unrecognized_autocomplete_fields,
      bool is_refill) const;

  // CreditCardAccessManager::Accessor
  void OnCreditCardFetched(CreditCardFetchResult result,
                           const CreditCard* credit_card,
                           const std::u16string& cvc) override;

  // Returns false if Autofill is disabled or if no Autofill data is available.
  bool RefreshDataModels();

  // TODO(crbug.com/1249665): Move the functions to AutofillSuggestionGenerator.
  // Gets the card referred to by the guid |unique_id|. Returns |nullptr| if
  // card does not exist.
  CreditCard* GetCreditCard(Suggestion::BackendId unique_id);

  // Gets the profile referred to by the guid |unique_id|. Returns |nullptr| if
  // profile does not exist.
  AutofillProfile* GetProfile(Suggestion::BackendId unique_id);

  // Determines whether a fill on |form| initiated from |triggered_field| will
  // wind up filling a credit card number. This is useful to determine if we
  // will need to unmask a card.
  bool WillFillCreditCardNumber(const FormData& form,
                                const FormFieldData& triggered_field);

  // Fills or previews the profile form.
  // Assumes the form and field are valid.
  // TODO(crbug.com/1330108): Clean up the API.
  void FillOrPreviewProfileForm(
      mojom::AutofillActionPersistence action_persistence,
      const FormData& form,
      const FormFieldData& field,
      const AutofillProfile& profile,
      const AutofillTriggerSource trigger_source);

  // Fills or previews |data_model| in the |form|.
  // TODO(crbug.com/1330108): Clean up the API.
  void FillOrPreviewDataModelForm(
      mojom::AutofillActionPersistence action_persistence,
      const FormData& form,
      const FormFieldData& field,
      absl::variant<const AutofillProfile*, const CreditCard*>
          profile_or_credit_card,
      const std::u16string* optional_cvc,
      FormStructure* form_structure,
      AutofillField* autofill_field,
      const AutofillTriggerSource trigger_source,
      bool is_refill = false);

  // Returns true if the field value should not be overridden by Autofill.
  // Selection fields are excluded from this check because they may have a
  // non-empty value. If the initiating element had a prefilled value but the
  // autofill suggestion is present that includes the currently filled value in
  // the field as a substring, Autofill would override the filled value in that
  // case.
  [[nodiscard]] bool ShouldPreventAutofillFromOverridingPrefilledField(
      mojom::AutofillActionPersistence action_persistence,
      AutofillField* cached_field,
      FormFieldData* field_data,
      bool is_initiating_field,
      absl::variant<const AutofillProfile*, const CreditCard*>
          profile_or_credit_card,
      const std::u16string* optional_cvc);

  // Creates a FormStructure using the FormData received from the renderer. Will
  // return an empty scoped_ptr if the data should not be processed for upload
  // or personal data.
  std::unique_ptr<FormStructure> ValidateSubmittedForm(const FormData& form);

  // Returns true if any field in the form corresponds to an address
  // |FieldTypeGroup|.
  // TODO(crbug.com/1411352): Consider moving to form_types.h.
  [[nodiscard]] bool FormHasAddressField(const FormData& form);

  // Returns suggestions for the `form`, if suggestions were triggered using
  // the `trigger_source` on the `field`. The field's type is `field_type`.
  // The `trigger_source` controls which fields are considered for filling and
  // thus influences the suggestion labels.
  std::vector<Suggestion> GetProfileSuggestions(
      const FormData& form,
      const FormStructure& form_structure,
      const FormFieldData& field,
      const AutofillField& autofill_field,
      AutofillSuggestionTriggerSource trigger_source) const;

  // Returns a list of values from the stored credit cards that match |type| and
  // the value of |field| and returns the labels of the matching credit cards.
  // |should_display_gpay_logo| will be set to true if there is no credit card
  // suggestions or all suggestions come from Payments server.
  std::vector<Suggestion> GetCreditCardSuggestions(
      const FormFieldData& field,
      const AutofillType& type,
      bool& should_display_gpay_logo) const;

  // Returns a mapping of credit card guid values to virtual card last fours for
  // standalone CVC field. Cards will only be added to the returned map if they
  // have usage data on the webpage and the VCN last four was found on webpage
  // DOM.
  base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>
  GetVirtualCreditCardsForStandaloneCvcField(const url::Origin& origin) const;

  // If |initial_interaction_timestamp_| is unset or is set to a later time than
  // |interaction_timestamp|, updates the cached timestamp.  The latter check is
  // needed because IPC messages can arrive out of order.
  void UpdateInitialInteractionTimestamp(
      const base::TimeTicks& interaction_timestamp);

  // Examines |form| and returns true if it is in a non-secure context or
  // its action attribute targets a HTTP url.
  bool IsFormNonSecure(const FormData& form) const;

  // Uses the existing personal data in |profiles| and |credit_cards| to
  // determine possible field types for the |form|.  This is
  // potentially expensive -- on the order of 50ms even for a small set of
  // |stored_data|. Hence, it should not run on the UI thread -- to avoid
  // locking up the UI -- nor on the IO thread -- to avoid blocking IPC calls.
  static void DeterminePossibleFieldTypesForUpload(
      const std::vector<AutofillProfile>& profiles,
      const std::vector<CreditCard>& credit_cards,
      const std::u16string& last_unlocked_credit_card_cvc,
      const std::string& app_locale,
      bool observed_submission,
      FormStructure* form);

  // Uses context about previous and next fields to select the appropriate type
  // for fields with ambiguous upload types.
  static void DisambiguateUploadTypes(FormStructure* form);

  // Disambiguates name field upload types.
  static void DisambiguateNameUploadTypes(
      FormStructure* form,
      size_t current_index,
      const ServerFieldTypeSet& upload_types);

  // Calls FieldFiller::FillFormField().
  //
  // If the field was newly filled, sets `autofill_field->is_autofilled` and
  // `field_data->is_autofilled` both to true (otherwise leaves them unchanged).
  //
  // Also logs metrics and, if `should_notify` is true, calls
  // AutofillClient::DidFillOrPreviewField().
  //
  // Returns true if the field has been filled, false otherwise. This is
  // independent of whether the field was filled or autofilled before.
  bool FillFieldWithValue(
      AutofillField* autofill_field,
      absl::variant<const AutofillProfile*, const CreditCard*>
          profile_or_credit_card,
      const std::map<FieldGlobalId, std::u16string>& forced_fill_values,
      FormFieldData* field_data,
      bool should_notify,
      const std::u16string& cvc,
      uint32_t profile_form_bitmask,
      mojom::AutofillActionPersistence action_persistence,
      std::string* failure_to_fill);

  void SetFillingContext(const FormStructure& form,
                         std::unique_ptr<FillingContext> context);

  FillingContext* GetFillingContext(const FormStructure& form);

  // Whether there should be an attempts to refill the form. Returns true if all
  // the following are satisfied:
  //  There have been no refill on that page yet.
  //  A non empty form name was recorded in a previous fill
  //  That form name matched the currently parsed form name
  //  It's been less than kLimitBeforeRefillMs since the original fill.
  bool ShouldTriggerRefill(const FormStructure& form_structure);

  // Schedules a call of TriggerRefill. Virtual for testing.
  virtual void ScheduleRefill(const FormData& form,
                              const AutofillTriggerSource trigger_source);

  // Attempts to refill the form that was changed dynamically. Should only be
  // called if ShouldTriggerRefill returns true.
  void TriggerRefill(const FormData& form,
                     const AutofillTriggerSource trigger_source);

  // This function is called by JavaScriptChangedAutofilledValue and may trigger
  // a refill in case the website used JavaScript to reformat an expiration date
  // like "05/2023" into "05 / 20" (i.e. it broke the year by cutting the last
  // two digits instead of stripping the first two digits).
  void MaybeTriggerRefillForExpirationDate(
      const FormData& form,
      const FormFieldData& field,
      const std::u16string& old_value,
      const AutofillTriggerSource trigger_source);

  // Checks whether JavaScript cleared an autofilled value within
  // kLimitBeforeRefill after the filling and records metrics for this. This
  // method should be called after we learned that JavaScript modified an
  // autofilled field. It's responsible for assessing the nature of the
  // modification.
  void AnalyzeJavaScriptChangedAutofilledValue(const FormData& form,
                                               const FormFieldData& field);

  // Replaces the contents of `suggestions` with available suggestions for
  // `field`. Which fields of the `form` are filled depends on the
  // `trigger_source`.
  // `context` will contain additional information about the suggestions, such
  // as if they correspond to credit card suggestions and if the context is
  // secure.
  void GetAvailableSuggestions(const FormData& form,
                               const FormFieldData& field,
                               AutofillSuggestionTriggerSource trigger_source,
                               std::vector<Suggestion>* suggestions,
                               SuggestionsContext* context);

  // For each submitted field in the |form_structure|, it determines whether
  // |ADDRESS_HOME_STATE| is a possible matching type.
  // This method is intended to run matching type detection on the browser UI
  // thread.
  void PreProcessStateMatchingTypes(
      const std::vector<AutofillProfile>& profiles,
      FormStructure* form_structure);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // Whether to show the option to use virtual card in the autofill popup.
  bool ShouldShowVirtualCardOption(FormStructure* form_structure);
#endif

  // Returns an appropriate EventFormLogger, depending on the given `field`'s
  // type. May return nullptr.
  autofill_metrics::FormEventLoggerBase* GetEventFormLogger(
      const AutofillField& field) const;

  void SetDataList(const std::vector<std::u16string>& values,
                   const std::vector<std::u16string>& labels);

  // Iterate through all the fields in the form to process the log events for
  // each field and record into FieldInfo UKM event.
  void ProcessFieldLogEventsInForm(const FormStructure& form_structure);

  // Log the number of log events of all types which have been recorded until
  // the FieldInfo metric is recorded into UKM at form submission or form
  // destruction time (whatever comes first).
  void LogEventCountsUMAMetric(const FormStructure& form_structure);

  // Returns whether the form is considered parseable and meets a couple of
  // other requirements which makes uploading UKM data worthwhile. E.g. the
  // form should not be a search form, the forms should have at least one
  // focusable input field with a type from heuristics or the server.
  bool ShouldUploadUkm(const FormStructure& form_structure);

  // Delegates to perform external processing (display, selection) on
  // our behalf.
  std::unique_ptr<AutofillExternalDelegate> external_delegate_;
  std::unique_ptr<TouchToFillDelegate> touch_to_fill_delegate_;
  std::unique_ptr<FastCheckoutDelegate> fast_checkout_delegate_;

  std::string app_locale_;

  // Used to help fill data into fields.
  FieldFiller field_filler_;

  // Container holding the history of Autofill filling operations. Used to undo
  // some of the filling operations.
  FormAutofillHistory form_autofill_history_;

  base::circular_deque<std::string> autofilled_form_signatures_;

  // Handles routing single-field form filling requests, such as for
  // Autocomplete and merchant promo codes.
  std::unique_ptr<SingleFieldFormFillRouter> single_field_form_fill_router_;

  // Utilities for logging form events. The loggers emit metrics during their
  // destruction, effectively when the BrowserAutofillManager is reset or
  // destroyed.
  // The address and credit card event loggers are used to emit key and funnel
  // metrics.
  std::unique_ptr<autofill_metrics::AddressFormEventLogger>
      address_form_event_logger_;
  std::unique_ptr<autofill_metrics::CreditCardFormEventLogger>
      credit_card_form_event_logger_;
  // The autocomplete unrecognized fallback logger is used to collect metrics
  // around the manual fallback for autocomplete=unrecognized fields.
  // Since no metrics for autocomplete=unrecognized fields are emitted through
  // the `address_form_event_logger_`, a separate logger specifically for
  // autocomplete=unrecognized fields is used.
  std::unique_ptr<autofill_metrics::AutocompleteUnrecognizedFallbackEventLogger>
      autocomplete_unrecognized_fallback_logger_;

  // Have we logged whether Autofill is enabled for this page load?
  bool has_logged_autofill_enabled_ = false;
  // Have we logged an address suggestions count metric for this page?
  bool has_logged_address_suggestions_count_ = false;
  // Have we shown Autofill suggestions at least once?
  bool did_show_suggestions_ = false;
  // Has the user manually edited at least one form field among the autofillable
  // ones?
  bool user_did_type_ = false;
  // Has the user autofilled a form on this page?
  bool user_did_autofill_ = false;
  // Has the user edited a field that was previously autofilled?
  bool user_did_edit_autofilled_field_ = false;

  // Does |this| have any parsed forms?
  bool has_parsed_forms_ = false;
  // Is there a field with autocomplete="one-time-code" observed?
  bool has_observed_one_time_code_field_ = false;
  // Is there a field with phone number collection observed?
  bool has_observed_phone_number_field_ = false;

  // When the user first interacted with a potentially fillable form on this
  // page.
  base::TimeTicks initial_interaction_timestamp_;

  // A copy of the currently interacted form data.
  std::unique_ptr<FormData> pending_form_data_;

  // The credit card access manager, used to access local and server cards.
  std::unique_ptr<CreditCardAccessManager> credit_card_access_manager_;

  // Helper class to generate Autofill suggestions.
  std::unique_ptr<AutofillSuggestionGenerator> suggestion_generator_;

  // Test addresses used to allow developers to test their forms.
  std::vector<autofill::AutofillProfile> test_addresses_;

  // Collected information about the autofill form where a credit card will be
  // filled.
  FormData credit_card_form_;
  FormFieldData credit_card_field_;
  CreditCard credit_card_;
  std::u16string last_unlocked_credit_card_cvc_;

  // Delegate used in test to get notifications on certain events.
  raw_ptr<BrowserAutofillManagerTestDelegate> test_delegate_ = nullptr;

  // A map from FormGlobalId to FillingContext instances used to make refill
  // attempts for dynamic forms.
  std::map<FormGlobalId, std::unique_ptr<FillingContext>> filling_context_;

  // Used to record metrics. This should be set at the beginning of the
  // interaction and re-used throughout the context of this manager.
  AutofillSyncSigninState sync_state_ = AutofillSyncSigninState::kNumSyncStates;

  // Helps with measuring whether phone number is collected and whether it is in
  // conjunction with WebOTP or OneTimeCode (OTC).
  // value="0" label="Phone Not Collected, WebOTP Not Used, OTC Not Used"
  // value="1" label="Phone Not Collected, WebOTP Not Used, OTC Used"
  // value="2" label="Phone Not Collected, WebOTP Used, OTC Not Used"
  // value="3" label="Phone Not Collected, WebOTP Used, OTC Used"
  // value="4" label="Phone Collected, WebOTP Not Used, OTC Not Used"
  // value="5" label="Phone Collected, WebOTP Not Used, OTC Used"
  // value="6" label="Phone Collected, WebOTP Used, OTC Not Used"
  // value="7" label="Phone Collected, WebOTP Used, OTC Used"
  uint32_t phone_collection_metric_state_ = 0;

  // List of callbacks to be called for sending blur votes. Only one callback is
  // stored per FormSignature. We rely on FormSignatures rather than
  // FormGlobalId to send votes for the various signatures of a form while it
  // evolves (when fields are added or removed). The list of callbacks is
  // ordered by time of creation: newest elements first. If the list becomes too
  // long, the oldest pending callbacks are just called and popped removed the
  // list.
  //
  // Callbacks are triggered in the following situations:
  // - We observe a form submission.
  // - The list becomes to large.

  // Callbacks are wiped in the following situations:
  // - A form is submitted.
  // - A callback is overridden by a more recent version.
  std::list<std::pair<FormSignature, base::OnceClosure>> queued_vote_uploads_;

  // This task runner sequentializes calls to
  // DeterminePossibleFieldTypesForUpload to ensure that blur votes are
  // processed before form submission votes. This is important so that a
  // submission can trigger the upload of blur votes.
  scoped_refptr<base::SequencedTaskRunner> vote_upload_task_runner_;

  // When the form was submitted.
  base::TimeTicks form_submitted_timestamp_;

  // The source that triggered unlocking a server card with the CVC.
  absl::optional<AutofillTriggerSource> fetched_credit_card_trigger_source_;

  // Contains a list of four digit combinations that were found in the webpage
  // DOM. Populated after a standalone cvc field is processed on a form. Used to
  // confirm that the virtual card last four is present in the webpage for card
  // on file case.
  std::vector<std::string> four_digit_combinations_in_dom_;

  base::WeakPtrFactory<BrowserAutofillManager> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_BROWSER_AUTOFILL_MANAGER_H_
