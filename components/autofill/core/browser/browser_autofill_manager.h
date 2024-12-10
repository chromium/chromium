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
#include "base/containers/flat_set.h"
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
#include "components/autofill/core/browser/autofill_ai_delegate.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_plus_address_delegate.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/crowdsourcing/votes_uploader.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/form_autofill_history.h"
#include "components/autofill/core/browser/filling/form_filler.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/form_events/address_form_event_logger.h"
#include "components/autofill/core/browser/metrics/form_events/credit_card_form_event_logger.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/password_form_classification.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/payments/card_unmask_delegate.h"
#include "components/autofill/core/browser/payments/full_card_request.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/single_field_fill_router.h"
#include "components/autofill/core/browser/suggestions/suggestions_context.h"
#include "components/autofill/core/browser/ui/fast_checkout_delegate.h"
#include "components/autofill/core/browser/ui/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/browser/ui/touch_to_fill_delegate.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace autofill {

class AutofillField;
class AutofillProfile;
class CreditCard;
class CreditCardAccessManager;

class FormData;
class FormFieldData;
struct SuggestionsContext;

namespace autofill_metrics {

class CreditCardFormEventLogger;
struct SuggestionRankingContext;

}  // namespace autofill_metrics

// Enum for the value patterns metric. Don't renumerate existing value. They are
// used for metrics.
enum class ValuePatternsMetric {
  kNoPatternFound = 0,
  kUpiVpa = 1,  // UPI virtual payment address.
  kIban = 2,    // International Bank Account Number.
  kMaxValue = kIban,
};

class BrowserAutofillManager;

// Manages saving and restoring the user's personal information entered into web
// forms. One per frame; owned by the AutofillDriver.
class BrowserAutofillManager : public AutofillManager {
 public:
  // Triggered when `GenerateSuggestionsAndMaybeShowUIPhase2` is complete.
  // `show_suggestions` indicates whether or not the list of `suggestions`
  // should be displayed (via the `external_delegate_`). `ranking_context`
  // contains information regarding the ranking of suggestions and is used for
  // metrics logging.
  using OnGenerateSuggestionsCallback = base::OnceCallback<void(
      bool show_suggestions,
      std::vector<Suggestion> suggestions,
      std::optional<autofill_metrics::SuggestionRankingContext>
          ranking_context)>;

  explicit BrowserAutofillManager(AutofillDriver* driver);

  BrowserAutofillManager(const BrowserAutofillManager&) = delete;
  BrowserAutofillManager& operator=(const BrowserAutofillManager&) = delete;

  ~BrowserAutofillManager() override;

  // Whether the |field| should show an entry to scan a credit card.
  virtual bool ShouldShowScanCreditCard(const FormData& form,
                                        const FormFieldData& field);

  // Handlers for the "Show Cards From Account" row. This row should be shown to
  // users who have cards in their account and can use Sync Transport. Clicking
  // the row records the user's consent to see these cards on this device, and
  // refreshes the popup.
  virtual bool ShouldShowCardsFromAccountOption(
      const FormData& form,
      const FormFieldData& field,
      AutofillSuggestionTriggerSource trigger_source) const;
  virtual void OnUserAcceptedCardsFromAccountOption();
  virtual void RefetchCardsAndUpdatePopup(const FormData& form,
                                          const FormFieldData& field);

  // Fills or previews `form` with the information in `credit_card`. `field_id`
  // is the ID of the field that triggered the filling operation.
  // `trigger_source` is the reason for triggering the filling operation.
  // `action_persistence` denotes whether the operation is a filling or preview
  // operation.
  virtual void FillOrPreviewCreditCardForm(
      mojom::ActionPersistence action_persistence,
      const FormData& form,
      const FieldGlobalId& field_id,
      const CreditCard& credit_card,
      AutofillTriggerSource trigger_source);

  // Routes calls from external components to FormFiller::FillOrPreviewField.
  // Virtual for testing.
  // TODO(crbug.com/40227496): Replace FormFieldData parameter by FieldGlobalId.
  virtual void FillOrPreviewField(mojom::ActionPersistence action_persistence,
                                  mojom::FieldActionType action_type,
                                  const FormData& form,
                                  const FormFieldData& field,
                                  const std::u16string& value,
                                  SuggestionType type,
                                  std::optional<FieldType> field_type_used);

  // Logs metrics when the user accepts address form filling suggestion. This
  // happens only for already parsed forms (`FormStructure` and `AutofillField`
  // are defined).
  // TODO(crbug.com/40227071): Remove when field-filling and form-filling are
  // merged
  virtual void OnDidFillAddressFormFillingSuggestion(
      const AutofillProfile& profile,
      const FormGlobalId& form_id,
      const FieldGlobalId& field_id,
      AutofillTriggerSource trigger_source);

  // Logs metrics when the user accepts a
  // `SuggestionType::kAddressEntryOnTyping` suggestion on the field represented
  // by `field_id`.
  virtual void OnDidFillAddressOnTypingSuggestion(
      const FieldGlobalId& field_id);

  // Calls UndoAutofillImpl and logs metrics. Virtual for testing.
  virtual void UndoAutofill(mojom::ActionPersistence action_persistence,
                            const FormData& form,
                            const FormFieldData& trigger_field);
  // Virtual for testing
  virtual void DidShowSuggestions(
      DenseSet<SuggestionType> shown_suggestion_types,
      const FormData& form,
      const FieldGlobalId& field_id);

  // Fills or previews the profile form.
  // Assumes the form and field are valid.
  // TODO(crbug.com/40227071): Clean up the API.
  virtual void FillOrPreviewProfileForm(
      mojom::ActionPersistence action_persistence,
      const FormData& form,
      const FieldGlobalId& field_id,
      const AutofillProfile& profile,
      AutofillTriggerSource trigger_source);

  /////////////////
  // DO NOT USE! //
  /////////////////
  // See `FormFiller::FillOrPreviewFormWithAutofillAiData()`.
  // TODO(crbug.com/40227071): Clean up the API and remove this function.
  void FillOrPreviewFormWithAutofillAiData(
      mojom::ActionPersistence action_persistence,
      const DenseSet<FieldFillingSkipReason>& ignorable_skip_reasons,
      const FormData& form,
      const FormFieldData& trigger_field,
      const base::flat_map<FieldGlobalId, std::u16string>& values_to_fill);

  // Invoked when the user selected the `suggestion` in a suggestions list from
  // single field filling.
  void OnSingleFieldSuggestionSelected(const Suggestion& suggestion,
                                       const FormData& form,
                                       const FormFieldData& field);

  // Update the pending form with |form|, possibly processing the current
  // pending form for upload.
  void UpdatePendingForm(const FormData& form);

  // Upload the current pending form.
  void ProcessPendingFormForUpload();

  CreditCardAccessManager& GetCreditCardAccessManager();
  const CreditCardAccessManager& GetCreditCardAccessManager() const;

  // Handles post-filling logic of `form_structure`, like notifying observers
  // and logging form metrics.
  // `filled_fields` are the fields that were filled by the browser.
  // `safe_fields` are the fields that were deemed safe to fill by the router
  // according to the iframe security policy.
  // `safe_filled_fields` is the intersection of `filled_fields` and
  // `safe_fields`. `skip_reasons` tells us for each field (mapped by their
  // IDs), whether the field was skipped for filling or not and why.
  void OnDidFillOrPreviewForm(
      mojom::ActionPersistence action_persistence,
      const FormData& form,
      FormStructure& form_structure,
      AutofillField& trigger_autofill_field,
      base::span<const FormFieldData*> safe_filled_fields,
      base::span<const AutofillField*> safe_filled_autofill_fields,
      const base::flat_set<FieldGlobalId>& filled_field_ids,
      const base::flat_set<FieldGlobalId>& safe_field_ids,
      const base::flat_map<FieldGlobalId, DenseSet<FieldFillingSkipReason>>&
          skip_reasons,
      const FillingPayload& filling_payload,
      AutofillTriggerSource trigger_source,
      bool is_refill);

  // AutofillManager:
  base::WeakPtr<AutofillManager> GetWeakPtr() override;
  bool ShouldClearPreviewedForm() override;
  void OnFocusOnNonFormFieldImpl() override;
  void OnFocusOnFormFieldImpl(const FormData& form,
                              const FieldGlobalId& field_id) override;
  void OnDidFillAutofillFormDataImpl(const FormData& form,
                                     const base::TimeTicks timestamp) override;
  void OnDidEndTextFieldEditingImpl() override;
  void OnHidePopupImpl() override;
  void OnSelectFieldOptionsDidChangeImpl(const FormData& form) override;
  void OnJavaScriptChangedAutofilledValueImpl(const FormData& form,
                                              const FieldGlobalId& field_id,
                                              const std::u16string& old_value,
                                              bool formatting_only) override;
  void Reset() override;

  // Retrieves the four digit combinations from the DOM of the current web page
  // and stores them in `four_digit_combinations_in_dom_`. This is used to check
  // for the virtual card last four when checking for standalone CVC field.
  void FetchPotentialCardLastFourDigitsCombinationFromDOM();

  // Shared code to determine if |form| should be uploaded to the Autofill
  // server. It verifies that uploading is allowed and |form| meets conditions
  // to be uploadable. Exposed for testing.
  bool ShouldUploadForm(const FormStructure& form);

  // Returns the last form the autofill manager considered in this frame.
  virtual const FormData& last_query_form() const;

  // Reports whether a document collects phone numbers, uses one time code, uses
  // WebOTP. There are cases that the reporting is not expected:
  //   1. some unit tests do not set necessary members,
  //   |browser_autofill_manager_|
  //   2. there is no form and WebOTP is not used
  void ReportAutofillWebOTPMetrics(bool used_web_otp) override;

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

  // Returns the field corresponding to |form| and |field| that can be
  // autofilled. Returns NULL if the field cannot be autofilled.
  [[nodiscard]] AutofillField* GetAutofillField(
      const FormData& form,
      const FormFieldData& field) const;

  // Notifies the `BrowserAutofillManager` that `credit_card` has been fetched
  // from the server. Opens a manual filling dialog for virtual credit cards.
  // Caches the credit card data for server and virtual credit cards.
  void OnCreditCardFetchedSuccessfully(const CreditCard& credit_card);

  autofill_metrics::CreditCardFormEventLogger& GetCreditCardFormEventLogger() {
    return metrics_->credit_card_form_event_logger;
  }

 protected:
  // Returns the card image for `credit_card`. If the `credit_card` has a card
  // art image linked, prefer it. Otherwise fall back to the network icon.
  virtual const gfx::Image& GetCardImage(const CreditCard& credit_card);

  // AutofillManager:
  void OnFormSubmittedImpl(const FormData& form,
                           mojom::SubmissionSource source) override;
  void OnCaretMovedInFormFieldImpl(const FormData& form,
                                   const FieldGlobalId& field_id,
                                   const gfx::Rect& caret_bounds) override {}
  void OnTextFieldDidChangeImpl(const FormData& form,
                                const FieldGlobalId& field_id,
                                const base::TimeTicks timestamp) override;
  void OnTextFieldDidScrollImpl(const FormData& form,
                                const FieldGlobalId& field_id) override {}
  void OnAskForValuesToFillImpl(
      const FormData& form,
      const FieldGlobalId& field_id,
      const gfx::Rect& caret_bounds,
      AutofillSuggestionTriggerSource trigger_source) override;
  void OnSelectControlDidChangeImpl(const FormData& form,
                                    const FieldGlobalId& field_id) override;
  bool ShouldParseForms() override;
  void OnBeforeProcessParsedForms() override;
  void OnFormProcessed(const FormData& form,
                       const FormStructure& form_structure) override;

 private:
  friend class BrowserAutofillManagerTestApi;

  // Utilities for logging form events. The loggers emit metrics during their
  // destruction, effectively when the BrowserAutofillManager is reset or
  // destroyed.
  struct MetricsState {
    explicit MetricsState(BrowserAutofillManager* owner);
    ~MetricsState();

    // The address and credit card event loggers are used to emit key and funnel
    // metrics.
    autofill_metrics::AddressFormEventLogger address_form_event_logger;
    autofill_metrics::CreditCardFormEventLogger credit_card_form_event_logger;

    // Have we logged whether Autofill is enabled for this page load?
    bool has_logged_autofill_enabled = false;
    // Has the user manually edited at least one form field among the
    // autofillable ones?
    bool user_did_type = false;

    // TODO(crbug.com/354043809): Move out of BAM.
    // Does |this| have any parsed forms?
    bool has_parsed_forms = false;
    // Is there a field with autocomplete="one-time-code" observed?
    bool has_observed_one_time_code_field = false;
    // Is there a field with phone number collection observed?
    bool has_observed_phone_number_field = false;

    // Should be set at the beginning of the interaction and re-used
    // throughout the context of this manager.
    AutofillMetrics::PaymentsSigninState signin_state_for_metrics =
        AutofillMetrics::PaymentsSigninState::kUnknown;

    // When the user first interacted with a potentially fillable form on this
    // page.
    base::TimeTicks initial_interaction_timestamp;

    // When the form was submitted.
    base::TimeTicks form_submitted_timestamp;
  };

  // Method containing logic to be run in `OnFormSubmittedImpl()` after any
  // import attempts of the submitted form occurred.
  void OnFormSubmittedAfterImport(std::unique_ptr<FormStructure> submitted_form,
                                  mojom::SubmissionSource source,
                                  base::TimeTicks form_submitted_timestamp);

  // Emits all metrics that should be recorded at submission time.
  void LogSubmissionMetrics(const FormStructure* submitted_form,
                            const base::TimeTicks& form_submitted_timestamp);

  // See `BrowserAutofillManager::FillOrPreviewCreditCardForm()` for initial
  // documentation. `require_card_fetching` denotes whether we need to fetch the
  // full card represented by `credit_card`.
  void FillOrPreviewCreditCardFormImpl(
      bool require_card_fetching,
      mojom::ActionPersistence action_persistence,
      const FormData& form,
      const FieldGlobalId& field_id,
      const CreditCard& credit_card,
      AutofillTriggerSource trigger_source);

  // When `AuthenticateThenFillCreditCardForm()` fetches a credit card, this
  // gets called once the fetching has finished. If successful, the
  // `credit_card` is filled.
  void OnCreditCardFetched(
      const FormData& form,
      const FieldGlobalId& field_id,
      AutofillTriggerSource fetched_credit_card_trigger_source,
      const CreditCard& credit_card);

  // Updates event loggers with information about data stored for Autofill.
  void UpdateLoggersReadinessData();

  // Creates a FormStructure using the FormData received from the renderer. Will
  // return an empty scoped_ptr if the data should not be processed for upload
  // or personal data.
  // Note that the function returns nullptr in incognito mode. Consequently, in
  // incognito mode Autofill doesn't:
  // - Import
  // - Vote
  // - Collect any key metrics
  // - Collect profile token quality observations
  std::unique_ptr<FormStructure> ValidateSubmittedForm(const FormData& form);

  // Returns suggestions for the `form`, if suggestions were triggered using
  // the `trigger_source` on the `field`. The field's type is `field_type`.
  // The `trigger_source` controls which fields are considered for filling and
  // thus influences the suggestion labels.
  // `form_structure` and `autofill_field` can be null when the `field` from
  // which Autofill was triggered is not an address field. This means the user
  // chose the address manual fallback option to fill an arbitrary non address
  // field.
  std::vector<Suggestion> GetProfileSuggestions(
      const FormData& form,
      const FormStructure& form_structure,
      const FormFieldData& trigger_field,
      const AutofillField& trigger_autofill_field,
      AutofillSuggestionTriggerSource trigger_source,
      std::optional<std::string> plus_address_email_override);

  // Returns a list of values from the stored credit cards that match
  // the type and value of `trigger_field` and returns the labels of the
  // matching credit cards. `ranking_context` contains information regarding the
  // ranking of suggestions and is used for metrics logging.
  // TODO(crbug.com/40227496): Keep only one of `form` or `form_structure` and
  // `trigger_field` or `autofill_trigger_field`.
  std::vector<Suggestion> GetCreditCardSuggestions(
      const FormData& form,
      const FormStructure& form_structure,
      const FormFieldData& trigger_field,
      const AutofillField& autofill_trigger_field,
      AutofillSuggestionTriggerSource trigger_source,
      autofill_metrics::SuggestionRankingContext& ranking_context);

  // If `metrics_->initial_interaction_timestamp` is unset or is set to a later
  // time than `interaction_timestamp`, updates the cached timestamp.  The
  // latter check is needed because IPC messages can arrive out of order.
  void UpdateInitialInteractionTimestamp(base::TimeTicks interaction_timestamp);

  // Examines |form| and returns true if it is in a non-secure context or
  // its action attribute targets a HTTP url.
  bool IsFormNonSecure(const FormData& form) const;

  // Checks whether JavaScript cleared an autofilled value within
  // kLimitBeforeRefill after the filling and records metrics for this. This
  // method should be called after we learned that JavaScript modified an
  // autofilled field. It's responsible for assessing the nature of the
  // modification. `cleared_value` is true if JS wiped the previous value, and
  // `formatting_only` is true if JS only modified whitespaces, symbols and
  // capitalization.
  // TODO(crbug.com/40227496): Remove `cleared_value` when `field` starts
  // containing the actual current value of the field.
  void AnalyzeJavaScriptChangedAutofilledValue(const FormStructure& form,
                                               AutofillField& field,
                                               bool cleared_value,
                                               bool formatting_only);

  // Populates all the fields (except for ablation study related fields) in
  // `SuggestionsContext` based on the given params.
  SuggestionsContext BuildSuggestionsContext(
      const FormData& form,
      const FormStructure* form_structure,
      const FormFieldData& field,
      const AutofillField* autofill_field,
      AutofillSuggestionTriggerSource trigger_source);

  // Evaluates the specifics of the ablation study, updates `context`, and
  // returns whether the study is enabled/disabled.
  bool EvaluateAblationStudy(
      const std::vector<Suggestion>& address_and_credit_card_suggestions,
      AutofillField& autofill_field,
      SuggestionsContext& context);

  // Returns a list with the suggestions available for `field`. Which fields of
  // the `form` are filled depends on the `trigger_source`. `context` could
  // contain additional information about the suggestions, such as ablation
  // study related fields.  `ranking_context` contains information
  // regarding the ranking of suggestions and is used for metrics logging.
  // TODO(crbug.com/340494671): Move ablation study fields out of the function
  // and make the context a const ref.
  std::vector<Suggestion> GetAvailableAddressAndCreditCardSuggestions(
      const FormData& form,
      const FormStructure* form_structure,
      const FormFieldData& field,
      AutofillField* autofill_field,
      AutofillSuggestionTriggerSource trigger_source,
      std::optional<std::string> plus_address_email_override,
      SuggestionsContext& context,
      autofill_metrics::SuggestionRankingContext& ranking_context);

  // Generates and prioritizes different kinds of suggestions and
  // suggestion surfaces accordingly (e.g. Fast Checkout, Autofill AI,
  // SingleFieldFiller(s), address and credit card popups).
  // Suggestion flows that handle their own UI flow (e.g. FastCheckout, TTF,
  // SingleFieldFiller) are triggered from within these functions.
  //
  // This process is split into phrases 1 and 2 to support asynchronous
  // operations in the middle.
  //
  // Phase 2 requires the list of `plus_addresses` as these can influence how
  // address profile suggestions are shown. Other flows that rely on the
  // `external_delegate_` to show their suggestions, pass the suggestions list
  // to the delegate via `OnGenerateSuggestionsComplete` and request them to be
  // shown (via `show_suggestions`). Note that the `callback` is almost always
  // called, regardless of the suggestion surface. The only case when it's not
  // called is when suggestions are suppressed (See
  // `ShouldSuppressSuggestions`).
  void GenerateSuggestionsAndMaybeShowUIPhase1(
      const FormData& form,
      const FormFieldData& field,
      AutofillSuggestionTriggerSource trigger_source,
      SuggestionsContext context,
      OnGenerateSuggestionsCallback callback,
      AutofillAiDelegate::HasData has_autofill_ai_data);
  void GenerateSuggestionsAndMaybeShowUIPhase2(
      const FormData& form,
      const FormFieldData& field,
      AutofillSuggestionTriggerSource trigger_source,
      AutofillAiDelegate::HasData has_autofill_ai_data,
      SuggestionsContext context,
      OnGenerateSuggestionsCallback callback,
      std::vector<std::string> plus_addresses);

  // Receives the lists of plus address and single field form fill suggestions
  // and combines them. It gives priority to the plus address suggestions,
  // ensuring they appear first in the final combined list that's sent to
  // `OnGenerateSuggestionsCallback`.
  void OnGeneratedPlusAddressAndSingleFieldFillSuggestions(
      AutofillPlusAddressDelegate::SuggestionContext suggestions_context,
      PasswordFormClassification::Type password_form_type,
      const FormData& form,
      const FormFieldData& field,
      bool should_offer_single_field_form_fill,
      OnGenerateSuggestionsCallback callback,
      std::vector<std::vector<Suggestion>> suggestion_lists);

  // Triggered when the user undoes the filling of an address profile using an
  // email override.
  void OnEmailOverrideUndone(const std::u16string& original_email,
                             const FormGlobalId& form_id,
                             const FieldGlobalId& field_id,
                             const FormFieldData& field_after_last_autofill);

  // The function receives a the list of `suggestions` from
  // `GenerateSuggestionsAndMaybeShowUIPhase2` and displays them if
  // `show_suggestions` is true (via the `external_delegate_`). It also logs
  // whether there is a suggestion for the user and whether the suggestion is
  // shown. `ranking_context` contains information regarding the ranking of
  // suggestions and is used for metrics logging.
  void OnGenerateSuggestionsComplete(
      const FormData& form,
      const FormFieldData& field,
      AutofillSuggestionTriggerSource trigger_source,
      const SuggestionsContext& context,
      bool show_suggestions,
      std::vector<Suggestion> suggestions,
      std::optional<autofill_metrics::SuggestionRankingContext>
          ranking_context);

  // Combines plus address and address profile suggestions into a single list,
  // prioritizing plus address suggestions first. Runs `callback` with the
  // resulting list of suggestions.
  void MixPlusAddressAndAddressSuggestions(
      std::vector<Suggestion> plus_address_suggestions,
      std::vector<Suggestion> address_suggestions,
      AutofillPlusAddressDelegate::SuggestionContext suggestions_context,
      PasswordFormClassification::Type password_form_type,
      const FormData& form,
      const FormFieldData& field,
      OnGenerateSuggestionsCallback callback);

  // Returns an appropriate EventFormLogger, depending on the given `field`'s
  // type. May return nullptr.
  autofill_metrics::FormEventLoggerBase* GetEventFormLogger(
      const AutofillField& field);

  // Iterate through all the fields in the form to process the log events for
  // each field and record into FieldInfo UKM event.
  void ProcessFieldLogEventsInForm(const FormStructure& form_structure);

  // Log the number of log events of all types which have been recorded until
  // the FieldInfo metric is recorded into UKM at form submission or form
  // destruction time (whatever comes first).
  void LogEventCountsUMAMetric(const FormStructure& form_structure);

  // Returns a compose suggestion if the compose service is available for
  // `field` and `trigger_source`.
  std::optional<Suggestion> MaybeGetComposeSuggestion(
      const FormFieldData& field,
      AutofillSuggestionTriggerSource trigger_source);

  // Appends TriggerFillFieldLogEvent and FillFieldLogEvents to the relevant
  // fields in the form_structure if there was a filling operation.
  void AppendFillLogEvents(
      const FormData& form,
      FormStructure& form_structure,
      AutofillField& trigger_autofill_field,
      const base::flat_set<FieldGlobalId>& safe_field_ids,
      const base::flat_map<FieldGlobalId, DenseSet<FieldFillingSkipReason>>&
          skip_reasons,
      const FillingPayload& filling_payload,
      bool is_refill);

  // Handles the credit card specific logic after a form is filled, including
  // logging the fill operation and recording card usage.
  void LogAndRecordCreditCardFill(
      FormStructure& form_structure,
      AutofillField& trigger_autofill_field,
      base::span<const FormFieldData*> safe_filled_fields,
      base::span<const AutofillField*> safe_filled_autofill_fields,
      const base::flat_set<FieldGlobalId>& filled_field_ids,
      const base::flat_set<FieldGlobalId>& safe_field_ids,
      const CreditCard& card,
      AutofillTriggerSource trigger_source,
      bool is_refill);

  // Handles the address specific logic after a form is filled, including
  // logging the fill operation and recording profile usage.
  void LogAndRecordProfileFill(
      FormStructure& form_structure,
      AutofillField& trigger_autofill_field,
      base::span<const FormFieldData*> safe_filled_fields,
      base::span<const AutofillField*> safe_filled_autofill_fields,
      const AutofillProfile& filled_profile,
      AutofillTriggerSource trigger_source,
      bool is_refill);

  // Checks if the user filled a form using a plus address email override and,
  // if so, shows a notification to the user.
  void MaybeShowPlusAddressEmailOverrideNotification(
      base::span<const AutofillField*> safe_filled_autofill_fields,
      base::span<const FormFieldData*> safe_filled_fields,
      const AutofillProfile& filled_profile,
      const FormStructure& form_structure);

  // Delegates to perform external processing (display, selection) on
  // our behalf.
  std::unique_ptr<AutofillExternalDelegate> external_delegate_ =
      std::make_unique<AutofillExternalDelegate>(this);
  std::unique_ptr<TouchToFillDelegate> touch_to_fill_delegate_;
  std::unique_ptr<FastCheckoutDelegate> fast_checkout_delegate_;

  // This is always non-nullopt except very briefly during Reset().
  std::optional<MetricsState> metrics_ = std::make_optional<MetricsState>(this);

  // If this is true, we consider the form to be secure. (Only use this for
  // testing purposes).
  std::optional<bool> consider_form_as_secure_for_testing_;

  // A copy of the currently interacted form data.
  std::optional<FormData> pending_form_data_;

  // The credit card access manager, used to access local and server cards.
  // Lazily initialized: access only through GetCreditCardAccessManager().
  std::unique_ptr<CreditCardAccessManager> credit_card_access_manager_;

  // Helper class to autofill forms and fields. Do not use directly, use
  // form_filler() instead, because tests inject test objects.
  std::unique_ptr<FormFiller> form_filler_ =
      std::make_unique<FormFiller>(*this);

  // Contains a list of four digit combinations that were found in the webpage
  // DOM. Populated after a standalone cvc field is processed on a form. Used to
  // confirm that the virtual card last four is present in the webpage for card
  // on file case.
  std::vector<std::string> four_digit_combinations_in_dom_;

  std::u16string last_unlocked_credit_card_cvc_;

  base::WeakPtrFactory<BrowserAutofillManager> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_BROWSER_AUTOFILL_MANAGER_H_
